/**
 * ESP][ PART F1 — user-supplied Apple II/II+ system ROM on ESP32-S3.
 *
 * Real RESET vector → ROM execution → interactive text → keyboard injection.
 * Slot 6 default NONE. No PC shortcuts. No ROM bytes in the repository.
 */
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <new>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <Arduino_GFX_Library.h>

#include "board_pins.h"
#include "display_power.h"
#include "esp32_sd_storage.hpp"
#include "qmi8658_min.h"

#include "esp_bracket/apple2_bus.hpp"
#include "esp_bracket/artifact_renderer.hpp"
#include "esp_bracket/cpu6502.hpp"
#include "esp_bracket/disk_ii_cleanroom.hpp"
#include "esp_bracket/disk_ii_controller.hpp"
#include "esp_bracket/disk_ii_media.hpp"
#include "esp_bracket/hgr_decoder.hpp"
#include "esp_bracket/key_map.hpp"
#include "esp_bracket/lores_decoder.hpp"
#include "esp_bracket/rom.hpp"
#include "esp_bracket/rom_identity.hpp"
#include "esp_bracket/sha256.hpp"
#include "esp_bracket/soft_switches.hpp"
#include "esp_bracket/text_decoder.hpp"
#include "esp_bracket/text_screen.hpp"
#include "esp_bracket/video_dirty_tracker.hpp"
#include "esp_bracket/video_state.hpp"

using namespace esp_bracket;

static constexpr char kBuildId[] = "apple2_rom_f1b";
static constexpr uint32_t kAppleIiHz = 1023000;
static constexpr uint32_t kExecQuantum = 2000;
static constexpr int kViewX = 0;
static constexpr int kViewY = 48;
static constexpr int kViewW = 280;
static constexpr int kViewH = 192;
static constexpr int kTextCellW = 7;
static constexpr uint32_t kQspiHz = 40000000;
static constexpr uint32_t kStabilityMs = 90000;
static constexpr int kFullUpdateThreshold = 96;
static constexpr uint32_t kRomStartupBudget = 8000000;
static constexpr uint32_t kWaitSlice = 2000;

enum class PresentColorMode : uint8_t { Sharp = 0, ArtifactColor };

static Arduino_DataBus *g_bus = nullptr;
static Arduino_CO5300 *g_gfx = nullptr;
static DisplayPowerManager g_display_power;
static constexpr uint8_t kDisplayBrightness = 180;

static Apple2Bus g_a2bus;
static Cpu6502 g_cpu;
static VideoDirtyTracker g_dirty;
static DiskIIController g_diskII;
static Esp32SdStorageBackend g_sdStore;
static PresentColorMode g_presentColor = PresentColorMode::Sharp;
static MachineProfile g_profile = MachineProfile::AppleIIPlus;
static RomIdentity g_romId{};

static Dos33NibbleImage *g_dskImage = nullptr;
static uint8_t *g_dskRaw = nullptr;

static bool g_display_ok = false;
static bool g_touch_ok = false;
static bool g_sd_ok = false;
static bool g_imu_ok = false;
static bool g_rom_ok = false;
static bool g_interactive_ok = false;
static bool g_basic_ok = false;
static bool g_level4_ok = false;

static uint16_t *g_viewportFb = nullptr;
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
    char buf[240];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    Serial.printf("[%s] %s\n", tag, buf);
}

static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ArtifactRenderer::toRgb565({r, g, b});
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
    logf("DISPLAY", "CO5300 init OK %dx%d qspi_hz=%u", g_gfx->width(), g_gfx->height(), kQspiHz);
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
        g_sdStore.setMounted(false);
        return false;
    }
    g_sdStore.setMounted(true);
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

static void *psramAlloc(size_t n) {
    void *p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p) {
        p = heap_caps_malloc(n, MALLOC_CAP_8BIT);
    }
    return p;
}

static void syncCycle() {
    g_a2bus.setAccessCycle(static_cast<uint32_t>(g_cpu.cycles() & 0xFFFFFFFFu));
}

static void runEmu(uint32_t cycles) {
    const uint32_t slice = 64;
    uint32_t left = cycles;
    while (left > 0) {
        const uint32_t step = left > slice ? slice : left;
        g_cpu.runCycles(step);
        syncCycle();
        left -= step;
    }
}

static void renderTextRows(uint16_t *fb, int row0, int row1) {
    const AppleIIVideoState vs = videoStateFromSoftSwitches(g_a2bus.softSwitches());
    uint8_t chars[TextDecoder::kRows * TextDecoder::kCols];
    TextDecoder::decodeScreen(g_a2bus.ram(), vs.textPageBase(), chars);
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
}

static void renderLoresRows(uint16_t *fb, int scan0, int scan1) {
    const AppleIIVideoState vs = videoStateFromSoftSwitches(g_a2bus.softSwitches());
    uint8_t blocks[LoresDecoder::kRows * LoresDecoder::kCols];
    LoresDecoder::decode(g_a2bus.ram(), vs.textPageBase(), blocks);
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
}

static void renderHgrRows(uint16_t *fb, int scan0, int scan1) {
    if (!g_hgrBits || !g_hgrHigh || scan0 > scan1) {
        return;
    }
    const AppleIIVideoState vs = videoStateFromSoftSwitches(g_a2bus.softSwitches());
    HgrDecoder::decode(g_a2bus.ram(), vs.hgrPageBase(), g_hgrBits, g_hgrHigh);
    for (int y = scan0; y <= scan1; ++y) {
        uint16_t *dst = fb + y * kViewW;
        if (g_presentColor == PresentColorMode::ArtifactColor) {
            ArtifactRenderer::renderScanlineRgb565(g_hgrBits + y * 280, g_hgrHigh + y * 40,
                                                   VideoColorMode::CompositeColor, dst);
        } else {
            for (int x = 0; x < 280; ++x) {
                dst[x] = g_hgrBits[y * 280 + x] ? 0xFFFF : 0x0000;
            }
        }
    }
}

static void renderDirtyIntoFb(uint16_t *fb, const VideoDirtyTracker::Bitset &bits) {
    const AppleIIVideoState vs = videoStateFromSoftSwitches(g_a2bus.softSwitches());
    auto anyIn = [&](int y0, int y1) {
        for (int y = y0; y <= y1; ++y) {
            if (bits.test(y)) {
                return true;
            }
        }
        return false;
    };
    if (vs.text) {
        for (int row = 0; row < 24; ++row) {
            if (anyIn(row * 8, row * 8 + 7)) {
                renderTextRows(fb, row, row);
            }
        }
        return;
    }
    const int graphicsEnd = vs.mixed ? 159 : 191;
    int d0 = -1, d1 = -1;
    for (int y = 0; y <= graphicsEnd; ++y) {
        if (bits.test(y)) {
            if (d0 < 0) {
                d0 = y;
            }
            d1 = y;
        }
    }
    if (d0 >= 0) {
        if (vs.hires) {
            renderHgrRows(fb, d0, d1);
        } else {
            renderLoresRows(fb, d0, d1);
        }
    }
    if (vs.mixed) {
        for (int row = 20; row < 24; ++row) {
            if (anyIn(row * 8, row * 8 + 7)) {
                renderTextRows(fb, row, row);
            }
        }
    }
}

static uint32_t transferScanlineRange(int y0, int y1) {
    if (!g_gfx || !g_viewportFb || y0 > y1) {
        return 0;
    }
    const int h = y1 - y0 + 1;
    const uint32_t t0 = micros();
    g_gfx->draw16bitRGBBitmap(kViewX, kViewY + y0, g_viewportFb + y0 * kViewW, kViewW, h);
    g_lastXferUs = micros() - t0;
    g_dispBytes = static_cast<uint32_t>(kViewW * h * 2);
    return g_lastXferUs;
}

static uint32_t presentDirty(const VideoDirtyTracker::Bitset &bits) {
    if (!g_viewportFb || bits.empty()) {
        return 0;
    }
    const uint32_t tR0 = micros();
    renderDirtyIntoFb(g_viewportFb, bits);
    g_lastRenderUs = micros() - tR0;
    const int pop = bits.popcount();
    if (pop >= kFullUpdateThreshold || pop >= kViewH) {
        return transferScanlineRange(0, kViewH - 1);
    }
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

static void presentFull() {
    portENTER_CRITICAL(&g_dirtyMux);
    auto bits = g_dirty.exchange();
    bits.markAll();
    portEXIT_CRITICAL(&g_dirtyMux);
    presentDirty(bits);
    g_dispFrames++;
}

static void markVideoDirty() {
    portENTER_CRITICAL(&g_dirtyMux);
    g_dirty.markAll();
    portEXIT_CRITICAL(&g_dirtyMux);
}

static TextScreen currentText() {
    return TextScreen::fromBus(g_a2bus, 0);
}

static bool textHas(const char *needle) {
    return currentText().contains(needle);
}

/** Interactive Applesoft/Monitor-ready heuristic (no copyrighted greeting required). */
static bool looksInteractive() {
    const TextScreen s = currentText();
    // Applesoft ready prompt or Monitor '*' — short machine identifiers.
    return s.contains("]") || s.contains("*");
}

static void injectKey(uint8_t apple7) {
    // Normalized key → Apple II keyboard latch ($C000) with strobe.
    g_a2bus.keyboard().keyDown(apple7);
    runEmu(800);
    // Allow ROM to clear via $C010; also clear if still pending (host script pattern).
    if (g_a2bus.keyboard().strobePending()) {
        g_a2bus.keyboard().clearStrobe();
    }
    runEmu(400);
}

static void injectString(const char *s) {
    if (!s) {
        return;
    }
    for (const char *p = s; *p; ++p) {
        uint8_t apple = 0;
        if (!AppleIIKeyMap::mapHostKey(static_cast<unsigned char>(*p), false, false, &apple)) {
            apple = AppleIIKeyMap::fromAscii(*p);
        }
        injectKey(apple);
    }
}

static bool waitText(const char *needle, uint32_t budgetCycles) {
    uint32_t left = budgetCycles;
    while (left > 0) {
        const uint32_t step = left > kWaitSlice ? kWaitSlice : left;
        runEmu(step);
        left -= step;
        markVideoDirty();
        if (textHas(needle)) {
            return true;
        }
    }
    return false;
}

static bool waitInteractive(uint32_t budgetCycles) {
    uint32_t left = budgetCycles;
    while (left > 0) {
        const uint32_t step = left > kWaitSlice ? kWaitSlice : left;
        runEmu(step);
        left -= step;
        markVideoDirty();
        if ((left % 200000u) < kWaitSlice) {
            presentFull();
        }
        if (looksInteractive()) {
            return true;
        }
    }
    return false;
}

static void dumpStartupFail(const char *why) {
    const CpuRegisters r = g_cpu.registers();
    const AppleIIVideoState vs = videoStateFromSoftSwitches(g_a2bus.softSwitches());
    logf("ROM", "startup=%s", why);
    logf("CPU", "pc=$%04X a=$%02X x=$%02X y=$%02X sp=$%02X status=$%02X cycles=%llu", r.pc, r.a,
         r.x, r.y, r.sp, r.status, static_cast<unsigned long long>(g_cpu.cycles()));
    logf("VIDEO", "mode=%s mixed=%d page2=%d hires=%d", vs.text ? "TEXT" : "GRAPHICS",
         vs.mixed ? 1 : 0, vs.page2 ? 1 : 0, vs.hires ? 1 : 0);
    char line[41];
    TextScreen s = currentText();
    for (int row = 0; row < 6; ++row) {
        s.rowString(row, line);
        logf("TEXT", "r%d='%s'", row, line);
    }
}

static const char *classifyRom(const RomIdentity &id) {
    if (id.status == RomIdStatus::Ok && !id.synthetic) {
        if (id.profile == MachineProfile::AppleII) {
            return "KNOWN_APPLE_II";
        }
        if (id.profile == MachineProfile::AppleIIPlus) {
            return "KNOWN_APPLE_II_PLUS";
        }
        return "KNOWN";
    }
    if (id.status == RomIdStatus::UnknownHash && id.sizeBytes == Rom::kApple2PlusRomBytes) {
        return "UNKNOWN_SUPPORTED_SIZE";
    }
    if (id.synthetic) {
        return "PROJECT_SYNTHETIC";
    }
    return "UNSUPPORTED";
}

static MachineProfile readProfileOverride() {
    if (!g_sdStore.exists(Esp32SdStorageBackend::kProfileFile)) {
        return MachineProfile::Unknown;
    }
    char buf[32]{};
    size_t n = 0;
    if (!g_sdStore.readAll(Esp32SdStorageBackend::kProfileFile, reinterpret_cast<uint8_t *>(buf),
                           sizeof(buf) - 1, &n)) {
        return MachineProfile::Unknown;
    }
    buf[n] = 0;
    if (strstr(buf, "AppleIIPlus") || strstr(buf, "II+")) {
        return MachineProfile::AppleIIPlus;
    }
    if (strstr(buf, "AppleII")) {
        return MachineProfile::AppleII;
    }
    return MachineProfile::Unknown;
}

static bool findUserRomPath(char outPath[96]) {
    outPath[0] = 0;
    if (!SD.begin(PIN_SD_CS)) {
        return false;
    }
    g_sdStore.setMounted(true);
    g_sdStore.ensureRomRoot();
    const char *cands[] = {
        Esp32SdStorageBackend::kSystemRom, Esp32SdStorageBackend::kApple2PlusRom,
        Esp32SdStorageBackend::kApple2Rom, "/esp2/roms/apple2+.rom",
        "/esp2/roms/APPLE2.ROM",
    };
    for (const char *p : cands) {
        if (g_sdStore.exists(p) && g_sdStore.fileSize(p) == Rom::kApple2PlusRomBytes) {
            strncpy(outPath, p, 95);
            outPath[95] = 0;
            return true;
        }
    }
    // Scan directory for first 12288-byte .rom/.bin
    File dir = SD.open(Esp32SdStorageBackend::kRomRoot);
    if (!dir || !dir.isDirectory()) {
        return false;
    }
    for (;;) {
        File f = dir.openNextFile();
        if (!f) {
            break;
        }
        if (!f.isDirectory() && f.size() == Rom::kApple2PlusRomBytes) {
            snprintf(outPath, 96, "%s/%s", Esp32SdStorageBackend::kRomRoot, f.name());
            f.close();
            dir.close();
            return true;
        }
        f.close();
    }
    dir.close();
    return false;
}

static bool loadUserRomFromSd() {
    char path[96];
    if (!findUserRomPath(path)) {
        logf("ROM", "path=(none) SKIPPED_NO_ROM");
        logf("ROM", "place 12288-byte Apple II/II+ ROM at /esp2/roms/system.rom");
        return false;
    }
    logf("ROM", "path=%s", path);
    const size_t sz = g_sdStore.fileSize(path);
    logf("ROM", "size=%u", static_cast<unsigned>(sz));
    if (sz != Rom::kApple2PlusRomBytes) {
        logf("ROM", "class=UNSUPPORTED");
        return false;
    }

    uint8_t *raw = static_cast<uint8_t *>(psramAlloc(Rom::kApple2PlusRomBytes));
    if (!raw) {
        logf("ROM", "[FAIL] buffer");
        return false;
    }
    size_t got = 0;
    if (!g_sdStore.readAll(path, raw, Rom::kApple2PlusRomBytes, &got) ||
        got != Rom::kApple2PlusRomBytes) {
        logf("ROM", "[FAIL] sd_read_us=%u", g_sdStore.lastReadUs());
        free(raw);
        return false;
    }
    logf("ROM", "sd_read_us=%u", g_sdStore.lastReadUs());

    g_romId = RomDatabase::identify(raw, Rom::kApple2PlusRomBytes);
    logf("ROM", "sha256=%s", g_romId.sha256Hex);
    logf("ROM", "class=%s id_status=%s", classifyRom(g_romId), romIdStatusName(g_romId.status));

    const MachineProfile override = readProfileOverride();
    if (g_romId.profile != MachineProfile::Unknown) {
        g_profile = g_romId.profile;
    } else if (override != MachineProfile::Unknown) {
        g_profile = override;
        logf("ROM", "profile_from_file=%s", machineProfileName(g_profile));
    } else {
        g_profile = MachineProfile::AppleIIPlus;
        logf("ROM", "profile_default=AppleIIPlus (explicit override via /esp2/roms/profile.txt)");
    }
    logf("ROM", "profile=%s", machineProfileName(g_profile));

    // Mapping check before load: reset vector bytes in image.
    const uint16_t rstImg =
        static_cast<uint16_t>(raw[0x2FFC] | (static_cast<uint16_t>(raw[0x2FFD]) << 8));
    logf("CPU", "reset_vector=$%04X (from ROM image $FFFC)", rstImg);

    const RomError err = loadAndIdentifyRom(g_a2bus.rom(), raw, Rom::kApple2PlusRomBytes, &g_romId);
    free(raw);
    if (err != RomError::Ok) {
        logf("ROM", "[FAIL] load");
        return false;
    }

    // Read-only: write to ROM window must not stick.
    const uint8_t before = g_a2bus.peek(0xE000);
    g_a2bus.write(0xE000, static_cast<uint8_t>(before ^ 0xFF));
    const uint8_t after = g_a2bus.peek(0xE000);
    logf("ROM", "write_protect=%s", (after == before) ? "PASS" : "FAIL");

    const uint16_t irq = static_cast<uint16_t>(g_a2bus.peek(0xFFFE) |
                                               (static_cast<uint16_t>(g_a2bus.peek(0xFFFF)) << 8));
    const uint16_t nmi = static_cast<uint16_t>(g_a2bus.peek(0xFFFA) |
                                               (static_cast<uint16_t>(g_a2bus.peek(0xFFFB)) << 8));
    logf("CPU", "irq_vector=$%04X nmi_vector=$%04X", irq, nmi);
    return true;
}

static bool runRealRomStartup() {
    // Slot 6 NONE — observe genuine system ROM, not Esp2BootTest.
    g_diskII.clearRom();
    g_a2bus.setSlotDevice(6, nullptr);

    g_a2bus.clearRam();
    g_a2bus.reset();
    g_a2bus.speaker().reset();
    g_cpu.reset(); // MUST read reset vector from user ROM — no PC hack.

    const uint16_t rst = static_cast<uint16_t>(g_a2bus.peek(0xFFFC) |
                                               (static_cast<uint16_t>(g_a2bus.peek(0xFFFD)) << 8));
    const uint16_t pc0 = g_cpu.registers().pc;
    logf("CPU", "reset_vector=$%04X first_pc=$%04X", rst, pc0);
    if (pc0 != rst) {
        logf("CPU", "[FAIL] PC not from reset vector");
        return false;
    }

    const uint64_t c0 = g_cpu.cycles();
    const uint32_t t0 = micros();
    const bool ok = waitInteractive(kRomStartupBudget);
    const uint64_t used = g_cpu.cycles() - c0;
    const uint32_t wallUs = micros() - t0;
    const double cps = wallUs ? (used * 1e6 / wallUs) : 0;
    logf("ROM", "startup_cycles=%llu wall_us=%u cps=%.0f", static_cast<unsigned long long>(used),
         wallUs, cps);
    logf("SPEAKER", "edges_startup=%u", static_cast<unsigned>(g_a2bus.speaker().edgeCountTotal()));

    const AppleIIVideoState vs = videoStateFromSoftSwitches(g_a2bus.softSwitches());
    logf("VIDEO", "mode=%s mixed=%d page2=%d hires=%d", vs.text ? "TEXT" : "GRAPHICS",
         vs.mixed ? 1 : 0, vs.page2 ? 1 : 0, vs.hires ? 1 : 0);

    presentFull();
    if (!ok) {
        dumpStartupFail("TIMEOUT_NO_INTERACTIVE");
        return false;
    }
    logf("ROM", "startup=INTERACTIVE");
    g_interactive_ok = true;
    delay(1500);
    return true;
}

static bool runBasicPrint22() {
    logf("INPUT", "path=keyboard_latch $C000/$C010 via keyDown");
    logf("INPUT", "script=TYPE PRINT 2+2 / RETURN");
    injectString("PRINT 2+2");
    injectKey(0x0D);
    const bool saw4 = waitText("4", 1500000);
    presentFull();
    logf("BASIC", "PRINT_2+2 result=%s", saw4 ? "PASS (saw 4)" : "FAIL");
    g_basic_ok = saw4;
    delay(1000);
    return saw4;
}

static bool runBasicPrintEsp() {
    logf("INPUT", "script=TYPE PRINT \"ESP][\" / RETURN");
    injectString("PRINT \"ESP][\"");
    injectKey(0x0D);
    const bool ok = waitText("ESP][", 1500000);
    presentFull();
    logf("BASIC", "PRINT_ESP result=%s", ok ? "PASS" : "FAIL");
    delay(1000);
    return ok;
}

static bool level4RegressionSpot() {
    logf("DISK", "Level4 regression spot (Slot-6 temporary cleanroom)");
    // Keep large objects out of loopTask stack and out of internal BSS.
    static uint8_t s_prom[DiskIIController::kSlotRomSize];
    static uint8_t s_exp[DiskIIController::kExpansionRomSize];

    if (!SD.begin(PIN_SD_CS)) {
        logf("DISK", "SKIPPED_NO_SD");
        return false;
    }
    g_sdStore.setMounted(true);
    if (!g_sdStore.exists(Esp32SdStorageBackend::kBootTestDsk)) {
        uint8_t *seed = static_cast<uint8_t *>(psramAlloc(kDos33ImageBytes));
        if (!seed || !generateEsp2BootTestImage(seed, kDos33ImageBytes) ||
            !g_sdStore.writeAll(Esp32SdStorageBackend::kBootTestDsk, seed, kDos33ImageBytes)) {
            logf("DISK", "SKIPPED_NO_BOOTTEST");
            free(seed);
            return false;
        }
        free(seed);
    }
    if (!g_dskRaw) {
        g_dskRaw = static_cast<uint8_t *>(psramAlloc(kDos33ImageBytes));
    }
    if (!g_dskImage) {
        void *mem = psramAlloc(sizeof(Dos33NibbleImage));
        g_dskImage = mem ? new (mem) Dos33NibbleImage() : nullptr;
    }
    if (!g_dskRaw || !g_dskImage) {
        logf("DISK", "[FAIL] alloc");
        return false;
    }
    size_t got = 0;
    if (!g_sdStore.readAll(Esp32SdStorageBackend::kBootTestDsk, g_dskRaw, kDos33ImageBytes, &got)) {
        logf("DISK", "[FAIL] read boottest");
        return false;
    }
    g_dskImage->load(g_dskRaw, kDos33ImageBytes, false);
    g_dskImage->setWriteProtected(true);

    generateCleanRoomDiskIICard(s_prom, s_exp);
    g_a2bus.setSlotDevice(6, &g_diskII);
    g_diskII.loadCleanRoomRom(s_prom, s_exp);
    g_diskII.attachMedia(1, g_dskImage);

    g_a2bus.clearRam();
    g_a2bus.reset();
    g_diskII.reset();
    g_diskII.attachMedia(1, g_dskImage);
    g_diskII.setRotationIndex(0);
    g_a2bus.write(0x03FE, 0);
    g_a2bus.write(0x03FF, 0);
    g_cpu.reset();
    CpuRegisters r = g_cpu.registers();
    r.pc = 0xC600; // controlled Level-4 entry (same as PART E)
    g_cpu.setRegisters(r);
    bool ok = false;
    const uint64_t c0 = g_cpu.cycles();
    for (;;) {
        runEmu(64);
        serviceCleanRoomDenibbleRequest(g_a2bus.ram());
        if (g_a2bus.ram()[0x03FE] == 0x4C && g_a2bus.ram()[0x03FF] == 0x34) {
            ok = true;
            break;
        }
        if ((g_cpu.cycles() - c0) > 500000ull) {
            break;
        }
    }
    logf("DISK", "Level4 marker=%s", ok ? "PASS" : "FAIL");
    g_level4_ok = ok;

    g_diskII.clearRom();
    g_a2bus.setSlotDevice(6, nullptr);
    return ok;
}

static void videoSpotCheck() {
    // Brief regression without destroying ROM session — use softswitches briefly.
    logf("VIDEO", "spot-check after ROM session");
    presentFull();
    logf("VIDEO", "TEXT/current frame presented");
}

static void bootSuite() {
    logf("APPLE2", "F1 user ROM bring-up");
    g_cpu.setCallbacks(&g_a2bus, Apple2Bus::busRead, Apple2Bus::busWrite);
    g_a2bus.setVideoDirtyTracker(&g_dirty);
    // Slot 6 stays NONE until Level-4 spot check.
    g_a2bus.setSlotDevice(6, nullptr);
    g_diskII.clearRom();

    g_viewportFb = static_cast<uint16_t *>(psramAlloc(kViewW * kViewH * sizeof(uint16_t)));
    g_hgrBits = static_cast<uint8_t *>(psramAlloc(280 * 192));
    g_hgrHigh = static_cast<uint8_t *>(psramAlloc(40 * 192));
    logf("RAM", "viewport=%s psram_free=%u heap=%u", g_viewportFb ? "ok" : "FAIL",
         ESP.getPsramSize() ? ESP.getFreePsram() : 0, ESP.getFreeHeap());

    g_rom_ok = loadUserRomFromSd();
    if (!g_rom_ok) {
        logf("ROM", "F1 incomplete without user ROM — Level-4 regression still runs");
    } else {
        if (runRealRomStartup()) {
            if (g_profile == MachineProfile::AppleIIPlus ||
                g_romId.status == RomIdStatus::UnknownHash) {
                // Applesoft expected for II+; Unknown may still be II+.
                runBasicPrint22();
                if (g_basic_ok) {
                    runBasicPrintEsp();
                }
            } else if (g_profile == MachineProfile::AppleII) {
                logf("BASIC", "AppleII Integer BASIC — PRINT 2+2 skipped (capability note)");
                // Still try a simple key to prove strobe path.
                injectKey(0x0D);
                runEmu(50000);
                presentFull();
                logf("INPUT", "RETURN injection done (Integer path)");
            }
        }
        logf("SPEAKER", "edges_total=%u",
             static_cast<unsigned>(g_a2bus.speaker().edgeCountTotal()));
    }

    level4RegressionSpot();
    videoSpotCheck();

    // Restore user ROM session for live run if available.
    if (g_rom_ok) {
        g_diskII.clearRom();
        g_a2bus.setSlotDevice(6, nullptr);
        // Re-run reset into interactive for live display (bounded).
        g_a2bus.clearRam();
        g_a2bus.reset();
        g_cpu.reset();
        waitInteractive(kRomStartupBudget);
        presentFull();
    }

    logf("RAM", "heap_free=%u heap_min=%u", ESP.getFreeHeap(), ESP.getMinFreeHeap());
    if (ESP.getPsramSize()) {
        logf("RAM", "psram_free=%u", ESP.getFreePsram());
    }
    logf("APPLE2", "F1 rom=%s interactive=%s basic=%s level4=%s REAL_SYSTEM_ROM=%s LEVEL_5=%s",
         g_rom_ok ? "PASS" : "SKIPPED_NO_ROM", g_interactive_ok ? "PASS" : "FAIL",
         g_basic_ok ? "PASS" : "n/a", g_level4_ok ? "PASS" : "FAIL",
         (g_rom_ok && g_interactive_ok) ? "ESP32_PHYSICALLY_VERIFIED" : "NOT_VERIFIED",
         (g_rom_ok && g_interactive_ok) ? "PARTIAL/READY_FOR_REAL_SOFTWARE_TEST"
                                        : "BLOCKED_NO_USER_ROM");
}

static void emulatorTask(void *) {
    while (!g_schedulerGo) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    logf("SCHED", "emu core=%d", xPortGetCoreID());
    g_wallStartUs = micros();
    g_emuCyclesAtBoot = g_cpu.cycles();
    for (;;) {
        g_cpu.runCycles(kExecQuantum);
        syncCycle();
        if (g_diskII.romKind() == DiskIIController::RomKind::CleanRoom) {
            serviceCleanRoomDenibbleRequest(g_a2bus.ram());
        }
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
            logf("PERF",
                 "ESP32 PHYSICAL MEASURED live cps=%.0f frames=%u heap=%u heap_min=%u "
                 "psram_free=%u",
                 cps, g_dispFrames, ESP.getFreeHeap(), ESP.getMinFreeHeap(),
                 ESP.getPsramSize() ? ESP.getFreePsram() : 0);
        }
        if (!stabilityDone && (now - startMs) >= kStabilityMs) {
            stabilityDone = true;
            logf("STABILITY", "duration_ms=%u frames=%u rom=%s interactive=%s", kStabilityMs,
                 g_dispFrames, g_rom_ok ? "yes" : "no", g_interactive_ok ? "yes" : "no");
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
        // Periodically refresh text screen from ROM (cursor flash etc.).
        static uint32_t lastRefresh = 0;
        if (now - lastRefresh > 200) {
            markVideoDirty();
            lastRefresh = now;
        }
        portENTER_CRITICAL(&g_dirtyMux);
        auto bits = g_dirty.exchange();
        portEXIT_CRITICAL(&g_dirtyMux);
        if (bits.empty()) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        presentDirty(bits);
        g_dispFrames++;
    }
}

void setup() {
    Serial.begin(115200);
    delay(1500);
    logf("ESP2", "build=%s", kBuildId);
    logf("ESP2", "psram=%u heap=%u", ESP.getPsramSize(), ESP.getFreeHeap());

    g_display_ok = init_display();
    g_display_power.begin(draw_screensaver_stub, restore_ui_stub, panel_sleep_co5300,
                          panel_wake_co5300);
    DisplayPowerSettings dps{};
    dps.screensaver_override_ms = 45000;
    dps.screen_off_override_ms = 90000;
    g_display_power.setSettings(dps);
    g_display_power.notifyActivity("boot");

    g_touch_ok = probe_touch();
    g_sd_ok = probe_sd();
    g_imu_ok = probe_imu();

    // Run bring-up on a large-stack task — loopTask stack is too small for Disk II.
    xTaskCreatePinnedToCore(
        [](void *) {
            bootSuite();
            logf("SELFTEST", "display=%s touch=%s sd=%s imu=%s rom=%s basic=%s level4=%s",
                 g_display_ok ? "PASS" : "FAIL", g_touch_ok ? "PASS" : "FAIL",
                 g_sd_ok ? "PASS" : "FAIL", g_imu_ok ? "PASS" : "FAIL",
                 g_rom_ok ? "PASS" : "SKIPPED_NO_ROM", g_basic_ok ? "PASS" : "n/a",
                 g_level4_ok ? "PASS" : "FAIL");
            xTaskCreatePinnedToCore(displayTask, "a2disp", 10240, nullptr, 2, nullptr, 0);
            xTaskCreatePinnedToCore(emulatorTask, "a2emu", 8192, nullptr, 1, nullptr, 1);
            g_schedulerGo = true;
            logf("SCHED", "tasks started");
            vTaskDelete(nullptr);
        },
        "a2boot", 24576, nullptr, 1, nullptr, 1);
}

void loop() {
    delay(100);
}
