/**
 * ESP][ first physical Apple II text-core port (Sharp only).
 *
 * Path: 6502 → synthetic ROM → text RAM → TextDecoder → CO5300
 * Does NOT draw status as a fake ESP32 overlay inside the Apple II viewport.
 */
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>
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

static constexpr char kBuildId[] = "apple2_text";
static constexpr uint32_t kRunCycles = 200000; // enough for ROM text + markers + idle
static constexpr int kViewOffsetX = 20;        // (280 - 240) / 2
static constexpr int kViewOffsetY = 48;        // below rounded top

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

static void logf(const char *tag, const char *fmt, ...) {
    char buf[192];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    Serial.printf("[%s] %s\n", tag, buf);
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
    // Full Apple II frame redraw happens from loop when Active.
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

/** Sharp: decode Apple II text RAM → RGB565 → CO5300 (no CRT / artifact). */
static void render_apple_text_sharp(const uint8_t *ram) {
    if (!g_gfx || !g_display_ok) {
        return;
    }
    uint8_t chars[TextDecoder::kRows * TextDecoder::kCols];
    TextDecoder::decodeScreen(ram, TextDecoder::kPage1Base, chars);

    // Line buffer RGB565 for one glyph row strip (full width panel).
    static uint16_t line[LCD_WIDTH];
    const uint16_t bg = 0x0000;
    const uint16_t fg = 0xFFFF;

    for (int py = 0; py < TextDecoder::kRgbH; ++py) {
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
        const int dy = kViewOffsetY + py;
        if (dy >= 0 && dy < LCD_HEIGHT) {
            g_gfx->draw16bitRGBBitmap(0, dy, line, LCD_WIDTH, 1);
        }
    }
}

/** Write decimal cycle count into Apple II text row 9 after "CYCLES ". */
static void patch_cycle_digits(uint8_t *ram, uint32_t cycles) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%09lu", static_cast<unsigned long>(cycles));
    const int row = 9;
    const int startCol = 7; // after "CYCLES "
    for (int i = 0; i < 9; ++i) {
        const uint16_t addr = TextDecoder::cellAddress(TextDecoder::kPage1Base, row, startCol + i);
        ram[addr] = static_cast<uint8_t>(0x80u | static_cast<uint8_t>(buf[i]));
    }
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

static void run_apple2_text_port() {
    logf("APPLE2", "cpu init");
    g_cpu.setCallbacks(&g_a2bus, Apple2Bus::busRead, Apple2Bus::busWrite);

    logf("APPLE2", "ram init");
    g_a2bus.clearRam();
    g_a2bus.reset();
    logf("APPLE2", "ram place=internal_sram_bss bytes=%u",
         static_cast<unsigned>(Apple2Bus::kRamBytes));

    logf("APPLE2", "rom synthetic esp32-text-port");
    // Keep ROM image off the Arduino task stack (12 KiB would overflow).
    static uint8_t romImg[Rom::kApple2PlusRomBytes];
    if (generateSyntheticEsp32TextPortRom(romImg, sizeof(romImg)) != RomError::Ok ||
        g_a2bus.rom().load(romImg, sizeof(romImg)) != RomError::Ok) {
        logf("APPLE2", "[FAIL] rom load");
        return;
    }

    g_cpu.reset(); // fetch RESET vector into PC
    logf("APPLE2", "pc=%04X after reset", g_cpu.registers().pc);

    const uint32_t t0 = micros();
    g_a2bus.setAccessCycle(0);
    const uint32_t ran = g_cpu.runCycles(kRunCycles);
    g_a2bus.setAccessCycle(static_cast<uint32_t>(g_cpu.cycles() & 0xFFFFFFFFu));
    const uint32_t elapsedUs = micros() - t0;
    const double cps = elapsedUs > 0
                           ? (static_cast<double>(ran) * 1000000.0 / static_cast<double>(elapsedUs))
                           : 0.0;
    const double headroom = cps / 1023000.0; // ~1× Apple II/II+ class

    logf("APPLE2", "run cycles=%u elapsed_us=%u", ran, elapsedUs);
    logf("PERF", "ESP32 PHYSICAL MEASURED cpu_emu_cycles_per_s=%.0f", cps);
    logf("PERF", "ESP32 PHYSICAL MEASURED realtime_headroom_x=%.2f", headroom);

    const bool markerOk = verify_marker(g_a2bus.ram());
    logf("APPLE2", "marker=%s", markerOk ? "PASS" : "FAIL");

    patch_cycle_digits(g_a2bus.ram(), ran);
    const bool textOk = verify_text_content(g_a2bus.ram());
    logf("APPLE2", "text memory=%s", textOk ? "PASS" : "FAIL");

    const uint32_t tDec0 = micros();
    uint8_t chars[TextDecoder::kRows * TextDecoder::kCols];
    TextDecoder::decodeScreen(g_a2bus.ram(), TextDecoder::kPage1Base, chars);
    const uint32_t tDec = micros() - tDec0;
    logf("PERF", "ESP32 PHYSICAL MEASURED text_decode_us=%u", tDec);

    const uint32_t tDraw0 = micros();
    if (g_display_power.isInteractive()) {
        g_gfx->fillScreen(0x0000);
        render_apple_text_sharp(g_a2bus.ram());
    }
    const uint32_t tDraw = micros() - tDraw0;
    logf("PERF", "ESP32 PHYSICAL MEASURED display_transfer_us=%u", tDraw);
    logf("APPLE2", "text render=%s", (textOk && markerOk) ? "PASS" : "FAIL");

    logf("RAM", "heap_free=%u heap_min=%u", ESP.getFreeHeap(), ESP.getMinFreeHeap());
    if (ESP.getPsramSize() > 0) {
        logf("RAM", "psram_free=%u", ESP.getFreePsram());
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
    // Longer timeouts during diagnostic so the test screen stays visible.
    dps.screensaver = ScreensaverTimeout::Min5;
    dps.screen_off = ScreenOffTimeout::Min10;
    g_display_power.setSettings(dps);
    g_display_power.notifyActivity("boot");

    g_touch_ok = probe_touch();
    g_sd_ok = probe_sd();
    g_imu_ok = probe_imu();

    run_apple2_text_port();

    // Hardware regression summary (probes only — emulator does not use them yet).
    logf("SELFTEST", "display=%s touch=%s sd=%s imu=%s", g_display_ok ? "PASS" : "FAIL",
         g_touch_ok ? "PASS" : "FAIL", g_sd_ok ? "PASS" : "FAIL", g_imu_ok ? "PASS" : "FAIL");
}

void loop() {
    const uint32_t now = millis();
    g_display_power.update(now);

    // Touch wake / activity (FT3168 TD_STATUS)
    if (g_touch_ok) {
        Wire.beginTransmission(FT3168_I2C_ADDR);
        Wire.write(0x02);
        if (Wire.endTransmission(false) == 0 && Wire.requestFrom((int)FT3168_I2C_ADDR, 1) == 1) {
            const uint8_t td = Wire.read() & 0x0F;
            if (td > 0) {
                const bool woke = g_display_power.notifyActivity("touch");
                if (woke && g_display_ok) {
                    g_gfx->fillScreen(0x0000);
                    render_apple_text_sharp(g_a2bus.ram());
                }
            }
        }
    }

    // Keep 6502 idle loop alive without using millis as Apple II time.
    if (g_cpu.cycles() > 0) {
        g_cpu.runCycles(1000);
        g_a2bus.setAccessCycle(static_cast<uint32_t>(g_cpu.cycles() & 0xFFFFFFFFu));
    }

    delay(16);
}
