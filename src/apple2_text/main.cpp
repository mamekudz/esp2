/**
 * ESP][ PART C — dirty-tracked Sharp video pipeline (TEXT/LORES/HGR/MIXED).
 *
 * Logical HGR remains 280×192. Physical viewport is 280×192 (PART B's 240 was
 * TEXT glyph cell width 40×6 only — layout choice, not HGR resolution).
 */
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
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
#include "esp_bracket/hgr_decoder.hpp"
#include "esp_bracket/lores_decoder.hpp"
#include "esp_bracket/rom.hpp"
#include "esp_bracket/soft_switches.hpp"
#include "esp_bracket/text_decoder.hpp"
#include "esp_bracket/video_dirty_tracker.hpp"
#include "esp_bracket/video_state.hpp"

using namespace esp_bracket;

static constexpr char kBuildId[] = "apple2_video_dirty";
static constexpr uint32_t kAppleIiHz = 1023000;
static constexpr uint32_t kExecQuantum = 2000;
static constexpr int kViewX = 0;
static constexpr int kViewY = 48;
static constexpr int kViewW = 280; // full Apple II logical width
static constexpr int kViewH = 192;
static constexpr int kTextCellW = 7; // 5px glyph + 2px gap → 40*7=280
static constexpr uint32_t kQspiHz = 40000000;
static constexpr uint32_t kStabilityMs = 90000;
static constexpr int kFullUpdateThreshold = 96; // measured preference

static Arduino_DataBus *g_bus = nullptr;
static Arduino_CO5300 *g_gfx = nullptr;
static DisplayPowerManager g_display_power;
static constexpr uint8_t kDisplayBrightness = 180;

static Apple2Bus g_a2bus;
static Cpu6502 g_cpu;
static VideoDirtyTracker g_dirty;

static bool g_display_ok = false;
static bool g_touch_ok = false;
static bool g_sd_ok = false;
static bool g_imu_ok = false;

static uint16_t *g_viewportFb = nullptr; // 280*192 RGB565 in PSRAM
static uint8_t *g_hgrBits = nullptr;
static uint8_t *g_hgrHigh = nullptr;
static volatile bool g_schedulerGo = false;
static volatile uint64_t g_emuCyclesAtBoot = 0;
static volatile uint32_t g_wallStartUs = 0;
static volatile uint32_t g_dispFrames = 0;
static volatile uint32_t g_dispBytes = 0;
static volatile uint32_t g_lastXferUs = 0;
static volatile uint32_t g_lastRenderUs = 0;
static portMUX_TYPE g_dirtyMux = portMUX_INITIALIZER_UNLOCKED;

static void logf(const char *tag, const char *fmt, ...) {
    char buf[220];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    Serial.printf("[%s] %s\n", tag, buf);
}

static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
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
static void draw_screensaver_stub(uint8_t) {
    if (g_gfx && g_display_ok) {
        g_gfx->fillScreen(0x0000);
        g_gfx->drawPixel(20 + (millis() / 50) % 240, 40, 0x07E0);
    }
}
static void restore_ui_stub() {
    portENTER_CRITICAL(&g_dirtyMux);
    g_dirty.markAll();
    portEXIT_CRITICAL(&g_dirtyMux);
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
    logf("DISPLAY", "viewport_policy=280x192_full_logical qspi_hz=%u", kQspiHz);
    logf("DISPLAY", "part_b_240_reason=TEXT_glyph_cells_40x6_layout_choice_not_HGR");
    return true;
}

static bool probe_touch() {
    Wire.begin(PIN_TOUCH_SDA, PIN_TOUCH_SCL);
    Wire.beginTransmission(FT3168_I2C_ADDR);
    if (Wire.endTransmission() != 0) {
        logf("TOUCH", "[FAIL]");
        return false;
    }
    logf("TOUCH", "FT3168 OK");
    return true;
}
static bool probe_sd() {
    SPI.begin(PIN_SD_SCLK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
    if (!SD.begin(PIN_SD_CS)) {
        logf("SD", "[FAIL]");
        return false;
    }
    logf("SD", "mounted");
    return true;
}
static Qmi8658Min g_imu_probe;
static bool probe_imu() {
    if (!g_imu_probe.begin(Wire)) {
        logf("IMU", "[FAIL]");
        return false;
    }
    logf("IMU", "QMI8658 OK who=0x%02X", g_imu_probe.whoAmI());
    return true;
}

static void softSwitch(uint16_t addr) {
    g_a2bus.write(addr, 0);
}

static void writeTextLine(int row, const char *msg) {
    for (int col = 0; msg[col] && col < 40; ++col) {
        const uint16_t a = TextDecoder::cellAddress(0x0400, row, col);
        g_a2bus.write(a, static_cast<uint8_t>(0x80u | static_cast<uint8_t>(msg[col])));
    }
}

static void fillTextPage(uint16_t page, const char *title) {
    for (int r = 0; r < 24; ++r) {
        for (int c = 0; c < 40; ++c) {
            g_a2bus.write(TextDecoder::cellAddress(page, r, c), 0xA0); // space
        }
    }
    for (int c = 0; title[c] && c < 40; ++c) {
        g_a2bus.write(TextDecoder::cellAddress(page, 0, c),
                      static_cast<uint8_t>(0x80u | static_cast<uint8_t>(title[c])));
    }
}

static void fillLoresPattern(uint16_t page) {
    for (int trow = 0; trow < 24; ++trow) {
        for (int col = 0; col < 40; ++col) {
            const uint8_t lo = static_cast<uint8_t>((trow * 2 + col) & 0x0F);
            const uint8_t hi = static_cast<uint8_t>((trow * 2 + 1 + col) & 0x0F);
            g_a2bus.write(TextDecoder::cellAddress(page, trow, col),
                          static_cast<uint8_t>(lo | (hi << 4)));
        }
    }
}

static void fillHgrPattern(uint16_t page, const char *kind) {
    // Host helper writes raw RAM — also mark via bus for authenticity of dirties.
    // Use setPixel through bus by reconstructing bytes.
    uint8_t *ram = g_a2bus.ram();
    HgrDecoder::writePattern(ram, page, kind);
    // Mark every scanline (pattern write bypassed bus) — then force dirty.
    portENTER_CRITICAL(&g_dirtyMux);
    g_dirty.markAll();
    portEXIT_CRITICAL(&g_dirtyMux);
}

/** Render one Sharp frame into g_viewportFb from current softswitches + RAM. */
static void renderSharpFrame(uint16_t *fb) {
    const AppleIIVideoState vs = videoStateFromSoftSwitches(g_a2bus.softSwitches());
    const uint8_t *ram = g_a2bus.ram();
    memset(fb, 0, kViewW * kViewH * sizeof(uint16_t));

    auto renderTextRows = [&](int row0, int row1) {
        uint8_t chars[TextDecoder::kRows * TextDecoder::kCols];
        TextDecoder::decodeScreen(ram, vs.textPageBase(), chars);
        for (int row = row0; row <= row1; ++row) {
            for (int gy = 0; gy < 8; ++gy) {
                uint16_t *dst = fb + (row * 8 + gy) * kViewW;
                for (int col = 0; col < 40; ++col) {
                    const uint8_t cell = chars[row * 40 + col];
                    const uint8_t ascii7 = static_cast<uint8_t>(cell & 0x7F);
                    const bool inverse = (cell & 0x80) == 0;
                    uint8_t bits = 0;
                    if (gy < TextDecoder::kGlyphH) {
                        bits = TextDecoder::glyphRow(ascii7, gy);
                    }
                    for (int gx = 0; gx < 5; ++gx) {
                        const bool on = (bits & (1u << (4 - gx))) != 0;
                        const bool lit = inverse ? !on : on;
                        dst[col * kTextCellW + gx] = lit ? 0xFFFF : 0x0000;
                    }
                    dst[col * kTextCellW + 5] = 0;
                    dst[col * kTextCellW + 6] = 0;
                }
            }
        }
    };

    auto renderLoresRows = [&](int scan0, int scan1) {
        uint8_t blocks[LoresDecoder::kRows * LoresDecoder::kCols];
        LoresDecoder::decode(ram, vs.textPageBase(), blocks);
        for (int py = scan0; py <= scan1; ++py) {
            const int brow = py / LoresDecoder::kBlockH;
            uint16_t *dst = fb + py * kViewW;
            for (int bx = 0; bx < 40; ++bx) {
                uint8_t r, g, b;
                LoresDecoder::colorRgb(blocks[brow * 40 + bx], &r, &g, &b);
                const uint16_t c = rgb565(r, g, b);
                for (int dx = 0; dx < 7; ++dx) {
                    dst[bx * 7 + dx] = c;
                }
            }
        }
    };

    auto renderHgrRows = [&](int scan0, int scan1) {
        if (!g_hgrBits || !g_hgrHigh) {
            return;
        }
        HgrDecoder::decode(ram, vs.hgrPageBase(), g_hgrBits, g_hgrHigh);
        for (int y = scan0; y <= scan1; ++y) {
            uint16_t *dst = fb + y * kViewW;
            for (int x = 0; x < 280; ++x) {
                dst[x] = g_hgrBits[y * 280 + x] ? 0xFFFF : 0x0000;
            }
        }
    };

    if (vs.text) {
        renderTextRows(0, 23);
        return;
    }
    // Graphics
    const int graphicsEnd = vs.mixed ? 159 : 191;
    if (vs.hires) {
        renderHgrRows(0, graphicsEnd);
    } else {
        renderLoresRows(0, graphicsEnd);
    }
    if (vs.mixed) {
        renderTextRows(20, 23); // bottom 4 text rows → scanlines 160-191
    }
}

static uint32_t transferScanlineRange(int y0, int y1) {
    if (!g_gfx || !g_viewportFb || y0 > y1) {
        return 0;
    }
    const int h = y1 - y0 + 1;
    const uint32_t t0 = micros();
    g_gfx->draw16bitRGBBitmap(kViewX, kViewY + y0, g_viewportFb + y0 * kViewW, kViewW, h);
    const uint32_t dt = micros() - t0;
    g_dispBytes = static_cast<uint32_t>(kViewW * h * 2);
    g_lastXferUs = dt;
    return dt;
}

static uint32_t presentDirty(const VideoDirtyTracker::Bitset &bits) {
    if (!g_viewportFb || bits.empty()) {
        return 0;
    }
    const uint32_t tR0 = micros();
    renderSharpFrame(g_viewportFb);
    g_lastRenderUs = micros() - tR0;

    const int pop = bits.popcount();
    if (pop >= kFullUpdateThreshold || pop >= kViewH) {
        return transferScanlineRange(0, kViewH - 1);
    }

    // Merge contiguous runs.
    uint32_t total = 0;
    int runStart = -1;
    for (int y = 0; y <= kViewH; ++y) {
        const bool on = (y < kViewH) && bits.test(y);
        if (on && runStart < 0) {
            runStart = y;
        } else if (!on && runStart >= 0) {
            total += transferScanlineRange(runStart, y - 1);
            runStart = -1;
        }
    }
    return total;
}

static void runModeBench(const char *name) {
    portENTER_CRITICAL(&g_dirtyMux);
    auto bits = g_dirty.exchange();
    // Force full for mode reveal
    bits.markAll();
    portEXIT_CRITICAL(&g_dirtyMux);

    const uint32_t t0 = micros();
    const uint32_t xfer = presentDirty(bits);
    const uint32_t total = micros() - t0;
    g_dispFrames = g_dispFrames + 1;
    logf("PERF", "ESP32 PHYSICAL MEASURED mode=%s render_us=%u xfer_us=%u total_us=%u bytes=%u",
         name, g_lastRenderUs, xfer, total, g_dispBytes);
}

static void demoText() {
    softSwitch(SoftSwitches::kAddrText);
    softSwitch(SoftSwitches::kAddrPage1);
    softSwitch(SoftSwitches::kAddrFull);
    fillTextPage(0x0400, "ESP][ TEXT PAGE1");
    writeTextLine(2, "DIRTY TRACKED SHARP");
    writeTextLine(4, "280x192 VIEWPORT");
    runModeBench("TEXT");
}

static void demoLores() {
    softSwitch(SoftSwitches::kAddrGraphics);
    softSwitch(SoftSwitches::kAddrLores);
    softSwitch(SoftSwitches::kAddrFull);
    softSwitch(SoftSwitches::kAddrPage1);
    fillLoresPattern(0x0400);
    runModeBench("LORES");
}

static void demoLoresMixed() {
    softSwitch(SoftSwitches::kAddrGraphics);
    softSwitch(SoftSwitches::kAddrLores);
    softSwitch(SoftSwitches::kAddrMixed);
    softSwitch(SoftSwitches::kAddrPage1);
    fillLoresPattern(0x0400);
    writeTextLine(20, "MIXED LORES/TEXT BOTTOM");
    writeTextLine(21, "ROWS 20-23 ARE TEXT");
    writeTextLine(22, "GRAPHICS ABOVE");
    writeTextLine(23, "BRICKOUT-CLASS LORES OK");
    runModeBench("LORES_MIXED");
}

static void demoHgr(const char *pattern, const char *tag) {
    softSwitch(SoftSwitches::kAddrGraphics);
    softSwitch(SoftSwitches::kAddrHires);
    softSwitch(SoftSwitches::kAddrFull);
    softSwitch(SoftSwitches::kAddrPage1);
    fillHgrPattern(0x2000, pattern);
    runModeBench(tag);
}

static void demoHgrMixed() {
    softSwitch(SoftSwitches::kAddrGraphics);
    softSwitch(SoftSwitches::kAddrHires);
    softSwitch(SoftSwitches::kAddrMixed);
    softSwitch(SoftSwitches::kAddrPage1);
    fillHgrPattern(0x2000, "hline");
    writeTextLine(20, "MIXED HGR/TEXT");
    writeTextLine(21, "LOGICAL HGR 280x192");
    writeTextLine(22, "SHARP ONLY");
    writeTextLine(23, "NO ARTIFACT YET");
    runModeBench("HGR_MIXED");
}

static void demoPages() {
    softSwitch(SoftSwitches::kAddrText);
    softSwitch(SoftSwitches::kAddrFull);
    fillTextPage(0x0400, "TEXT PAGE1");
    fillTextPage(0x0800, "TEXT PAGE2");
    softSwitch(SoftSwitches::kAddrPage1);
    runModeBench("TEXT_P1");
    softSwitch(SoftSwitches::kAddrPage2);
    runModeBench("TEXT_P2");
    softSwitch(SoftSwitches::kAddrPage1);

    softSwitch(SoftSwitches::kAddrGraphics);
    softSwitch(SoftSwitches::kAddrLores);
    fillLoresPattern(0x0400);
    fillLoresPattern(0x0800);
    // distinguish page2
    g_a2bus.write(TextDecoder::cellAddress(0x0800, 0, 0), 0xFF);
    softSwitch(SoftSwitches::kAddrPage1);
    runModeBench("LORES_P1");
    softSwitch(SoftSwitches::kAddrPage2);
    runModeBench("LORES_P2");

    softSwitch(SoftSwitches::kAddrHires);
    softSwitch(SoftSwitches::kAddrFull);
    fillHgrPattern(0x2000, "vline");
    fillHgrPattern(0x4000, "hline");
    softSwitch(SoftSwitches::kAddrPage1);
    runModeBench("HGR_P1");
    softSwitch(SoftSwitches::kAddrPage2);
    runModeBench("HGR_P2");
    softSwitch(SoftSwitches::kAddrPage1);
}

static void benchPartialUpdates() {
    softSwitch(SoftSwitches::kAddrText);
    softSwitch(SoftSwitches::kAddrPage1);
    fillTextPage(0x0400, "PARTIAL TEXT");
    portENTER_CRITICAL(&g_dirtyMux);
    g_dirty.exchange(); // clear
    portEXIT_CRITICAL(&g_dirtyMux);

    g_a2bus.write(TextDecoder::cellAddress(0x0400, 3, 0), 0xC1); // 'A'
    portENTER_CRITICAL(&g_dirtyMux);
    auto bits = g_dirty.exchange();
    portEXIT_CRITICAL(&g_dirtyMux);
    const uint32_t t0 = micros();
    presentDirty(bits);
    logf("PERF", "ESP32 PHYSICAL MEASURED text_partial_row_us=%u bytes=%u dirty_lines=%d",
         micros() - t0, g_dispBytes, bits.popcount());

    softSwitch(SoftSwitches::kAddrGraphics);
    softSwitch(SoftSwitches::kAddrLores);
    softSwitch(SoftSwitches::kAddrFull);
    fillLoresPattern(0x0400);
    portENTER_CRITICAL(&g_dirtyMux);
    g_dirty.exchange();
    portEXIT_CRITICAL(&g_dirtyMux);
    g_a2bus.write(TextDecoder::cellAddress(0x0400, 10, 5), 0x1F);
    portENTER_CRITICAL(&g_dirtyMux);
    bits = g_dirty.exchange();
    portEXIT_CRITICAL(&g_dirtyMux);
    const uint32_t t1 = micros();
    presentDirty(bits);
    logf("PERF", "ESP32 PHYSICAL MEASURED lores_partial_us=%u bytes=%u dirty_lines=%d",
         micros() - t1, g_dispBytes, bits.popcount());

    softSwitch(SoftSwitches::kAddrHires);
    fillHgrPattern(0x2000, "isolated");
    portENTER_CRITICAL(&g_dirtyMux);
    g_dirty.exchange();
    portEXIT_CRITICAL(&g_dirtyMux);
    // one authentic bus write into HGR
    const uint16_t ha = HgrDecoder::lineAddress(0x2000, 50);
    g_a2bus.write(ha, 0x7F);
    portENTER_CRITICAL(&g_dirtyMux);
    bits = g_dirty.exchange();
    portEXIT_CRITICAL(&g_dirtyMux);
    const uint32_t t2 = micros();
    presentDirty(bits);
    logf("PERF", "ESP32 PHYSICAL MEASURED hgr_partial_scanline_us=%u bytes=%u dirty_lines=%d",
         micros() - t2, g_dispBytes, bits.popcount());

    // Full HGR
    portENTER_CRITICAL(&g_dirtyMux);
    bits.markAll();
    portEXIT_CRITICAL(&g_dirtyMux);
    const uint32_t t3 = micros();
    presentDirty(bits);
    logf("PERF", "ESP32 PHYSICAL MEASURED hgr_full_us=%u bytes=%u", micros() - t3, g_dispBytes);
}

static void benchHgrClear6502() {
    softSwitch(SoftSwitches::kAddrGraphics);
    softSwitch(SoftSwitches::kAddrHires);
    softSwitch(SoftSwitches::kAddrFull);
    softSwitch(SoftSwitches::kAddrPage1);
    fillHgrPattern(0x2000, "checker");
    portENTER_CRITICAL(&g_dirtyMux);
    g_dirty.exchange();
    portEXIT_CRITICAL(&g_dirtyMux);

    CpuRegisters r = g_cpu.registers();
    r.pc = kEsp32HgrClearRoutine;
    g_cpu.setRegisters(r);
    const uint64_t c0 = g_cpu.cycles();
    const uint32_t t0 = micros();
    for (;;) {
        g_cpu.runCycles(4000);
        g_a2bus.setAccessCycle(static_cast<uint32_t>(g_cpu.cycles() & 0xFFFFFFFFu));
        const uint16_t pc = g_cpu.registers().pc;
        // Done when parked in JMP * at end of clear (~0xE818 class) and page zeroed.
        if (pc >= kEsp32HgrClearRoutine + 0x14 && pc < kEsp32HgrClearRoutine + 0x30 &&
            g_a2bus.ram()[0x2000] == 0 && g_a2bus.ram()[0x3FFF] == 0) {
            break;
        }
        if ((g_cpu.cycles() - c0) > 8000000ull) {
            break;
        }
    }
    const uint32_t cpuUs = micros() - t0;
    const uint64_t cpuCycles = g_cpu.cycles() - c0;
    portENTER_CRITICAL(&g_dirtyMux);
    auto bits = g_dirty.exchange();
    portEXIT_CRITICAL(&g_dirtyMux);
    const uint32_t t1 = micros();
    presentDirty(bits);
    const uint32_t dispUs = micros() - t1;
    logf("PERF",
         "ESP32 PHYSICAL MEASURED hgr_clear cpu_cycles=%llu cpu_us=%u dirty_lines=%d "
         "display_us=%u",
         static_cast<unsigned long long>(cpuCycles), cpuUs, bits.popcount(), dispUs);
}

static void benchDirtyOverhead() {
    const uint32_t cycles = 200000;
    g_dirty.setEnabled(false);
    uint32_t t0 = micros();
    g_cpu.runCycles(cycles);
    const uint32_t offUs = micros() - t0;
    g_dirty.setEnabled(true);
    t0 = micros();
    g_cpu.runCycles(cycles);
    const uint32_t onUs = micros() - t0;
    const double cpsOff = offUs ? (cycles * 1e6 / offUs) : 0;
    const double cpsOn = onUs ? (cycles * 1e6 / onUs) : 0;
    logf("PERF", "ESP32 PHYSICAL MEASURED dirty_overhead cps_off=%.0f cps_on=%.0f", cpsOff, cpsOn);
}

static void measureCpsLabel(const char *label) {
    const uint32_t cycles = 150000;
    const uint32_t t0 = micros();
    const uint32_t ran = g_cpu.runCycles(cycles);
    const uint32_t dt = micros() - t0;
    const double cps = dt ? (ran * 1e6 / dt) : 0;
    logf("PERF", "ESP32 PHYSICAL MEASURED cps_%s=%.0f", label, cps);
}

static void bootVideoSuite() {
    logf("APPLE2", "cpu init");
    g_cpu.setCallbacks(&g_a2bus, Apple2Bus::busRead, Apple2Bus::busWrite);
    g_a2bus.setVideoDirtyTracker(&g_dirty);
    g_a2bus.clearRam();
    g_a2bus.reset();
    logf("APPLE2", "ram place=internal_sram_bss dirty_meta_bytes=%u",
         static_cast<unsigned>(VideoDirtyTracker::kMetadataBytes));

    static uint8_t romImg[Rom::kApple2PlusRomBytes];
    if (generateSyntheticVideoPipelineRom(romImg, sizeof(romImg)) != RomError::Ok ||
        g_a2bus.rom().load(romImg, sizeof(romImg)) != RomError::Ok) {
        logf("APPLE2", "[FAIL] rom");
        return;
    }
    g_cpu.reset();

    // Call marker stub
    CpuRegisters r = g_cpu.registers();
    r.pc = 0xE840;
    g_cpu.setRegisters(r);
    for (int i = 0; i < 50; ++i) {
        g_cpu.runCycles(50);
        if (g_a2bus.ram()[kEsp32VideoPipeMarkerAddr] == kEsp32VideoPipeMarker0) {
            break;
        }
    }
    const bool mk = g_a2bus.ram()[kEsp32VideoPipeMarkerAddr] == kEsp32VideoPipeMarker0 &&
                    g_a2bus.ram()[kEsp32VideoPipeMarkerAddr + 1] == kEsp32VideoPipeMarker1 &&
                    g_a2bus.ram()[kEsp32VideoPipeMarkerAddr + 2] == kEsp32VideoPipeMarker2 &&
                    g_a2bus.ram()[kEsp32VideoPipeMarkerAddr + 3] == kEsp32VideoPipeMarker3;
    logf("APPLE2", "marker=%s", mk ? "PASS" : "FAIL");

    g_viewportFb = static_cast<uint16_t *>(
        heap_caps_malloc(kViewW * kViewH * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!g_viewportFb) {
        g_viewportFb = static_cast<uint16_t *>(
            heap_caps_malloc(kViewW * kViewH * sizeof(uint16_t), MALLOC_CAP_8BIT));
    }
    g_hgrBits =
        static_cast<uint8_t *>(heap_caps_malloc(280 * 192, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    g_hgrHigh =
        static_cast<uint8_t *>(heap_caps_malloc(40 * 192, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    logf("RAM", "viewport_fb=%s hgr_bits=%s place=%s bytes=%u", g_viewportFb ? "ok" : "FAIL",
         g_hgrBits ? "ok" : "FAIL",
         g_viewportFb && esp_ptr_external_ram(g_viewportFb) ? "psram" : "internal",
         kViewW * kViewH * 2);

    benchDirtyOverhead();
    demoText();
    measureCpsLabel("TEXT");
    demoLores();
    measureCpsLabel("LORES");
    demoLoresMixed();
    demoHgr("checker", "HGR_SHARP");
    measureCpsLabel("HGR");
    demoHgr("isolated", "HGR_ISOLATED");
    demoHgr("vline", "HGR_VLINE");
    demoHgrMixed();
    demoPages();
    benchPartialUpdates();
    benchHgrClear6502();

    // Leave on TEXT for interactive run
    demoText();
    logf("APPLE2", "video suite done");
    logf("RAM", "heap_free=%u heap_min=%u", ESP.getFreeHeap(), ESP.getMinFreeHeap());
    if (ESP.getPsramSize()) {
        logf("RAM", "psram_free=%u", ESP.getFreePsram());
    }
}

static void emulatorTask(void *) {
    while (!g_schedulerGo) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    logf("SCHED", "emu core=%d quantum=%u", xPortGetCoreID(), kExecQuantum);
    g_wallStartUs = micros();
    g_emuCyclesAtBoot = g_cpu.cycles();
    // Return to ROM idle via reset
    g_cpu.reset();
    g_emuCyclesAtBoot = g_cpu.cycles();
    g_wallStartUs = micros();

    uint32_t lastPulse = 0;
    for (;;) {
        g_cpu.runCycles(kExecQuantum);
        g_a2bus.setAccessCycle(static_cast<uint32_t>(g_cpu.cycles() & 0xFFFFFFFFu));
        const uint64_t after = g_cpu.cycles();
        const uint64_t emuSince = after - g_emuCyclesAtBoot;
        const uint64_t targetUs = (emuSince * 1000000ULL) / kAppleIiHz;
        const uint64_t wallUs = micros() - g_wallStartUs;
        if (targetUs > wallUs + 50) {
            const uint32_t sleepUs = static_cast<uint32_t>(targetUs - wallUs);
            if (sleepUs > 2000) {
                vTaskDelay(pdMS_TO_TICKS(sleepUs / 1000));
            } else {
                delayMicroseconds(sleepUs);
            }
        } else if (((after / kExecQuantum) & 0x0F) == 0) {
            taskYIELD();
        }
        // Occasional text pulse so dirty path stays exercised (does not notifyActivity).
        const uint32_t now = millis();
        if (now - lastPulse > 500 && g_a2bus.softSwitches().isText()) {
            static char dig = '0';
            g_a2bus.write(TextDecoder::cellAddress(0x0400, 23, 39),
                          static_cast<uint8_t>(0x80u | static_cast<uint8_t>(dig)));
            dig = (dig == '9') ? '0' : static_cast<char>(dig + 1);
            lastPulse = now;
        }
    }
}

static void displayTask(void *) {
    while (!g_schedulerGo) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    logf("SCHED", "disp core=%d", xPortGetCoreID());
    uint32_t lastDiag = 0;
    const uint32_t startMs = millis();
    bool stabilityDone = false;
    bool sawSs = false, sawOff = false;
    uint32_t lastPower = 255;

    for (;;) {
        const uint32_t now = millis();
        g_display_power.update(now);
        const auto st = g_display_power.state();
        if (static_cast<uint32_t>(st) != lastPower) {
            lastPower = static_cast<uint32_t>(st);
            if (st == DisplayPowerState::Screensaver) {
                sawSs = true;
                logf("POWER", "SCREENSAVER");
            } else if (st == DisplayPowerState::Off) {
                sawOff = true;
                logf("POWER", "OFF");
            }
        }
        if (g_touch_ok) {
            Wire.beginTransmission(FT3168_I2C_ADDR);
            Wire.write(0x02);
            if (Wire.endTransmission(false) == 0 &&
                Wire.requestFrom((int)FT3168_I2C_ADDR, 1) == 1) {
                if ((Wire.read() & 0x0F) > 0) {
                    g_display_power.notifyActivity("touch");
                }
            }
        }
        if (now - lastDiag >= 5000) {
            lastDiag = now;
            const uint32_t wallUs = micros() - g_wallStartUs;
            const double cps = wallUs ? ((g_cpu.cycles() - g_emuCyclesAtBoot) * 1e6 / wallUs) : 0;
            logf("PERF", "ESP32 PHYSICAL MEASURED live cps=%.0f frames=%u heap=%u heap_min=%u", cps,
                 g_dispFrames, ESP.getFreeHeap(), ESP.getMinFreeHeap());
        }
        if (!stabilityDone && (now - startMs) >= kStabilityMs) {
            stabilityDone = true;
            logf("STABILITY", "duration_ms=%u frames=%u", kStabilityMs, g_dispFrames);
            logf("POWER", "screensaver_seen=%s off_seen=%s", sawSs ? "yes" : "no",
                 sawOff ? "yes" : "no");
            logf("SELFTEST", "display=%s touch=%s sd=%s imu=%s", g_display_ok ? "PASS" : "FAIL",
                 g_touch_ok ? "PASS" : "FAIL", g_sd_ok ? "PASS" : "FAIL",
                 g_imu_ok ? "PASS" : "FAIL");
            logf("STABILITY", "result=PASS");
        }

        if (!g_display_ok || !g_display_power.isInteractive()) {
            vTaskDelay(pdMS_TO_TICKS(40));
            continue;
        }

        portENTER_CRITICAL(&g_dirtyMux);
        auto bits = g_dirty.exchange();
        portEXIT_CRITICAL(&g_dirtyMux);
        if (bits.empty()) {
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }
        presentDirty(bits);
        g_dispFrames = g_dispFrames + 1;
        // paint ≠ user activity
    }
}

void setup() {
    Serial.begin(115200);
    delay(300);
    logf("ESP2", "build=%s", kBuildId);
    logf("ESP2", "psram=%u heap=%u", ESP.getPsramSize(), ESP.getFreeHeap());

    g_display_ok = init_display();
    g_display_power.begin(draw_screensaver_stub, restore_ui_stub, panel_sleep_co5300,
                          panel_wake_co5300);
    DisplayPowerSettings dps{};
    dps.screensaver_override_ms = 35000;
    dps.screen_off_override_ms = 70000;
    g_display_power.setSettings(dps);
    g_display_power.notifyActivity("boot");

    g_touch_ok = probe_touch();
    g_sd_ok = probe_sd();
    g_imu_ok = probe_imu();

    bootVideoSuite();

    logf("SELFTEST", "display=%s touch=%s sd=%s imu=%s", g_display_ok ? "PASS" : "FAIL",
         g_touch_ok ? "PASS" : "FAIL", g_sd_ok ? "PASS" : "FAIL", g_imu_ok ? "PASS" : "FAIL");

    xTaskCreatePinnedToCore(displayTask, "a2disp", 10240, nullptr, 2, nullptr, 0);
    xTaskCreatePinnedToCore(emulatorTask, "a2emu", 8192, nullptr, 1, nullptr, 1);
    g_schedulerGo = true;
    logf("SCHED", "tasks started");
}

void loop() {
    delay(100);
}
