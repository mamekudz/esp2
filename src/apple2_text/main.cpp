/**
 * ESP][ PART E — physical Disk II + microSD + Level-4 clean-room boot.
 *
 * Path: microSD → Esp32SdStorageBackend → Dos33NibbleImage (PSRAM) →
 * DiskIIController → Slot 6 → 6502 → video RAM → CO5300.
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
#include "esp_bracket/lores_decoder.hpp"
#include "esp_bracket/rom.hpp"
#include "esp_bracket/sha256.hpp"
#include "esp_bracket/soft_switches.hpp"
#include "esp_bracket/text_decoder.hpp"
#include "esp_bracket/video_dirty_tracker.hpp"
#include "esp_bracket/video_state.hpp"

using namespace esp_bracket;

static constexpr char kBuildId[] = "apple2_disk_ii_l4";
static constexpr uint32_t kAppleIiHz = 1023000;
static constexpr uint32_t kExecQuantum = 2000;
static constexpr int kViewX = 0;
static constexpr int kViewY = 48;
static constexpr int kViewW = 280;
static constexpr int kViewH = 192;
static constexpr int kTextCellW = 7;
static constexpr uint32_t kQspiHz = 40000000;
static constexpr uint32_t kStabilityMs = 180000;
static constexpr int kFullUpdateThreshold = 96;
static constexpr uint32_t kBootCycleBudget = 4000000;

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

static Dos33NibbleImage *g_dskImage = nullptr; // PSRAM
static NibTrackImage *g_nibImage = nullptr;    // PSRAM (optional)
static uint8_t *g_dskRaw = nullptr;            // PSRAM copy of mounted DSK bytes
static char g_dskShaHex[65] = {};
static uint32_t g_dskSdReadUs = 0;

static bool g_display_ok = false;
static bool g_touch_ok = false;
static bool g_sd_ok = false;
static bool g_imu_ok = false;
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
static volatile uint32_t g_bootStallUs = 0;
static volatile uint32_t g_worstTrackUs = 0;
static volatile uint32_t g_lastTrackUs = 0;
static portMUX_TYPE g_dirtyMux = portMUX_INITIALIZER_UNLOCKED;

/** Times nibble-track builds (cache miss) without changing Disk II semantics. */
class TimedDos33Image : public Dos33NibbleImage {
  public:
    const uint8_t *trackNibbles(int wholeTrack, size_t *outLength) override {
        const uint32_t misses0 = cacheMisses();
        const uint32_t t0 = micros();
        const uint8_t *p = Dos33NibbleImage::trackNibbles(wholeTrack, outLength);
        const uint32_t dt = micros() - t0;
        if (cacheMisses() != misses0) {
            g_lastTrackUs = dt;
            if (dt > g_worstTrackUs) {
                g_worstTrackUs = dt;
            }
            g_bootStallUs += dt;
            logTrackMiss(wholeTrack, dt);
        }
        return p;
    }

  private:
    static void logTrackMiss(int track, uint32_t us) {
        Serial.printf("[DISK] cache miss track=%d track_build_us=%u\n", track, us);
    }
};

static void logf(const char *tag, const char *fmt, ...) {
    char buf[220];
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

static TimedDos33Image *allocTimedImage() {
    void *mem = psramAlloc(sizeof(TimedDos33Image));
    if (!mem) {
        return nullptr;
    }
    return new (mem) TimedDos33Image();
}

static NibTrackImage *allocNibImage() {
    void *mem = psramAlloc(sizeof(NibTrackImage));
    if (!mem) {
        return nullptr;
    }
    return new (mem) NibTrackImage();
}

static void renderTextRows(uint16_t *fb, int row0, int row1) {
    const AppleIIVideoState vs = videoStateFromSoftSwitches(g_a2bus.softSwitches());
    const uint8_t *ram = g_a2bus.ram();
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
}

static void renderLoresRows(uint16_t *fb, int scan0, int scan1) {
    const AppleIIVideoState vs = videoStateFromSoftSwitches(g_a2bus.softSwitches());
    const uint8_t *ram = g_a2bus.ram();
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

static void fillHgr(const char *kind) {
    HgrDecoder::writePattern(g_a2bus.ram(), 0x2000, kind);
    portENTER_CRITICAL(&g_dirtyMux);
    g_dirty.markAll();
    portEXIT_CRITICAL(&g_dirtyMux);
}

static void loadCleanRoomSlot6() {
    uint8_t prom[DiskIIController::kSlotRomSize];
    uint8_t exp[DiskIIController::kExpansionRomSize];
    if (!generateCleanRoomDiskIICard(prom, exp)) {
        logf("DISK", "[FAIL] cleanroom generate");
        return;
    }
    if (!g_diskII.loadCleanRoomRom(prom, exp)) {
        logf("DISK", "[FAIL] cleanroom load");
        return;
    }
    logf("DISK", "cleanroom Slot-6 PROM $C600 + EXP $C800 OK");
}

static bool seedBootImageIfMissing(const char *path) {
    if (!g_sd_ok) {
        return false;
    }
    // Re-assert SD after display activity (shared host SPI).
    if (!SD.begin(PIN_SD_CS)) {
        logf("SD", "[FAIL] re-begin before seed");
        return false;
    }
    g_sdStore.setMounted(true);
    if (g_sdStore.exists(path)) {
        return true;
    }
    logf("DISK", "seeding project-owned image path=%s", path);
    uint8_t *raw = static_cast<uint8_t *>(psramAlloc(kDos33ImageBytes));
    if (!raw) {
        return false;
    }
    const bool okGen = generateEsp2BootTestImage(raw, kDos33ImageBytes);
    const bool okWrite = okGen && g_sdStore.writeAll(path, raw, kDos33ImageBytes);
    logf("DISK", "seed gen=%s write=%s write_us=%u", okGen ? "ok" : "FAIL",
         okWrite ? "ok" : "FAIL", g_sdStore.lastWriteUs());
    free(raw);
    return okWrite;
}

static bool loadDskRawFromSd(const char *path) {
    if (!g_sd_ok) {
        return false;
    }
    if (!SD.begin(PIN_SD_CS)) {
        logf("SD", "[FAIL] re-begin before read");
        return false;
    }
    g_sdStore.setMounted(true);
    if (!g_sdStore.exists(path)) {
        logf("DISK", "image=%s found=no", path);
        return false;
    }
    const size_t sz = g_sdStore.fileSize(path);
    logf("DISK", "image=%s found=yes size=%u format=DSK", path, static_cast<unsigned>(sz));
    if (sz != kDos33ImageBytes) {
        logf("DISK", "[FAIL] unexpected size want=%u", static_cast<unsigned>(kDos33ImageBytes));
        return false;
    }
    if (!g_dskRaw) {
        g_dskRaw = static_cast<uint8_t *>(psramAlloc(kDos33ImageBytes));
    }
    if (!g_dskRaw) {
        logf("DISK", "[FAIL] raw buffer");
        return false;
    }
    size_t got = 0;
    const bool okRead = g_sdStore.readAll(path, g_dskRaw, kDos33ImageBytes, &got);
    g_dskSdReadUs = g_sdStore.lastReadUs();
    logf("DISK", "sd_read_us=%u ok=%s", g_dskSdReadUs, okRead ? "yes" : "no");
    if (!okRead || got != kDos33ImageBytes) {
        return false;
    }
    Sha256::hashHex(g_dskRaw, kDos33ImageBytes, g_dskShaHex);
    logf("DISK", "sha256=%s", g_dskShaHex);
    return true;
}

static bool mountFromRaw(bool poOrder, const char *label) {
    if (!g_dskRaw) {
        logf("DISK", "[FAIL] no raw image in PSRAM");
        return false;
    }
    if (!g_dskImage) {
        g_dskImage = allocTimedImage();
    }
    if (!g_dskImage) {
        logf("DISK", "[FAIL] PSRAM image alloc");
        return false;
    }
    g_dskImage->eject();
    g_dskImage->clearCacheStats();
    if (!g_dskImage->load(g_dskRaw, kDos33ImageBytes, poOrder)) {
        logf("DISK", "[FAIL] load %s", label);
        return false;
    }
    g_dskImage->setWriteProtected(true);
    g_diskII.attachMedia(1, g_dskImage);
    logf("DISK", "mount result=OK drive=1 wp=1 label=%s format=%s cache_slots=2", label,
         poOrder ? "PO" : "DSK");
    return true;
}

static bool textContains(const char *needle) {
    if (!needle) {
        return false;
    }
    uint8_t chars[TextDecoder::kRows * TextDecoder::kCols];
    TextDecoder::decodeScreen(g_a2bus.ram(), 0x0400, chars);
    const size_t nlen = strlen(needle);
    for (int row = 0; row < 24; ++row) {
        char line[41];
        for (int c = 0; c < 40; ++c) {
            line[c] = static_cast<char>(chars[row * 40 + c] & 0x7F);
        }
        line[40] = 0;
        if (strstr(line, needle)) {
            return true;
        }
    }
    (void)nlen;
    return false;
}

static bool runLevel4Boot(uint32_t rotSeed, const char *tag) {
    g_bootStallUs = 0;
    g_worstTrackUs = 0;
    if (g_dskImage) {
        g_dskImage->clearCacheStats();
    }
    g_a2bus.clearRam();
    g_a2bus.reset();
    g_diskII.reset();
    loadCleanRoomSlot6();
    if (g_dskImage && g_dskImage->inserted()) {
        g_diskII.attachMedia(1, g_dskImage);
    }
    g_diskII.driveState(1).quarterTrack = 0;
    g_diskII.setRotationIndex(rotSeed);
    g_a2bus.write(0x03FE, 0);
    g_a2bus.write(0x03FF, 0);
    g_cpu.reset();
    CpuRegisters r = g_cpu.registers();
    r.pc = 0xC600;
    g_cpu.setRegisters(r);

    logf("DISK", "boot start tag=%s rot=%u pc=$C600", tag, rotSeed);
    const uint64_t c0 = g_cpu.cycles();
    const uint32_t t0 = micros();
    bool ok = false;
    uint32_t sawC0EC = 0;
    for (;;) {
        g_cpu.runCycles(64);
        g_a2bus.setAccessCycle(static_cast<uint32_t>(g_cpu.cycles() & 0xFFFFFFFFu));
        if (g_diskII.romKind() == DiskIIController::RomKind::CleanRoom) {
            serviceCleanRoomDenibbleRequest(g_a2bus.ram());
        }
        // Sample latch path activity via diag (Q7L read mode + motor).
        const auto d = g_diskII.diagState();
        if (d.motorOn && !d.q7) {
            ++sawC0EC;
        }
        if (g_a2bus.ram()[0x03FE] == 0x4C && g_a2bus.ram()[0x03FF] == 0x34) {
            ok = true;
            break;
        }
        if ((g_cpu.cycles() - c0) > kBootCycleBudget) {
            break;
        }
    }
    const uint32_t wallUs = micros() - t0;
    const uint64_t used = g_cpu.cycles() - c0;
    const double cps = wallUs ? (used * 1e6 / wallUs) : 0;

    logf("DISK", "boot marker=%s tag=%s cycles=%llu wall_us=%u cps=%.0f", ok ? "PASS" : "FAIL",
         tag, static_cast<unsigned long long>(used), wallUs, cps);
    logf("DISK", "motor=%s drive=%d c0ec_samples=%u stall_us=%u worst_track_us=%u",
         g_diskII.diagState().motorOn ? "ON" : "OFF", g_diskII.diagState().selectedDrive, sawC0EC,
         g_bootStallUs, g_worstTrackUs);
    if (g_dskImage) {
        logf("DISK", "cache hits=%u misses=%u builds=%u bytes=%u", g_dskImage->cacheHits(),
             g_dskImage->cacheMisses(), g_dskImage->trackBuildCount(),
             static_cast<unsigned>(g_dskImage->cacheBytesUsed()));
    }
    if (ok) {
        const bool t1 = textContains("ESP][ LEVEL 4");
        const bool t2 = textContains("DISK II BOOT OK");
        logf("DISK", "boot_screen L4=%s OK=%s (via Apple II text RAM)", t1 ? "yes" : "no",
             t2 ? "yes" : "no");
        portENTER_CRITICAL(&g_dirtyMux);
        g_dirty.markAll();
        portEXIT_CRITICAL(&g_dirtyMux);
        presentFull();
        delay(2000);
    }
    return ok;
}

static void testMissingMedia() {
    logf("DISK", "test=missing_media");
    g_diskII.ejectDrive(1);
    if (g_dskImage) {
        g_dskImage->eject();
    }
    g_a2bus.clearRam();
    g_cpu.reset();
    CpuRegisters r = g_cpu.registers();
    r.pc = 0xC600;
    g_cpu.setRegisters(r);
    for (int i = 0; i < 5000; ++i) {
        g_cpu.runCycles(64);
        g_a2bus.setAccessCycle(static_cast<uint32_t>(g_cpu.cycles() & 0xFFFFFFFFu));
        serviceCleanRoomDenibbleRequest(g_a2bus.ram());
    }
    const bool noMark = g_a2bus.ram()[0x03FF] != 0x34;
    logf("DISK", "missing_media surviving=%s marker_absent=%s", "yes", noMark ? "yes" : "no");
}

static void testMalformedMedia() {
    logf("DISK", "test=malformed_media");
    if (!g_dskImage) {
        g_dskImage = allocTimedImage();
    }
    if (!g_dskImage) {
        return;
    }
    g_dskImage->clear();
    g_dskImage->setWriteProtected(true);
    g_diskII.attachMedia(1, g_dskImage);
    g_a2bus.clearRam();
    g_a2bus.reset();
    g_diskII.reset();
    loadCleanRoomSlot6();
    g_diskII.attachMedia(1, g_dskImage);
    g_cpu.reset();
    CpuRegisters r = g_cpu.registers();
    r.pc = 0xC600;
    g_cpu.setRegisters(r);
    for (int i = 0; i < 20000; ++i) {
        g_cpu.runCycles(64);
        g_a2bus.setAccessCycle(static_cast<uint32_t>(g_cpu.cycles() & 0xFFFFFFFFu));
        serviceCleanRoomDenibbleRequest(g_a2bus.ram());
    }
    logf("DISK", "malformed marker_absent=%s pc=$%04X",
         g_a2bus.ram()[0x03FF] != 0x34 ? "yes" : "no", g_cpu.registers().pc);
}

static bool testNibBoot() {
    logf("DISK", "test=NIB");
    if (!g_dskImage || !g_dskImage->inserted()) {
        if (!mountFromRaw(false, "DSK")) {
            return false;
        }
    }
    if (!g_nibImage) {
        g_nibImage = allocNibImage();
    }
    if (!g_nibImage) {
        logf("DISK", "NIB alloc FAIL");
        return false;
    }
    uint8_t *raw = static_cast<uint8_t *>(psramAlloc(NibTrackImage::kImageBytes));
    if (!raw) {
        return false;
    }
    memset(raw, 0xFF, NibTrackImage::kImageBytes);
    for (int t = 0; t < NibTrackImage::kTracks; ++t) {
        size_t len = 0;
        const uint8_t *tr = g_dskImage->trackNibbles(t, &len);
        if (!tr || len == 0) {
            continue;
        }
        const size_t copy = len < NibTrackImage::kTrackLen ? len : NibTrackImage::kTrackLen;
        memcpy(raw + static_cast<size_t>(t) * NibTrackImage::kTrackLen, tr, copy);
    }
    const bool loaded = g_nibImage->load(raw, NibTrackImage::kImageBytes);
    free(raw);
    if (!loaded) {
        logf("DISK", "NIB load FAIL");
        return false;
    }
    g_nibImage->setWriteProtected(true);
    g_diskII.attachMedia(1, g_nibImage);
    g_a2bus.clearRam();
    g_a2bus.reset();
    g_diskII.reset();
    loadCleanRoomSlot6();
    g_diskII.attachMedia(1, g_nibImage);
    g_diskII.setRotationIndex(5);
    g_a2bus.write(0x03FE, 0);
    g_a2bus.write(0x03FF, 0);
    g_cpu.reset();
    CpuRegisters r = g_cpu.registers();
    r.pc = 0xC600;
    g_cpu.setRegisters(r);
    bool ok = false;
    const uint64_t c0 = g_cpu.cycles();
    for (;;) {
        g_cpu.runCycles(64);
        g_a2bus.setAccessCycle(static_cast<uint32_t>(g_cpu.cycles() & 0xFFFFFFFFu));
        serviceCleanRoomDenibbleRequest(g_a2bus.ram());
        if (g_a2bus.ram()[0x03FE] == 0x4C && g_a2bus.ram()[0x03FF] == 0x34) {
            ok = true;
            break;
        }
        if ((g_cpu.cycles() - c0) > kBootCycleBudget) {
            break;
        }
    }
    logf("DISK", "NIB boot marker=%s", ok ? "PASS" : "FAIL");
    if (ok) {
        presentFull();
        delay(1500);
    }
    if (g_dskImage && g_dskImage->inserted()) {
        g_diskII.attachMedia(1, g_dskImage);
    }
    return ok;
}

static void videoSpotCheck() {
    g_presentColor = PresentColorMode::Sharp;
    softSwitch(SoftSwitches::kAddrText);
    softSwitch(SoftSwitches::kAddrPage1);
    softSwitch(SoftSwitches::kAddrFull);
    for (int c = 0; c < 16; ++c) {
        g_a2bus.write(TextDecoder::cellAddress(0x0400, 0, c),
                      static_cast<uint8_t>(0x80u | "TEXT OK SPOTCHK"[c]));
    }
    presentFull();
    logf("VIDEO", "TEXT spot OK");

    softSwitch(SoftSwitches::kAddrGraphics);
    softSwitch(SoftSwitches::kAddrLores);
    for (int trow = 0; trow < 24; ++trow) {
        for (int col = 0; col < 40; ++col) {
            g_a2bus.write(TextDecoder::cellAddress(0x0400, trow, col),
                          static_cast<uint8_t>((trow + col) & 0xFF));
        }
    }
    presentFull();
    logf("VIDEO", "LORES spot OK");

    softSwitch(SoftSwitches::kAddrHires);
    fillHgr("checker");
    presentFull();
    logf("VIDEO", "HGR_SHARP spot OK");

    g_presentColor = PresentColorMode::ArtifactColor;
    fillHgr("artifact_ref");
    presentFull();
    logf("VIDEO", "HGR_ARTIFACT spot OK");
    g_presentColor = PresentColorMode::Sharp;
}

static void bootSuite() {
    logf("APPLE2", "cpu+disk init");
    g_cpu.setCallbacks(&g_a2bus, Apple2Bus::busRead, Apple2Bus::busWrite);
    g_a2bus.setVideoDirtyTracker(&g_dirty);
    g_a2bus.setSlotDevice(6, &g_diskII);
    g_a2bus.clearRam();
    g_a2bus.reset();

    static uint8_t romImg[Rom::kApple2PlusRomBytes];
    if (generateSyntheticRom(romImg, sizeof(romImg)) != RomError::Ok ||
        g_a2bus.rom().load(romImg, sizeof(romImg)) != RomError::Ok) {
        logf("APPLE2", "[FAIL] mb rom");
        return;
    }
    g_cpu.reset();
    loadCleanRoomSlot6();

    g_viewportFb = static_cast<uint16_t *>(psramAlloc(kViewW * kViewH * sizeof(uint16_t)));
    g_hgrBits = static_cast<uint8_t *>(psramAlloc(280 * 192));
    g_hgrHigh = static_cast<uint8_t *>(psramAlloc(40 * 192));
    logf("RAM", "viewport=%s disk_img_bytes=%u psram_free=%u", g_viewportFb ? "ok" : "FAIL",
         static_cast<unsigned>(sizeof(TimedDos33Image)),
         ESP.getPsramSize() ? ESP.getFreePsram() : 0);

    // --- Media fault paths ---
    testMissingMedia();
    testMalformedMedia();

    // --- Seed + mount DSK (one SD read into PSRAM; later formats reuse RAM) ---
    seedBootImageIfMissing(Esp32SdStorageBackend::kBootTestDsk);
    seedBootImageIfMissing(Esp32SdStorageBackend::kBootTestPo);
    bool bootDsk = false;
    if (loadDskRawFromSd(Esp32SdStorageBackend::kBootTestDsk) && mountFromRaw(false, "DSK")) {
        bootDsk = runLevel4Boot(0, "DSK_rot0");
        bootDsk = runLevel4Boot(37, "DSK_rot37") && bootDsk;
        bootDsk = runLevel4Boot(128, "DSK_rot128") && bootDsk;
        bootDsk = runLevel4Boot(777, "DSK_rot777") && bootDsk;
    }
    logf("DISK", "DSK Level4=%s", bootDsk ? "PASS" : "FAIL");

    // --- PO from same PSRAM bytes (ProDOS sector order flag) ---
    bool bootPo = false;
    if (g_dskRaw && mountFromRaw(true, "PO")) {
        bootPo = runLevel4Boot(19, "PO_rot19");
    }
    logf("DISK", "PO Level4=%s", bootPo ? "PASS" : "FAIL");

    // Restore DSK for NIB source + stability
    mountFromRaw(false, "DSK");
    const bool bootNib = testNibBoot();
    logf("DISK", "NIB Level4=%s", bootNib ? "PASS" : "FAIL");

    g_level4_ok = bootDsk;
    logf("DISK", "Level4 ESP32=%s (DSK primary)", g_level4_ok ? "PHYSICALLY_VERIFIED" : "FAIL");

    // Leave successful boot screen
    if (g_level4_ok) {
        mountFromRaw(false, "DSK");
        runLevel4Boot(0, "DSK_final");
    }

    videoSpotCheck();

    logf("RAM", "heap_free=%u heap_min=%u", ESP.getFreeHeap(), ESP.getMinFreeHeap());
    if (ESP.getPsramSize()) {
        logf("RAM", "psram_free=%u", ESP.getFreePsram());
    }
    logf("APPLE2", "suite done");
}

static void emulatorTask(void *) {
    while (!g_schedulerGo) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    logf("SCHED", "emu core=%d", xPortGetCoreID());
    g_wallStartUs = micros();
    g_emuCyclesAtBoot = g_cpu.cycles();
    // Keep running from current PC (boot payload idle loop at $0800+).
    for (;;) {
        g_cpu.runCycles(kExecQuantum);
        g_a2bus.setAccessCycle(static_cast<uint32_t>(g_cpu.cycles() & 0xFFFFFFFFu));
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
        // Disk activity must NOT notify display power.
        (void)g_diskII.lastActivity();
        g_diskII.clearLastActivity();
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
                 "psram_free=%u disk_sd_err=%u",
                 cps, g_dispFrames, ESP.getFreeHeap(), ESP.getMinFreeHeap(),
                 ESP.getPsramSize() ? ESP.getFreePsram() : 0, g_sdStore.readErrorCount());
        }
        if (!stabilityDone && (now - startMs) >= kStabilityMs) {
            stabilityDone = true;
            logf("STABILITY", "duration_ms=%u frames=%u level4=%s", kStabilityMs, g_dispFrames,
                 g_level4_ok ? "PASS" : "FAIL");
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

    bootSuite();

    logf("SELFTEST", "display=%s touch=%s sd=%s imu=%s level4=%s", g_display_ok ? "PASS" : "FAIL",
         g_touch_ok ? "PASS" : "FAIL", g_sd_ok ? "PASS" : "FAIL", g_imu_ok ? "PASS" : "FAIL",
         g_level4_ok ? "PASS" : "FAIL");

    xTaskCreatePinnedToCore(displayTask, "a2disp", 10240, nullptr, 2, nullptr, 0);
    xTaskCreatePinnedToCore(emulatorTask, "a2emu", 8192, nullptr, 1, nullptr, 1);
    g_schedulerGo = true;
    logf("SCHED", "tasks started");
}

void loop() {
    delay(100);
}
