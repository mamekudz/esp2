/**
 * ESP][ PART B — display throughput + real-time scheduling (text port).
 *
 * Emulated 6502 cycles remain the Apple II timeline.
 * Display SPI must never advance machine time or stall the CPU hot path.
 */
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <Arduino_GFX_Library.h>

#include "board_pins.h"
#include "display_power.h"
#include "qmi8658_min.h"

#include "esp_bracket/apple2_bus.hpp"
#include "esp_bracket/cpu6502.hpp"
#include "esp_bracket/rom.hpp"
#include "esp_bracket/text_decoder.hpp"

using namespace esp_bracket;

static constexpr char kBuildId[] = "apple2_text_perf";
static constexpr uint32_t kBootRunCycles = 200000;
static constexpr uint32_t kAppleIiHz = 1023000;
static constexpr uint32_t kExecQuantum = 2000;    // cycles per scheduling checkpoint
static constexpr int kViewOffsetX = 20;           // (280 - 240) / 2
static constexpr int kViewOffsetY = 48;           // below rounded top
static constexpr int kViewW = TextDecoder::kRgbW; // 240
static constexpr int kViewH = TextDecoder::kRgbH; // 192
static constexpr uint32_t kQspiHz = 40000000;     // Arduino_ESP32QSPI default
static constexpr uint32_t kQspiLanes = 4;
static constexpr uint32_t kStabilityMs = 180000; // 3 minutes

static Arduino_DataBus *g_bus = nullptr;
static Arduino_CO5300 *g_gfx = nullptr;
static DisplayPowerManager g_display_power;
static constexpr uint8_t kDisplayBrightness = 180;

static Apple2Bus g_a2bus;
static Cpu6502 g_cpu;

static bool g_display_ok = false;
static bool g_touch_ok = false;
static bool g_sd_ok = false;
static bool g_imu_ok = false;

/** Published text snapshot (display never reads half-written). */
struct TextSnapshot {
    uint8_t chars[TextDecoder::kRows * TextDecoder::kCols];
    uint32_t generation;
};

static TextSnapshot g_snap[2];
static volatile uint32_t g_pubIndex = 0; // index of latest complete snapshot
static volatile uint32_t g_pubGeneration = 0;
static portMUX_TYPE g_snapMux = portMUX_INITIALIZER_UNLOCKED;

static volatile uint32_t g_dispFrames = 0;
static volatile uint32_t g_dispSkipped = 0;
static volatile uint32_t g_dispBytes = 0;
static volatile uint32_t g_lastTransferUs = 0;
static volatile uint32_t g_lastRenderUs = 0;
static volatile uint64_t g_emuCyclesAtBoot = 0;
static volatile uint32_t g_wallStartUs = 0;
static volatile bool g_tasksRunning = false;
static volatile bool g_schedulerGo = false;

static uint16_t *g_viewportFb = nullptr; // kViewW * kViewH RGB565 (PSRAM staging OK)
static uint16_t *g_stripFb = nullptr;    // kViewW * 8 DMA-friendly strip

static void logf(const char *tag, const char *fmt, ...) {
    char buf[220];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    Serial.printf("[%s] %s\n", tag, buf);
}

static uint32_t theoreticalUs(uint32_t bytes) {
    // Quad SPI payload bandwidth estimate (command/addr overhead ignored).
    const uint64_t bitsPerSec = static_cast<uint64_t>(kQspiHz) * kQspiLanes;
    if (bitsPerSec == 0) {
        return 0;
    }
    return static_cast<uint32_t>((static_cast<uint64_t>(bytes) * 8ULL * 1000000ULL) / bitsPerSec);
}

static void panel_sleep_co5300() {
    if (g_gfx) {
        g_gfx->displayOff();
    }
}

static void panel_wake_co5300() {
    if (g_gfx) {
        g_gfx->displayOn();
        g_gfx->setBrightness(kDisplayBrightness);
    }
}

static void draw_screensaver_stub(uint8_t /*slot*/) {
    if (!g_gfx || !g_display_ok) {
        return;
    }
    g_gfx->fillScreen(0x0000);
    g_gfx->drawPixel(20 + (millis() / 50) % 240, 40, 0x07E0);
}

static void restore_ui_stub() {
    // Display task redraws from latest snapshot when Active.
    portENTER_CRITICAL(&g_snapMux);
    g_pubGeneration = g_pubGeneration + 1; // force redraw after wake
    portEXIT_CRITICAL(&g_snapMux);
}

static bool init_display() {
    g_bus = new Arduino_ESP32QSPI(PIN_LCD_CS, PIN_LCD_SCK, PIN_LCD_D0, PIN_LCD_D1, PIN_LCD_D2,
                                  PIN_LCD_D3);
    g_gfx = new Arduino_CO5300(g_bus, PIN_LCD_RST, 0, LCD_WIDTH, LCD_HEIGHT, LCD_COL_OFFSET1,
                               LCD_ROW_OFFSET1, LCD_COL_OFFSET2, LCD_ROW_OFFSET2);
    if (!g_gfx->begin()) {
        logf("DISPLAY", "[FAIL] CO5300 begin");
        return false;
    }
    g_gfx->setBrightness(kDisplayBrightness);
    g_gfx->fillScreen(0x0000);
    logf("DISPLAY", "CO5300 init OK %dx%d", g_gfx->width(), g_gfx->height());
    logf("DISPLAY",
         "ESP32 PHYSICAL MEASURED qspi_hz=%u lanes=%u dma=yes max_px_chunk=1024 host=SPI2 "
         "blocking_poll=yes",
         kQspiHz, kQspiLanes);
    return true;
}

static bool probe_touch() {
    Wire.begin(PIN_TOUCH_SDA, PIN_TOUCH_SCL);
    Wire.beginTransmission(FT3168_I2C_ADDR);
    const uint8_t err = Wire.endTransmission();
    if (err != 0) {
        logf("TOUCH", "[FAIL] FT3168 nack err=%u", err);
        return false;
    }
    logf("TOUCH", "FT3168 OK");
    return true;
}

static bool probe_sd() {
    SPI.begin(PIN_SD_SCLK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
    if (!SD.begin(PIN_SD_CS)) {
        logf("SD", "[FAIL] begin");
        return false;
    }
    logf("SD", "mounted");
    return true;
}

static Qmi8658Min g_imu_probe;

static bool probe_imu() {
    if (!g_imu_probe.begin(Wire)) {
        logf("IMU", "[FAIL] QMI8658 not detected");
        return false;
    }
    logf("IMU", "QMI8658 OK who=0x%02X", g_imu_probe.whoAmI());
    return true;
}

static void renderCharsToViewport(const uint8_t *chars, uint16_t *fb) {
    const uint16_t bg = 0x0000;
    const uint16_t fg = 0xFFFF;
    for (int py = 0; py < kViewH; ++py) {
        uint16_t *row = fb + py * kViewW;
        for (int x = 0; x < kViewW; ++x) {
            row[x] = bg;
        }
        const int trow = py / TextDecoder::kCellH;
        const int gy = py % TextDecoder::kCellH;
        for (int col = 0; col < TextDecoder::kCols; ++col) {
            const uint8_t cell = chars[trow * TextDecoder::kCols + col];
            const uint8_t ascii7 = static_cast<uint8_t>(cell & 0x7F);
            const bool inverse = (cell & 0x80) == 0;
            uint8_t bits = 0;
            if (gy < TextDecoder::kGlyphH) {
                bits = TextDecoder::glyphRow(ascii7, gy);
            }
            for (int gx = 0; gx < TextDecoder::kGlyphW; ++gx) {
                const bool on = (bits & (1u << (4 - gx))) != 0;
                const bool lit = inverse ? !on : on;
                row[col * TextDecoder::kCellW + gx] = lit ? fg : bg;
            }
        }
    }
}

/** PART A baseline: full-width scanlines + fillScreen (for before metrics). */
static void renderBaselineScanlines(const uint8_t *ram, uint32_t *outDecodeUs, uint32_t *outGlyphUs,
                                    uint32_t *outXferUs, uint32_t *outFillUs) {
    uint8_t chars[TextDecoder::kRows * TextDecoder::kCols];
    uint32_t t0 = micros();
    TextDecoder::decodeScreen(ram, TextDecoder::kPage1Base, chars);
    *outDecodeUs = micros() - t0;

    t0 = micros();
    g_gfx->fillScreen(0x0000);
    *outFillUs = micros() - t0;

    static uint16_t line[LCD_WIDTH];
    const uint16_t bg = 0x0000;
    const uint16_t fg = 0xFFFF;
    uint32_t glyphUs = 0;
    uint32_t xferUs = 0;

    for (int py = 0; py < TextDecoder::kRgbH; ++py) {
        const uint32_t tg0 = micros();
        for (int x = 0; x < LCD_WIDTH; ++x) {
            line[x] = bg;
        }
        const int row = py / TextDecoder::kCellH;
        const int gy = py % TextDecoder::kCellH;
        for (int col = 0; col < TextDecoder::kCols; ++col) {
            const uint8_t cell = chars[row * TextDecoder::kCols + col];
            const uint8_t ascii7 = static_cast<uint8_t>(cell & 0x7F);
            const bool inverse = (cell & 0x80) == 0;
            uint8_t bits = 0;
            if (gy < TextDecoder::kGlyphH) {
                bits = TextDecoder::glyphRow(ascii7, gy);
            }
            for (int gx = 0; gx < TextDecoder::kGlyphW; ++gx) {
                const bool on = (bits & (1u << (4 - gx))) != 0;
                const bool lit = inverse ? !on : on;
                const int dx = kViewOffsetX + col * TextDecoder::kCellW + gx;
                if (dx >= 0 && dx < LCD_WIDTH) {
                    line[dx] = lit ? fg : bg;
                }
            }
        }
        glyphUs += micros() - tg0;

        const int dy = kViewOffsetY + py;
        const uint32_t tx0 = micros();
        if (dy >= 0 && dy < LCD_HEIGHT) {
            g_gfx->draw16bitRGBBitmap(0, dy, line, LCD_WIDTH, 1);
        }
        xferUs += micros() - tx0;
    }
    *outGlyphUs = glyphUs;
    *outXferUs = xferUs;
}

/** Optimized: one address window for Apple II viewport (240x192). */
static uint32_t transferViewportOnce(const uint8_t *chars) {
    if (!g_viewportFb || !g_gfx) {
        return 0;
    }
    const uint32_t t0 = micros();
    renderCharsToViewport(chars, g_viewportFb);
    const uint32_t tRender = micros();
    g_gfx->draw16bitRGBBitmap(kViewOffsetX, kViewOffsetY, g_viewportFb, kViewW, kViewH);
    const uint32_t t1 = micros();
    g_lastRenderUs = tRender - t0;
    g_lastTransferUs = t1 - tRender;
    g_dispBytes = static_cast<uint32_t>(kViewW * kViewH * 2);
    return t1 - t0;
}

/** Optimized: 24× (240×8) block transfers (one text row strip each). */
static uint32_t transferBlock8(const uint8_t *chars) {
    if (!g_stripFb || !g_gfx) {
        return 0;
    }
    const uint32_t t0 = micros();
    for (int trow = 0; trow < TextDecoder::kRows; ++trow) {
        for (int gy = 0; gy < TextDecoder::kCellH; ++gy) {
            uint16_t *row = g_stripFb + gy * kViewW;
            memset(row, 0, kViewW * sizeof(uint16_t));
            for (int col = 0; col < TextDecoder::kCols; ++col) {
                const uint8_t cell = chars[trow * TextDecoder::kCols + col];
                const uint8_t ascii7 = static_cast<uint8_t>(cell & 0x7F);
                const bool inverse = (cell & 0x80) == 0;
                uint8_t bits = 0;
                if (gy < TextDecoder::kGlyphH) {
                    bits = TextDecoder::glyphRow(ascii7, gy);
                }
                for (int gx = 0; gx < TextDecoder::kGlyphW; ++gx) {
                    const bool on = (bits & (1u << (4 - gx))) != 0;
                    const bool lit = inverse ? !on : on;
                    row[col * TextDecoder::kCellW + gx] = lit ? 0xFFFF : 0x0000;
                }
            }
        }
        const int y = kViewOffsetY + trow * TextDecoder::kCellH;
        g_gfx->draw16bitRGBBitmap(kViewOffsetX, y, g_stripFb, kViewW, TextDecoder::kCellH);
    }
    return micros() - t0;
}

static uint32_t transferFullPanelSolid(uint16_t color) {
    // Measure full-panel cost with one bitmap when possible.
    static uint16_t line[LCD_WIDTH];
    for (int x = 0; x < LCD_WIDTH; ++x) {
        line[x] = color;
    }
    const uint32_t t0 = micros();
    for (int y = 0; y < LCD_HEIGHT; ++y) {
        g_gfx->draw16bitRGBBitmap(0, y, line, LCD_WIDTH, 1);
    }
    return micros() - t0;
}

static void publishTextSnapshot(const uint8_t *ram) {
    const uint32_t next = (g_pubIndex + 1) & 1u;
    TextDecoder::decodeScreen(ram, TextDecoder::kPage1Base, g_snap[next].chars);
    portENTER_CRITICAL(&g_snapMux);
    g_snap[next].generation = g_pubGeneration + 1;
    g_pubGeneration = g_snap[next].generation;
    g_pubIndex = next;
    portEXIT_CRITICAL(&g_snapMux);
}

static bool verify_marker(const uint8_t *ram) {
    return ram[kEsp32TextPortMarkerAddr] == kEsp32TextPortMarker0 &&
           ram[kEsp32TextPortMarkerAddr + 1] == kEsp32TextPortMarker1 &&
           ram[kEsp32TextPortMarkerAddr + 2] == kEsp32TextPortMarker2 &&
           ram[kEsp32TextPortMarkerAddr + 3] == kEsp32TextPortMarker3;
}

static bool verify_text_content(const uint8_t *ram) {
    uint8_t chars[TextDecoder::kRows * TextDecoder::kCols];
    TextDecoder::decodeScreen(ram, TextDecoder::kPage1Base, chars);
    auto rowHas = [&](int row, const char *needle) -> bool {
        char line[41];
        for (int c = 0; c < 40; ++c) {
            line[c] = static_cast<char>(chars[row * 40 + c] & 0x7F);
        }
        line[40] = 0;
        return strstr(line, needle) != nullptr;
    };
    return rowHas(0, "ESP][") && rowHas(2, "ESP32-S3 PORT TEST") && rowHas(4, "6502") &&
           rowHas(7, "TEXT");
}

static void patch_cycle_digits(uint8_t *ram, uint32_t cycles) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%09lu", static_cast<unsigned long>(cycles));
    const int row = 9;
    const int startCol = 7;
    for (int i = 0; i < 9; ++i) {
        const uint16_t addr = TextDecoder::cellAddress(TextDecoder::kPage1Base, row, startCol + i);
        ram[addr] = static_cast<uint8_t>(0x80u | static_cast<uint8_t>(buf[i]));
    }
}

static void runBootBenchmarks(const uint8_t *ram) {
    uint8_t chars[TextDecoder::kRows * TextDecoder::kCols];
    TextDecoder::decodeScreen(ram, TextDecoder::kPage1Base, chars);

    logf("PERF", "ESP32 PHYSICAL MEASURED theoretical_us_280x192=%u bytes=%u",
         theoreticalUs(280 * 192 * 2), 280 * 192 * 2);
    logf("PERF", "ESP32 PHYSICAL MEASURED theoretical_us_240x192=%u bytes=%u",
         theoreticalUs(kViewW * kViewH * 2), kViewW * kViewH * 2);
    logf("PERF", "ESP32 PHYSICAL MEASURED theoretical_us_280x456=%u bytes=%u",
         theoreticalUs(LCD_WIDTH * LCD_HEIGHT * 2), LCD_WIDTH * LCD_HEIGHT * 2);

    uint32_t decodeUs = 0, glyphUs = 0, xferUs = 0, fillUs = 0;
    const uint32_t tBase0 = micros();
    renderBaselineScanlines(ram, &decodeUs, &glyphUs, &xferUs, &fillUs);
    const uint32_t baseTotal = micros() - tBase0;
    logf("PERF",
         "ESP32 PHYSICAL MEASURED before_breakdown decode_us=%u glyph_us=%u "
         "fill_us=%u scanline_xfer_us=%u total_us=%u area=%dx%d_x192_tx",
         decodeUs, glyphUs, fillUs, xferUs, baseTotal, LCD_WIDTH, 1);
    logf("PERF", "ESP32 PHYSICAL MEASURED before_display_transfer_us=%u", baseTotal);

    const uint32_t fullUs = transferFullPanelSolid(0x0000);
    logf("PERF", "ESP32 PHYSICAL MEASURED full_panel_280x456_scanline_us=%u", fullUs);

    g_gfx->fillScreen(0x0000);
    const uint32_t optUs = transferViewportOnce(chars);
    logf("PERF",
         "ESP32 PHYSICAL MEASURED after_viewport_240x192_us=%u render_us=%u "
         "xfer_us=%u bytes=%u",
         optUs, g_lastRenderUs, g_lastTransferUs, g_dispBytes);

    const uint32_t blkUs = transferBlock8(chars);
    logf("PERF", "ESP32 PHYSICAL MEASURED after_block8_24x_us=%u", blkUs);

    if (optUs > 0) {
        const double fps = 1000000.0 / static_cast<double>(optUs);
        logf("PERF", "ESP32 PHYSICAL MEASURED max_full_viewport_refresh_hz=%.1f", fps);
    }

    // Small-update: rewrite one text row strip only.
    const uint32_t tSmall0 = micros();
    if (g_stripFb) {
        memset(g_stripFb, 0, kViewW * 8 * sizeof(uint16_t));
        g_gfx->draw16bitRGBBitmap(kViewOffsetX, kViewOffsetY, g_stripFb, kViewW, 8);
    }
    const uint32_t smallUs = micros() - tSmall0;
    logf("PERF", "ESP32 PHYSICAL MEASURED small_update_one_row_us=%u bytes=%u", smallUs,
         kViewW * 8 * 2);
}

static void measureCpuThroughput(const char *label, bool displayBusy) {
    const uint32_t cycles = 200000;
    // Optionally keep display task busy via forced dirty publishes.
    const uint32_t t0 = micros();
    const uint32_t ran = g_cpu.runCycles(cycles);
    g_a2bus.setAccessCycle(static_cast<uint32_t>(g_cpu.cycles() & 0xFFFFFFFFu));
    const uint32_t dt = micros() - t0;
    const double cps =
        dt > 0 ? (static_cast<double>(ran) * 1000000.0 / static_cast<double>(dt)) : 0.0;
    logf("PERF", "ESP32 PHYSICAL MEASURED %s cpu_emu_cycles_per_s=%.0f display_busy=%d", label, cps,
         displayBusy ? 1 : 0);
}

static void emulatorTask(void * /*arg*/) {
    while (!g_schedulerGo) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    logf("SCHED", "emu task core=%d prio=%d quantum=%u", xPortGetCoreID(),
         uxTaskPriorityGet(nullptr), kExecQuantum);
    g_wallStartUs = micros();
    g_emuCyclesAtBoot = g_cpu.cycles();

    uint32_t lastPublishMs = 0;
    uint32_t lastDigitPatch = 0;
    uint8_t lastChars[TextDecoder::kRows * TextDecoder::kCols];
    bool haveLast = false;

    for (;;) {
        const uint64_t before = g_cpu.cycles();
        g_cpu.runCycles(kExecQuantum);
        g_a2bus.setAccessCycle(static_cast<uint32_t>(g_cpu.cycles() & 0xFFFFFFFFu));
        const uint64_t after = g_cpu.cycles();
        const uint64_t ran = after - before;

        // Real-time throttle vs Apple II Hz (chunked — not per instruction).
        const uint64_t emuSinceBoot = after - g_emuCyclesAtBoot;
        const uint64_t targetUs = (emuSinceBoot * 1000000ULL) / static_cast<uint64_t>(kAppleIiHz);
        const uint64_t wallUs = static_cast<uint64_t>(micros() - g_wallStartUs);
        if (targetUs > wallUs + 50) {
            const uint32_t sleepUs = static_cast<uint32_t>(targetUs - wallUs);
            if (sleepUs > 2000) {
                vTaskDelay(pdMS_TO_TICKS(sleepUs / 1000));
            } else {
                delayMicroseconds(sleepUs);
            }
        } else if (((after / kExecQuantum) & 0x0F) == 0) {
            // Behind wall clock: periodic yield for WDT / peers.
            taskYIELD();
        }

        // Patch cycle counter in text RAM infrequently (dirty source).
        const uint32_t nowMs = millis();
        if (nowMs - lastDigitPatch >= 250) {
            patch_cycle_digits(g_a2bus.ram(), static_cast<uint32_t>(after & 0xFFFFFFFFu));
            lastDigitPatch = nowMs;
        }

        // Cheap dirty detect on decoded text; publish only when changed.
        if (nowMs - lastPublishMs >= 16 || !haveLast) {
            uint8_t chars[TextDecoder::kRows * TextDecoder::kCols];
            TextDecoder::decodeScreen(g_a2bus.ram(), TextDecoder::kPage1Base, chars);
            if (!haveLast || memcmp(chars, lastChars, sizeof(chars)) != 0) {
                memcpy(lastChars, chars, sizeof(chars));
                haveLast = true;
                const uint32_t next = (g_pubIndex + 1) & 1u;
                memcpy(g_snap[next].chars, chars, sizeof(chars));
                portENTER_CRITICAL(&g_snapMux);
                g_snap[next].generation = g_pubGeneration + 1;
                g_pubGeneration = g_snap[next].generation;
                g_pubIndex = next;
                portEXIT_CRITICAL(&g_snapMux);
            }
            lastPublishMs = nowMs;
        }

        (void)ran;
    }
}

static void displayTask(void * /*arg*/) {
    while (!g_schedulerGo) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    logf("SCHED", "disp task core=%d prio=%d", xPortGetCoreID(), uxTaskPriorityGet(nullptr));
    uint32_t lastSeenGen = 0;
    uint8_t drawn[TextDecoder::kRows * TextDecoder::kCols];
    bool haveDrawn = false;
    uint32_t lastDiagMs = 0;
    const uint32_t startMs = millis();
    bool stabilityDone = false;
    uint32_t lastPowerState = 255;
    bool sawScreensaver = false;
    bool sawOff = false;

    for (;;) {
        // Housekeeping lives on core 0 so core-1 emu cannot starve diagnostics.
        const uint32_t now = millis();
        g_display_power.update(now);

        const auto st = g_display_power.state();
        if (static_cast<uint32_t>(st) != lastPowerState) {
            lastPowerState = static_cast<uint32_t>(st);
            if (st == DisplayPowerState::Screensaver) {
                sawScreensaver = true;
                logf("POWER", "state=SCREENSAVER (render≠activity)");
            } else if (st == DisplayPowerState::Off) {
                sawOff = true;
                logf("POWER", "state=OFF");
            } else if (st == DisplayPowerState::Active) {
                logf("POWER", "state=ACTIVE");
            }
        }

        if (g_touch_ok) {
            Wire.beginTransmission(FT3168_I2C_ADDR);
            Wire.write(0x02);
            if (Wire.endTransmission(false) == 0 &&
                Wire.requestFrom((int)FT3168_I2C_ADDR, 1) == 1) {
                const uint8_t td = Wire.read() & 0x0F;
                if (td > 0) {
                    g_display_power.notifyActivity("touch");
                }
            }
        }

        if (now - lastDiagMs >= 5000) {
            lastDiagMs = now;
            const uint64_t emu = g_cpu.cycles();
            const uint32_t wallUs = micros() - g_wallStartUs;
            const double cps = wallUs > 0 ? (static_cast<double>(emu - g_emuCyclesAtBoot) *
                                             1000000.0 / static_cast<double>(wallUs))
                                          : 0.0;
            logf("PERF",
                 "ESP32 PHYSICAL MEASURED live cps=%.0f frames=%u skipped=%u "
                 "xfer_us=%u heap=%u heap_min=%u",
                 cps, g_dispFrames, g_dispSkipped, g_lastTransferUs, ESP.getFreeHeap(),
                 ESP.getMinFreeHeap());
        }

        if (!stabilityDone && (now - startMs) >= kStabilityMs) {
            stabilityDone = true;
            logf("STABILITY", "duration_ms=%u frames=%u heap=%u heap_min=%u", kStabilityMs,
                 g_dispFrames, ESP.getFreeHeap(), ESP.getMinFreeHeap());
            logf("POWER", "screensaver_seen=%s off_seen=%s", sawScreensaver ? "yes" : "no",
                 sawOff ? "yes" : "no");
            logf("SELFTEST", "display=%s touch=%s sd=%s imu=%s", g_display_ok ? "PASS" : "FAIL",
                 g_touch_ok ? "PASS" : "FAIL", g_sd_ok ? "PASS" : "FAIL",
                 g_imu_ok ? "PASS" : "FAIL");
            logf("STABILITY", "result=PASS");
        }

        if (!g_display_ok || !g_display_power.isInteractive()) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        uint32_t idx = 0;
        uint32_t gen = 0;
        portENTER_CRITICAL(&g_snapMux);
        idx = g_pubIndex;
        gen = g_pubGeneration;
        portEXIT_CRITICAL(&g_snapMux);

        if (gen == lastSeenGen) {
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }

        if (gen > lastSeenGen + 1) {
            g_dispSkipped = g_dispSkipped + (gen - lastSeenGen - 1);
        }
        TextSnapshot local = g_snap[idx];
        lastSeenGen = gen;

        if (haveDrawn && memcmp(local.chars, drawn, sizeof(drawn)) == 0) {
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }

        const uint32_t us = transferViewportOnce(local.chars);
        memcpy(drawn, local.chars, sizeof(drawn));
        haveDrawn = true;
        g_dispFrames = g_dispFrames + 1;
        (void)us;
        // NOTE: framebuffer changes must NOT call notifyActivity.
    }
}

static void run_apple2_text_port() {
    logf("APPLE2", "cpu init");
    g_cpu.setCallbacks(&g_a2bus, Apple2Bus::busRead, Apple2Bus::busWrite);

    logf("APPLE2", "ram init");
    g_a2bus.clearRam();
    g_a2bus.reset();
    logf("APPLE2", "ram place=internal_sram_bss bytes=%u",
         static_cast<unsigned>(Apple2Bus::kRamBytes));

    logf("APPLE2", "rom synthetic esp32-text-port");
    static uint8_t romImg[Rom::kApple2PlusRomBytes];
    if (generateSyntheticEsp32TextPortRom(romImg, sizeof(romImg)) != RomError::Ok ||
        g_a2bus.rom().load(romImg, sizeof(romImg)) != RomError::Ok) {
        logf("APPLE2", "[FAIL] rom load");
        return;
    }

    g_cpu.reset();
    logf("APPLE2", "pc=%04X after reset", g_cpu.registers().pc);

    // Allocate staging buffers (PSRAM preferred for viewport FB; strip in DMA RAM).
    g_viewportFb = static_cast<uint16_t *>(
        heap_caps_malloc(kViewW * kViewH * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!g_viewportFb) {
        g_viewportFb = static_cast<uint16_t *>(
            heap_caps_malloc(kViewW * kViewH * sizeof(uint16_t), MALLOC_CAP_8BIT));
    }
    g_stripFb = static_cast<uint16_t *>(
        heap_caps_aligned_alloc(16, kViewW * 8 * sizeof(uint16_t), MALLOC_CAP_DMA));
    if (!g_stripFb) {
        g_stripFb = static_cast<uint16_t *>(malloc(kViewW * 8 * sizeof(uint16_t)));
    }
    logf("RAM", "viewport_fb=%s strip_fb=%s", g_viewportFb ? "ok" : "FAIL",
         g_stripFb ? "ok" : "FAIL");
    if (g_viewportFb) {
        logf("RAM", "viewport_fb_bytes=%u place=%s", kViewW * kViewH * 2,
             esp_ptr_external_ram(g_viewportFb) ? "psram" : "internal");
    }

    // Boot: raw CPU, display idle.
    measureCpuThroughput("display_idle", false);

    const uint32_t t0 = micros();
    g_a2bus.setAccessCycle(0);
    const uint32_t ran = g_cpu.runCycles(kBootRunCycles);
    g_a2bus.setAccessCycle(static_cast<uint32_t>(g_cpu.cycles() & 0xFFFFFFFFu));
    const uint32_t elapsedUs = micros() - t0;
    const double cps = elapsedUs > 0
                           ? (static_cast<double>(ran) * 1000000.0 / static_cast<double>(elapsedUs))
                           : 0.0;
    logf("APPLE2", "run cycles=%u elapsed_us=%u", ran, elapsedUs);
    logf("PERF", "ESP32 PHYSICAL MEASURED boot_cpu_emu_cycles_per_s=%.0f", cps);
    logf("PERF", "ESP32 PHYSICAL MEASURED realtime_headroom_x=%.2f", cps / 1023000.0);

    const bool markerOk = verify_marker(g_a2bus.ram());
    logf("APPLE2", "marker=%s", markerOk ? "PASS" : "FAIL");
    patch_cycle_digits(g_a2bus.ram(), ran);
    const bool textOk = verify_text_content(g_a2bus.ram());
    logf("APPLE2", "text memory=%s", textOk ? "PASS" : "FAIL");

    if (g_display_power.isInteractive()) {
        runBootBenchmarks(g_a2bus.ram());
    }

    publishTextSnapshot(g_a2bus.ram());
    logf("APPLE2", "text render=%s", (textOk && markerOk) ? "PASS" : "FAIL");

    logf("RAM", "heap_free=%u heap_min=%u", ESP.getFreeHeap(), ESP.getMinFreeHeap());
    if (ESP.getPsramSize() > 0) {
        logf("RAM", "psram_free=%u", ESP.getFreePsram());
    }

    // Display-active CPU measure: force transfers on this core briefly.
    if (g_display_ok && g_viewportFb) {
        uint8_t chars[TextDecoder::kRows * TextDecoder::kCols];
        TextDecoder::decodeScreen(g_a2bus.ram(), TextDecoder::kPage1Base, chars);
        const uint32_t tBusy0 = micros();
        uint32_t ranBusy = 0;
        while (micros() - tBusy0 < 200000) {
            ranBusy += g_cpu.runCycles(kExecQuantum);
            g_a2bus.setAccessCycle(static_cast<uint32_t>(g_cpu.cycles() & 0xFFFFFFFFu));
            transferViewportOnce(chars);
        }
        const uint32_t dtBusy = micros() - tBusy0;
        const double cpsBusy =
            dtBusy > 0 ? (static_cast<double>(ranBusy) * 1000000.0 / static_cast<double>(dtBusy))
                       : 0.0;
        logf("PERF",
             "ESP32 PHYSICAL MEASURED display_active_blocking "
             "cpu_emu_cycles_per_s=%.0f",
             cpsBusy);
    }
}

void setup() {
    Serial.begin(115200);
    delay(300);
    logf("ESP2", "build=%s", kBuildId);
    logf("ESP2", "psram=%u", ESP.getPsramSize());
    logf("ESP2", "heap=%u", ESP.getFreeHeap());

    g_display_ok = init_display();
    g_display_power.begin(draw_screensaver_stub, restore_ui_stub, panel_sleep_co5300,
                          panel_wake_co5300);
    DisplayPowerSettings dps{};
    // Shorter timeouts so PART B can observe SCREENSAVER/OFF without waiting forever.
    dps.screensaver = ScreensaverTimeout::Min1;
    dps.screen_off = ScreenOffTimeout::Min2;
    // Lab overrides: 45s screensaver, 90s off — still proves rendering ≠ activity.
    dps.screensaver_override_ms = 45000;
    dps.screen_off_override_ms = 90000;
    g_display_power.setSettings(dps);
    g_display_power.notifyActivity("boot");

    g_touch_ok = probe_touch();
    g_sd_ok = probe_sd();
    g_imu_ok = probe_imu();

    run_apple2_text_port();

    logf("SELFTEST", "display=%s touch=%s sd=%s imu=%s", g_display_ok ? "PASS" : "FAIL",
         g_touch_ok ? "PASS" : "FAIL", g_sd_ok ? "PASS" : "FAIL", g_imu_ok ? "PASS" : "FAIL");

    // Create display first (core 0), then emulator (core 1). Both wait for go-flag
    // so setup/loop on core 1 is not preempted before initialization finishes.
    // Same priority as Arduino loop; throttle delays yield time. Display on core 0.
    xTaskCreatePinnedToCore(displayTask, "a2disp", 8192, nullptr, 2, nullptr, 0);
    xTaskCreatePinnedToCore(emulatorTask, "a2emu", 8192, nullptr, 1, nullptr, 1);
    g_tasksRunning = true;
    g_schedulerGo = true;
    logf("SCHED", "tasks started quantum=%u throttle_hz=%u", kExecQuantum, kAppleIiHz);
}

void loop() {
    // Power / touch / stability diagnostics run in displayTask (core 0).
    delay(100);
}
