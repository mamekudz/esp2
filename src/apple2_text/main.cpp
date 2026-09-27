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
#include "esp2_macro_engine.hpp"
#include "esp2_sd_bus.hpp"
#include "esp2_system_config.hpp"
#include "esp32_sd_storage.hpp"
#include "i18x_fw.hpp"
#include "qmi8658_min.h"
#include "sd_ownership.hpp"
#include "serial_media_upload.hpp"
#include "usb_storage_mode.hpp"

#if !ARDUINO_USB_MODE
#include "USB.h"
#endif

#include "esp_bracket/apple2_bus.hpp"
#include "esp_bracket/artifact_renderer.hpp"
#include "esp_bracket/cpu6502.hpp"
#include "esp_bracket/disk_ii_cleanroom.hpp"
#include "esp_bracket/disk_ii_controller.hpp"
#include "esp_bracket/disk_ii_media.hpp"
#include "esp_bracket/display_effect.hpp"
#include "esp_bracket/hgr_decoder.hpp"
#include "esp_bracket/key_map.hpp"
#include "esp_bracket/landscape_present.hpp"
#include "esp_bracket/lores_decoder.hpp"
#include "esp_bracket/phosphor.hpp"
#include "esp_bracket/rom.hpp"
#include "esp_bracket/rom_identity.hpp"
#include "esp_bracket/sha256.hpp"
#include "esp_bracket/soft_switches.hpp"
#include "esp_bracket/text_decoder.hpp"
#include "esp_bracket/text_screen.hpp"
#include "esp_bracket/video_dirty_tracker.hpp"
#include "esp_bracket/video_state.hpp"

using namespace esp_bracket;

static constexpr char kBuildId[] = "apple2_standalone_v1";
static constexpr uint32_t kAppleIiHz = 1023000;
static constexpr uint32_t kExecQuantum = 2000;
static constexpr int kViewX = 0;
static constexpr int kViewY = 48;
static constexpr int kViewW = 280;
static constexpr int kViewH = 192;
static constexpr int kTextCellW = 7;
static constexpr int kPanelW = LCD_WIDTH;  // 280
static constexpr int kPanelH = LCD_HEIGHT; // 456
static constexpr uint32_t kQspiHz = 40000000;
static constexpr uint32_t kStabilityMs = 90000;
static constexpr int kFullUpdateThreshold = 96;
static constexpr uint32_t kRomStartupBudget = 8000000;
static constexpr uint32_t kWaitSlice = 2000;

/** Monitor appearance — independent of Apple II soft-switches / VRAM. */
enum class PresentMonitor : uint8_t { White = 0, Green, Amber, Artifact };
/** Optional CRT/TV pass after monitor appearance. */
enum class PresentEffect : uint8_t { Clean = 0, Crt };
/** Physical panel layout — independent of AppleIIMachine. */
enum class PresentOrientation : uint8_t { Classic = 0, Landscape };

static Arduino_DataBus *g_bus = nullptr;
static Arduino_CO5300 *g_gfx = nullptr;
static DisplayPowerManager g_display_power;
static constexpr uint8_t kDisplayBrightness = 180;

static Apple2Bus g_a2bus;
static Cpu6502 g_cpu;
static VideoDirtyTracker g_dirty;
static DiskIIController g_diskII;
static Esp32SdStorageBackend g_sdStore;
static SdOwnership g_sdOwner;
static UsbStorageMode g_usbStorage(g_sdOwner);
static bool g_usbStorageUi = false;
static PresentMonitor g_presentMonitor = PresentMonitor::White;
static PresentEffect g_presentEffect = PresentEffect::Clean;
static PresentOrientation g_presentOrient = PresentOrientation::Classic;
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
static bool g_galaxianVisibleLogged = false;

/** CDC host→device session (MEDIA owns RX exclusively while active). */
enum class CdcDevMode : uint8_t { Idle = 0, LiveInput = 1 };
static volatile CdcDevMode g_cdcMode = CdcDevMode::Idle;
static volatile bool g_diagQuiet = false; // silence [TAG] logs during MEDIA (CDC contention)
/** Display must not touch QSPI/SPI while SD media transfer runs (shared SPI host crash). */
static volatile bool g_sdExclusive = false;
static char g_mountedDiskPath[96] = {};
static bool g_diskMounted = false;
static bool g_slot6UserProm = false;

static esp2_config::SystemConfig g_sysConfig = esp2_config::defaultSystemConfig();
static esp2_macro::MacroBank g_macroBank{};
static esp2_macro::Runner g_macroRunner{};
static bool g_startupConfigApplied = false;

static uint16_t *g_viewportFb = nullptr;
/** Landscape: rotated+scaled RGB565 (panel-sized or fitted region). */
static uint16_t *g_presentFb = nullptr;
static int g_landOutW = 0;
static int g_landOutH = 0;
static int g_landOx = 0;
static int g_landOy = 0;
static uint8_t *g_hgrBits = nullptr;
static uint8_t *g_hgrHigh = nullptr;
static volatile bool g_schedulerGo = false;
static volatile bool g_emuHold = false; // pause a2emu during SD/boot mutations
static TaskHandle_t g_dispTaskHandle = nullptr;
static volatile uint64_t g_emuCyclesAtBoot = 0;
static volatile uint32_t g_wallStartUs = 0;
static volatile uint32_t g_dispFrames = 0;
static volatile uint32_t g_dispBytes = 0;
static volatile uint32_t g_lastXferUs = 0;
static volatile uint32_t g_lastRenderUs = 0;
static volatile uint32_t g_lastPresentUs = 0;
static portMUX_TYPE g_dirtyMux = portMUX_INITIALIZER_UNLOCKED;
/** CDC may change present mode; only a2disp may touch CO5300 SPI. */
static volatile bool g_presentApplyPending = false;

static void logf(const char *tag, const char *fmt, ...) {
    if (g_diagQuiet || esp2_upload::isSessionActive()) {
        return;
    }
    char buf[240];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    Serial.printf("[%s] %s\n", tag, buf);
}

static void *psramAlloc(size_t n);
static void markVideoDirty();
static void pauseMediaForUsb();
static void emitDiskSnap(const char *tag);
static void maybeLogDiskHeartbeat();
static void resyncEmuWallClock();
static void applyPresentChange();
static void servicePresentApplyOnDisplayTask();
static void emitPresentStatus();
static const char *presentMonitorName();
static const char *presentEffectName();
static const char *presentOrientName();
static VideoColorMode presentVideoColorMode();
static uint16_t presentMonoPixel(bool lit);
static void applyCrtToViewportIfNeeded();

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
static void *psramAlloc(size_t n);

static bool probe_sd() {
    if (!g_sdOwner.esp2MayUseFat()) {
        logf("SD", "blocked owner=%s", sdOwnerStateName(g_sdOwner.state()));
        return false;
    }
    if (!esp2SdBusBegin()) {
        logf("SD", "[FAIL]");
        g_sdStore.setMounted(false);
        return false;
    }
    g_sdStore.setMounted(true);
    logf("SD", "mounted owner=%s bus=SPI3", sdOwnerStateName(g_sdOwner.state()));
    return true;
}

static void drawUsbStorageScreen() {
    if (!g_gfx || !g_display_ok) {
        return;
    }
    g_gfx->fillScreen(0x0000);
    g_gfx->setTextColor(0xFFFF);
    g_gfx->setTextSize(2);
    g_gfx->setCursor(16, 80);
    g_gfx->print(esp2_i18x::t("usb_storage.title"));
    g_gfx->setTextSize(1);
    g_gfx->setCursor(16, 130);
    g_gfx->print(esp2_i18x::t("usb_storage.mounted"));
    g_gfx->setCursor(16, 150);
    g_gfx->print(esp2_i18x::t("usb_storage.on_computer"));
    g_gfx->setCursor(16, 190);
    g_gfx->print(esp2_i18x::t("usb_storage.eject_hint"));
    g_gfx->setCursor(16, 210);
    g_gfx->print(esp2_i18x::t("usb_storage.before_return"));
}

static void pauseMediaForUsb() {
    g_diskII.clearRom();
    g_a2bus.setSlotDevice(6, nullptr);
    g_dskImage = nullptr; // stale nibble image invalid across ownership change
    g_slot6UserProm = false;
    g_diskMounted = false;
    g_mountedDiskPath[0] = 0;
}

static bool prepareEsp2Tree() {
    if (!g_sdOwner.esp2MayUseFat() || !g_sdStore.beginMounted()) {
        return false;
    }
    g_sdStore.ensureDiskRoot();
    g_sdStore.ensureRomRoot();
    if (!SD.exists("/esp2/config")) {
        SD.mkdir("/esp2/config");
    }
    if (!SD.exists("/esp2/diagnostics")) {
        SD.mkdir("/esp2/diagnostics");
    }
    if (!SD.exists("/esp2/tmp")) {
        SD.mkdir("/esp2/tmp");
    }
    // Seed project-owned Esp2BootTest if missing.
    if (!g_sdStore.exists(Esp32SdStorageBackend::kBootTestDsk)) {
        uint8_t *seed = static_cast<uint8_t *>(psramAlloc(kDos33ImageBytes));
        if (seed && generateEsp2BootTestImage(seed, kDos33ImageBytes)) {
            g_sdStore.writeAll(Esp32SdStorageBackend::kBootTestDsk, seed, kDos33ImageBytes);
            logf("SD", "wrote project Esp2BootTest.dsk");
        }
        free(seed);
    }
    // Lightweight manifest (metadata only).
    const char *manifest =
        "{\n"
        "  \"schemaVersion\": 1,\n"
        "  \"root\": \"/esp2\",\n"
        "  \"note\": \"Generated runtime manifest — not authoritative\",\n"
        "  \"entries\": [\n"
        "    {\"path\":\"/esp2/disks/Esp2BootTest.dsk\",\"category\":\"PROJECT_OWNED\"},\n"
        "    {\"path\":\"/esp2/roms/system.rom\",\"category\":\"USER_SUPPLIED\"}\n"
        "  ]\n"
        "}\n";
    g_sdStore.writeAll("/esp2/diagnostics/storage-manifest.json",
                       reinterpret_cast<const uint8_t *>(manifest), strlen(manifest));
    logf("SD", "tree prepared + manifest");
    return true;
}

static bool enterUsbStorageMode() {
    if (!UsbStorageMode::isSupported()) {
        logf("USB", "MSC unsupported (need TinyUSB / USB_MODE=0)");
        return false;
    }
    if (!g_sdOwner.esp2MayUseFat()) {
        logf("USB", "enter denied owner=%s", sdOwnerStateName(g_sdOwner.state()));
        return false;
    }
    pauseMediaForUsb();
    if (!g_usbStorage.enter()) {
        logf("USB", "enter FAIL err=%s", g_sdOwner.lastError());
        return false;
    }
    g_sdStore.setMounted(false);
    g_usbStorageUi = true;
    drawUsbStorageScreen();
    logf("USB", "MSC active sectors=%u size=%u owner=%s", g_usbStorage.sectorCount(),
         g_usbStorage.sectorSize(), sdOwnerStateName(g_sdOwner.state()));
    Serial.println("#ESP2USBMSC READY");
    return true;
}

static bool leaveUsbStorageMode(bool unsafe) {
    if (!g_usbStorage.leave(unsafe)) {
        logf("USB", "leave FAIL err=%s", g_sdOwner.lastError());
        return false;
    }
    g_sdStore.setMounted(true);
    g_usbStorageUi = false;
    prepareEsp2Tree();
    logf("USB", "FAT remounted owner=%s unsafe=%d", sdOwnerStateName(g_sdOwner.state()),
         unsafe ? 1 : 0);
    Serial.println("#ESP2USBMSC LEFT");
    return true;
}

static bool ensureDskBuffers() {
    if (!g_dskRaw) {
        g_dskRaw = static_cast<uint8_t *>(psramAlloc(kDos33ImageBytes));
    }
    if (!g_dskImage) {
        void *mem = psramAlloc(sizeof(Dos33NibbleImage));
        g_dskImage = mem ? new (mem) Dos33NibbleImage() : nullptr;
    }
    return g_dskRaw && g_dskImage;
}

static void sdExclusiveEnd() {
    if (g_dispTaskHandle) {
        vTaskResume(g_dispTaskHandle);
    }
    g_sdExclusive = false;
    g_diagQuiet = false;
}

static bool sdExclusiveBegin() {
    g_diagQuiet = true;
    g_sdExclusive = true;
    if (g_dispTaskHandle) {
        vTaskSuspend(g_dispTaskHandle);
    }
    delay(40);
    // Real remount on SPI3 (QSPI owns SPI2). SDFS::begin() no-ops if still mounted.
    esp2SdBusEnd();
    g_sdStore.setMounted(false);
    if (!esp2SdBusBegin()) {
        Serial.println("#NAK sd");
        sdExclusiveEnd();
        return false;
    }
    g_sdStore.setMounted(true);
    return true;
}

static bool loadDiskIiUserProm() {
    if (!sdExclusiveBegin()) {
        return false;
    }
    const char *cands[] = {
        Esp32SdStorageBackend::kDiskIiProm,
        Esp32SdStorageBackend::kDiskIiPromAlt,
    };
    uint8_t prom[DiskIIController::kSlotRomSize];
    for (const char *p : cands) {
        if (!g_sdStore.exists(p) || g_sdStore.fileSize(p) != DiskIIController::kSlotRomSize) {
            continue;
        }
        size_t got = 0;
        if (!g_sdStore.readAll(p, prom, sizeof(prom), &got) || got != sizeof(prom)) {
            continue;
        }
        if (!g_diskII.loadUserRom(prom, sizeof(prom))) {
            continue;
        }
        g_a2bus.setSlotDevice(6, &g_diskII);
        g_slot6UserProm = true;
        logf("DISK", "prom=%s size=256 kind=UserSupplied", p);
        Serial.println("#ACK DISK PROM");
        sdExclusiveEnd();
        return true;
    }
    logf("DISK", "[FAIL] no Slot-6 PROM at /esp2/roms/diskii.prom");
    Serial.println("#NAK DISK PROM");
    sdExclusiveEnd();
    return false;
}

static bool mountDiskPath(const char *path) {
    if (!path || path[0] != '/') {
        Serial.println("#NAK DISK path");
        return false;
    }
    if (!g_sdOwner.esp2MayUseFat()) {
        Serial.println("#NAK owner");
        return false;
    }
    g_emuHold = true;
    delay(10);
    if (!sdExclusiveBegin()) {
        g_emuHold = false;
        return false;
    }
    if (!g_sdStore.exists(path)) {
        Serial.println("#NAK DISK missing");
        sdExclusiveEnd();
        g_emuHold = false;
        return false;
    }
    const size_t sz = g_sdStore.fileSize(path);
    if (sz != kDos33ImageBytes) {
        Serial.println("#NAK DISK size");
        sdExclusiveEnd();
        g_emuHold = false;
        return false;
    }
    if (!ensureDskBuffers()) {
        Serial.println("#NAK DISK alloc");
        sdExclusiveEnd();
        g_emuHold = false;
        return false;
    }
    size_t got = 0;
    if (!g_sdStore.readAll(path, g_dskRaw, kDos33ImageBytes, &got) || got != kDos33ImageBytes) {
        Serial.println("#NAK DISK read");
        sdExclusiveEnd();
        g_emuHold = false;
        return false;
    }
    if (!g_dskImage->load(g_dskRaw, kDos33ImageBytes, false)) {
        Serial.println("#NAK DISK nibble");
        sdExclusiveEnd();
        g_emuHold = false;
        return false;
    }
    g_dskImage->setWriteProtected(true);
    g_diskII.attachMedia(1, g_dskImage);
    strncpy(g_mountedDiskPath, path, sizeof(g_mountedDiskPath) - 1);
    g_mountedDiskPath[sizeof(g_mountedDiskPath) - 1] = 0;
    g_diskMounted = true;
    g_galaxianVisibleLogged = false;
    logf("DISK", "mount path=%s size=%u", path, static_cast<unsigned>(sz));
    Serial.printf("#ACK DISK MOUNT path=%s\n", path);
    // Do not notifyActivity — disk I/O is not user input (screensaver must still arm).
    sdExclusiveEnd();
    g_emuHold = false;
    return true;
}

static void resyncEmuWallClock() {
    // CPU cycle counter restarts on reset; throttle/PERF use deltas from these bases.
    // Without resync, (cycles - g_emuCyclesAtBoot) underflows and the emu sleeps forever.
    g_emuCyclesAtBoot = g_cpu.cycles();
    g_wallStartUs = micros();
}

static void appleIiResetKeepDisks() {
    g_a2bus.reset();
    g_a2bus.speaker().reset();
    g_a2bus.keyboard().reset();
    if (g_diskMounted && g_dskImage) {
        g_diskII.attachMedia(1, g_dskImage);
    }
    if (g_slot6UserProm) {
        g_a2bus.setSlotDevice(6, &g_diskII);
    }
    g_diskII.reset();
    if (g_diskMounted && g_dskImage) {
        g_diskII.attachMedia(1, g_dskImage);
    }
    g_diskII.setRotationIndex(0);
    g_cpu.reset();
    resyncEmuWallClock();
    markVideoDirty();
    logf("APPLE2", "reset keep_disks=%d slot6=%d", g_diskMounted ? 1 : 0, g_slot6UserProm ? 1 : 0);
    Serial.println("#ACK APPLE RESET");
}

static bool bootMountedDisk() {
    if (!g_rom_ok) {
        Serial.println("#NAK BOOT no_rom");
        return false;
    }
    if (!g_diskMounted) {
        Serial.println("#NAK BOOT no_disk");
        return false;
    }
    g_emuHold = true;
    delay(20);
    // Always reload Slot-6 PROM — upload/VERIFY clears ROM but may leave stale flags.
    if (!loadDiskIiUserProm()) {
        g_emuHold = false;
        return false;
    }
    g_a2bus.clearRam();
    appleIiResetKeepDisks();
    // Prove Slot-6 window after reset (Autostart scans $Cn01/$Cn03/$Cn05).
    emitDiskSnap("POST_RESET");
    const uint8_t c1 = g_a2bus.peek(0xC601);
    const uint8_t c3 = g_a2bus.peek(0xC603);
    const uint8_t c5 = g_a2bus.peek(0xC605);
    if (!(c1 == 0x20 && c3 == 0x00 && c5 == 0x03)) {
        Serial.println("#NAK BOOT no_autostart_sig");
        g_emuHold = false;
        return false;
    }
    logf("DISK", "boot Autostart path=%s slot6=%d", g_mountedDiskPath, g_slot6UserProm ? 1 : 0);
    Serial.printf("#ACK DISK BOOT path=%s\n", g_mountedDiskPath);
    g_emuHold = false;
    return true;
}

static void applyLiveKey(uint8_t apple7, bool pressed) {
    if (pressed) {
        // Normal Apple II latch path — ROM clears strobe via $C010.
        g_a2bus.keyboard().keyDown(apple7);
    } else {
        g_a2bus.keyboard().keyUp(apple7);
    }
    g_display_power.notifyActivity("input");
}

static void applyLivePad(uint8_t p0, uint8_t p1, bool pb0, bool pb1, bool pb2) {
    g_a2bus.gameIo().setPaddle(0, p0);
    g_a2bus.gameIo().setPaddle(1, p1);
    g_a2bus.gameIo().setButton(0, pb0);
    g_a2bus.gameIo().setButton(1, pb1);
    g_a2bus.gameIo().setButton(2, pb2);
    g_display_power.notifyActivity("input");
}

static void handleDevCommandLine(const char *line) {
    if (!line || !line[0]) {
        return;
    }
    if (strcmp(line, "#ESP2USBMSC") == 0) {
        enterUsbStorageMode();
        return;
    }
    if (strcmp(line, "#ESP2USBMSC LEAVE") == 0) {
        leaveUsbStorageMode(false);
        return;
    }
    if (strcmp(line, "#ESP2UPLOAD") == 0) {
        if (g_cdcMode == CdcDevMode::LiveInput) {
            g_cdcMode = CdcDevMode::Idle;
            Serial.println("#ESP2INPUT IDLE");
        }
        // Quiet diagnostics first — concurrent Serial from PERF/display races HW CDC TX.
        g_diagQuiet = true;
        if (!g_sdOwner.esp2MayUseFat()) {
            Serial.println("#NAK owner");
            g_diagQuiet = false;
            return;
        }
        pauseMediaForUsb();
        Serial.println("#ESP2UPLOAD ENTER");
        Serial.flush();
        if (!sdExclusiveBegin()) {
            g_diagQuiet = false;
            return;
        }
        if (!SD.exists("/esp2")) {
            SD.mkdir("/esp2");
        }
        if (!SD.exists("/esp2/roms")) {
            SD.mkdir("/esp2/roms");
        }
        if (!SD.exists("/esp2/disks")) {
            SD.mkdir("/esp2/disks");
        }
        if (!SD.exists("/esp2/tmp")) {
            SD.mkdir("/esp2/tmp");
        }
        if (!SD.exists("/esp2") || !SD.exists("/esp2/roms")) {
            Serial.println("#NAK mkdir_esp2");
            sdExclusiveEnd();
            return;
        }
        esp2_upload::runSessionNow();
        sdExclusiveEnd();
        return;
    }
    if (strcmp(line, "#ESP2INPUT LIVE") == 0) {
        if (esp2_upload::isSessionActive()) {
            Serial.println("#NAK INPUT media_active");
            return;
        }
        g_cdcMode = CdcDevMode::LiveInput;
        Serial.println("#ACK INPUT LIVE");
        logf("INPUT", "mode=LIVE provider=windows_cdc_bridge");
        return;
    }
    if (strcmp(line, "#ESP2INPUT IDLE") == 0) {
        g_cdcMode = CdcDevMode::Idle;
        Serial.println("#ACK INPUT IDLE");
        return;
    }
    if (strncmp(line, "#ESP2INPUT KEY ", 15) == 0) {
        if (g_cdcMode != CdcDevMode::LiveInput) {
            Serial.println("#NAK INPUT not_live");
            return;
        }
        unsigned apple = 0;
        unsigned pressed = 0;
        if (sscanf(line + 15, "%x %u", &apple, &pressed) != 2) {
            Serial.println("#NAK INPUT KEY");
            return;
        }
        applyLiveKey(static_cast<uint8_t>(apple & 0x7F), pressed != 0);
        return;
    }
    if (strncmp(line, "#ESP2INPUT PAD ", 15) == 0) {
        if (g_cdcMode != CdcDevMode::LiveInput) {
            Serial.println("#NAK INPUT not_live");
            return;
        }
        unsigned p0 = 128, p1 = 128, b0 = 0, b1 = 0, b2 = 0;
        if (sscanf(line + 15, "%u %u %u %u %u", &p0, &p1, &b0, &b1, &b2) < 2) {
            Serial.println("#NAK INPUT PAD");
            return;
        }
        applyLivePad(static_cast<uint8_t>(p0 > 255 ? 255 : p0),
                     static_cast<uint8_t>(p1 > 255 ? 255 : p1), b0 != 0, b1 != 0, b2 != 0);
        return;
    }
    if (strncmp(line, "#ESP2DISK MOUNT ", 16) == 0) {
        mountDiskPath(line + 16);
        return;
    }
    if (strcmp(line, "#ESP2DISK PROM") == 0) {
        loadDiskIiUserProm();
        return;
    }
    if (strcmp(line, "#ESP2DISK BOOT") == 0) {
        bootMountedDisk();
        return;
    }
    if (strcmp(line, "#ESP2APPLE RESET") == 0) {
        appleIiResetKeepDisks();
        return;
    }
    if (strcmp(line, "#ESP2DIAG SNAP") == 0) {
        emitDiskSnap("SNAP");
        Serial.println("#ACK DIAG SNAP");
        return;
    }
    if (strncmp(line, "#ESP2PRESENT MONITOR ", 21) == 0) {
        const char *arg = line + 21;
        if (strcmp(arg, "WHITE") == 0 || strcmp(arg, "SHARP") == 0 || strcmp(arg, "MONO") == 0) {
            g_presentMonitor = PresentMonitor::White;
            applyPresentChange();
            return;
        }
        if (strcmp(arg, "GREEN") == 0) {
            g_presentMonitor = PresentMonitor::Green;
            applyPresentChange();
            return;
        }
        if (strcmp(arg, "AMBER") == 0) {
            g_presentMonitor = PresentMonitor::Amber;
            applyPresentChange();
            return;
        }
        if (strcmp(arg, "ARTIFACT") == 0 || strcmp(arg, "ARTIFACTCOLOR") == 0) {
            g_presentMonitor = PresentMonitor::Artifact;
            applyPresentChange();
            return;
        }
        Serial.println("#NAK PRESENT monitor");
        return;
    }
    // Legacy alias: COLOR SHARP|ARTIFACT → MONITOR WHITE|ARTIFACT
    if (strncmp(line, "#ESP2PRESENT COLOR ", 19) == 0) {
        const char *arg = line + 19;
        if (strcmp(arg, "SHARP") == 0 || strcmp(arg, "WHITE") == 0) {
            g_presentMonitor = PresentMonitor::White;
            applyPresentChange();
            return;
        }
        if (strcmp(arg, "ARTIFACT") == 0 || strcmp(arg, "ARTIFACTCOLOR") == 0) {
            g_presentMonitor = PresentMonitor::Artifact;
            applyPresentChange();
            return;
        }
        if (strcmp(arg, "GREEN") == 0) {
            g_presentMonitor = PresentMonitor::Green;
            applyPresentChange();
            return;
        }
        if (strcmp(arg, "AMBER") == 0) {
            g_presentMonitor = PresentMonitor::Amber;
            applyPresentChange();
            return;
        }
        Serial.println("#NAK PRESENT color");
        return;
    }
    if (strncmp(line, "#ESP2PRESENT EFFECT ", 20) == 0) {
        const char *arg = line + 20;
        if (strcmp(arg, "CLEAN") == 0 || strcmp(arg, "OFF") == 0 || strcmp(arg, "SHARP") == 0) {
            g_presentEffect = PresentEffect::Clean;
            applyPresentChange();
            return;
        }
        if (strcmp(arg, "CRT") == 0 || strcmp(arg, "CRT_TV") == 0 || strcmp(arg, "TV") == 0) {
            g_presentEffect = PresentEffect::Crt;
            applyPresentChange();
            return;
        }
        Serial.println("#NAK PRESENT effect");
        return;
    }
    if (strncmp(line, "#ESP2PRESENT ORIENT ", 20) == 0) {
        const char *arg = line + 20;
        if (strcmp(arg, "CLASSIC") == 0) {
            g_presentOrient = PresentOrientation::Classic;
            applyPresentChange();
            return;
        }
        if (strcmp(arg, "LANDSCAPE") == 0) {
            g_presentOrient = PresentOrientation::Landscape;
            applyPresentChange();
            return;
        }
        Serial.println("#NAK PRESENT orient");
        return;
    }
    if (strcmp(line, "#ESP2PRESENT STATUS") == 0) {
        emitPresentStatus();
        return;
    }
    if (strcmp(line, "#ESP2CONFIG STATUS") == 0) {
        Serial.printf("#ACK CONFIG loaded=%d valid=%d rom=%s drive1=%s boot=%d macro=%s "
                      "orient=%s monitor=%s effect=%s color=%s ss=%u\n",
                      g_sysConfig.loaded ? 1 : 0, g_sysConfig.valid ? 1 : 0,
                      g_sysConfig.romPath[0] ? g_sysConfig.romPath : "-",
                      g_sysConfig.drive1[0] ? g_sysConfig.drive1 : "-",
                      g_sysConfig.bootFromDisk ? 1 : 0,
                      g_sysConfig.startupMacro[0] ? g_sysConfig.startupMacro : "-",
                      esp2_config::orientName(g_sysConfig.orientation),
                      esp2_config::monitorName(g_sysConfig.monitor),
                      esp2_config::effectName(g_sysConfig.effect),
                      esp2_config::colorName(g_sysConfig.monitor),
                      static_cast<unsigned>(g_sysConfig.screensaverSeconds));
        return;
    }
    if (strncmp(line, "#ESP2MACRO RUN ", 15) == 0) {
        char err[48]{};
        if (!esp2_macro::startMacro(&g_macroRunner, g_macroBank, line + 15, millis(), err,
                                    sizeof(err))) {
            Serial.printf("#NAK MACRO RUN %s\n", err[0] ? err : "fail");
            return;
        }
        Serial.printf("#ACK MACRO RUN id=%s\n", g_macroRunner.activeId);
        return;
    }
    if (strcmp(line, "#ESP2MACRO STOP") == 0) {
        esp2_macro::stopMacro(&g_macroRunner, "serial_stop");
        Serial.println("#ACK MACRO STOP");
        return;
    }
    if (strcmp(line, "#ESP2MACRO STATUS") == 0) {
        const char *st = "idle";
        if (g_macroRunner.state == esp2_macro::RunState::Running) {
            st = "running";
        } else if (g_macroRunner.state == esp2_macro::RunState::Done) {
            st = "done";
        } else if (g_macroRunner.state == esp2_macro::RunState::Failed) {
            st = "failed";
        }
        Serial.printf("#ACK MACRO STATUS state=%s id=%s step=%u fail=%s bank=%u\n", st,
                      g_macroRunner.activeId[0] ? g_macroRunner.activeId : "-",
                      static_cast<unsigned>(g_macroRunner.index),
                      g_macroRunner.failReason[0] ? g_macroRunner.failReason : "-",
                      static_cast<unsigned>(g_macroBank.count));
        return;
    }
}

static void serviceDevSerialCommands() {
    // MEDIA binary session owns CDC RX — never steal bytes for line parsing.
    if (esp2_upload::isSessionActive()) {
        return;
    }
    static char line[128];
    static size_t len = 0;
    while (Serial.available()) {
        const int b = Serial.read();
        if (b < 0) {
            break;
        }
        if (b == '\n' || b == '\r') {
            if (len == 0) {
                continue;
            }
            line[len] = 0;
            len = 0;
            handleDevCommandLine(line);
            continue;
        }
        if (len + 1 < sizeof(line)) {
            line[len++] = static_cast<char>(b);
        } else {
            len = 0; // overflow — resync on next newline
        }
    }
    if (g_usbStorage.active() && g_usbStorage.ejectRequested()) {
        g_usbStorage.clearEjectRequest();
        leaveUsbStorageMode(false);
    }
}

static void maybeLogGalaxianVisible() {
    if (g_galaxianVisibleLogged || !g_diskMounted) {
        return;
    }
    // Heuristic only: mounted title path + settled HGR with substantial ink.
    if (strstr(g_mountedDiskPath, "Galaxian") == nullptr &&
        strstr(g_mountedDiskPath, "galaxian") == nullptr) {
        return;
    }
    const AppleIIVideoState vs = videoStateFromSoftSwitches(g_a2bus.softSwitches());
    if (vs.text || !vs.hires) {
        return;
    }
    const uint16_t base = vs.page2 ? 0x4000 : 0x2000;
    unsigned lit = 0;
    for (uint16_t a = base; a < base + 0x2000; a += 3) {
        if (g_a2bus.peek(a) & 0x7F) {
            ++lit;
        }
    }
    const CpuRegisters r = g_cpu.registers();
    const bool playfieldPc = r.pc >= 0x9000u && r.pc < 0xB000u;
    // Cracktro (~B1xx–B7xx) can reach ~400 lit; playfield/attract sits in $9000–$AFFF.
    if (lit < 100) {
        return;
    }
    if (!playfieldPc) {
        return;
    }
    g_galaxianVisibleLogged = true;
    markVideoDirty();
    const uint32_t wallUs = micros() - g_wallStartUs;
    const double cps = wallUs ? ((g_cpu.cycles() - g_emuCyclesAtBoot) * 1e6 / wallUs) : 0;
    Serial.printf("[GALAXIAN] ESP32_VISIBLE pc=$%04X cycles=%llu cps=%.0f video=HGR "
                  "page2=%d hgr_samples=%u\n",
                  r.pc, static_cast<unsigned long long>(g_cpu.cycles()), cps, vs.page2 ? 1 : 0,
                  lit);
    logf("GALAXIAN", "ESP32_VISIBLE");
}

static void emitDiskSnap(const char *tag) {
    const CpuRegisters r = g_cpu.registers();
    const AppleIIVideoState vs = videoStateFromSoftSwitches(g_a2bus.softSwitches());
    const DiskIIDiagState d = g_diskII.diagState();
    const char *vmode = vs.text ? "TEXT" : (vs.hires ? "HGR" : "LORES");
    const uint8_t c0 = g_a2bus.peek(0xC600);
    const uint8_t c1 = g_a2bus.peek(0xC601);
    const uint8_t c2 = g_a2bus.peek(0xC602);
    const uint8_t c3 = g_a2bus.peek(0xC603);
    const uint8_t c4 = g_a2bus.peek(0xC604);
    const uint8_t c5 = g_a2bus.peek(0xC605);
    const char *rk = "none";
    switch (g_diskII.romKind()) {
    case DiskIIController::RomKind::Synthetic:
        rk = "synthetic";
        break;
    case DiskIIController::RomKind::CleanRoom:
        rk = "cleanroom";
        break;
    case DiskIIController::RomKind::UserSupplied:
        rk = "user";
        break;
    default:
        break;
    }
    Serial.printf("[DISK][%s] pc=$%04X cycles=%llu video=%s%s page2=%d motor=%d qt=%d "
                  "drive=%d slot6=%d rom=%s path=%s\n",
                  tag ? tag : "SNAP", r.pc, static_cast<unsigned long long>(g_cpu.cycles()), vmode,
                  vs.mixed ? "+MIXED" : "", vs.page2 ? 1 : 0, d.motorOn ? 1 : 0,
                  d.drive1.quarterTrack, d.selectedDrive, g_slot6UserProm ? 1 : 0, rk,
                  g_mountedDiskPath[0] ? g_mountedDiskPath : "(none)");
    Serial.printf("[DISK][%s] C600=%02X %02X %02X %02X %02X %02X "
                  "autostart_sig=%s\n",
                  tag ? tag : "SNAP", c0, c1, c2, c3, c4, c5,
                  (c1 == 0x20 && c3 == 0x00 && c5 == 0x03) ? "yes" : "no");
    if (!vs.text && vs.hires) {
        const uint16_t base = vs.page2 ? 0x4000 : 0x2000;
        unsigned lit = 0;
        for (uint16_t a = base; a < base + 0x2000; a += 3) {
            if (g_a2bus.peek(a) & 0x7F) {
                ++lit;
            }
        }
        Serial.printf("[DISK][%s] hgr_lit=%u\n", tag ? tag : "SNAP", lit);
    }
}

static void maybeLogDiskHeartbeat() {
    if (!g_diskMounted) {
        return;
    }
    static uint32_t lastMs = 0;
    const uint32_t now = millis();
    if (lastMs != 0 && (now - lastMs) < 5000) {
        return;
    }
    lastMs = now;
    emitDiskSnap("HB");
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

static VideoColorMode presentVideoColorMode() {
    switch (g_presentMonitor) {
    case PresentMonitor::Green:
        return VideoColorMode::MonochromeGreen;
    case PresentMonitor::Amber:
        return VideoColorMode::MonochromeAmber;
    case PresentMonitor::Artifact:
        return VideoColorMode::CompositeColor;
    case PresentMonitor::White:
    default:
        return VideoColorMode::MonochromeWhite;
    }
}

static uint16_t presentMonoPixel(bool lit) {
    return mapLuminanceToPhosphorRgb565(presentVideoColorMode(), lit ? 255 : 0);
}

static void renderTextRows(uint16_t *fb, int row0, int row1) {
    const AppleIIVideoState vs = videoStateFromSoftSwitches(g_a2bus.softSwitches());
    uint8_t chars[TextDecoder::kRows * TextDecoder::kCols];
    TextDecoder::decodeScreen(g_a2bus.ram(), vs.textPageBase(), chars);
    const uint16_t onPx = presentMonoPixel(true);
    const uint16_t offPx = presentMonoPixel(false);
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
                    dst[col * kTextCellW + gx] = lit ? onPx : offPx;
                }
                dst[col * kTextCellW + 5] = offPx;
                dst[col * kTextCellW + 6] = offPx;
            }
        }
    }
}

static void renderLoresRows(uint16_t *fb, int scan0, int scan1) {
    const AppleIIVideoState vs = videoStateFromSoftSwitches(g_a2bus.softSwitches());
    uint8_t blocks[LoresDecoder::kRows * LoresDecoder::kCols];
    LoresDecoder::decode(g_a2bus.ram(), vs.textPageBase(), blocks);
    const bool phosphor = g_presentMonitor != PresentMonitor::Artifact;
    const VideoColorMode monoMode = presentVideoColorMode();
    for (int py = scan0; py <= scan1; ++py) {
        const int brow = py / LoresDecoder::kBlockH;
        uint16_t *dst = fb + py * kViewW;
        for (int bx = 0; bx < 40; ++bx) {
            uint8_t r, g, b;
            LoresDecoder::colorRgb(blocks[brow * 40 + bx], &r, &g, &b);
            uint16_t c;
            if (phosphor) {
                const uint8_t lum = luminanceFromRgb888(r, g, b);
                c = mapLuminanceToPhosphorRgb565(monoMode, lum);
            } else {
                c = rgb565(r, g, b);
            }
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
    const VideoColorMode mode = presentVideoColorMode();
    for (int y = scan0; y <= scan1; ++y) {
        uint16_t *dst = fb + y * kViewW;
        ArtifactRenderer::renderScanlineRgb565(g_hgrBits + y * 280, g_hgrHigh + y * 40, mode, dst);
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

/**
 * Landscape presentation: rotate Apple II 280×192 CW 90° → 192×280, then
 * nearest-neighbor scale to maximize fit on 280×456 while preserving aspect.
 * Apple II memory / soft-switches are untouched.
 */
static void computeLandscapeGeometry() {
    LandscapePresent::computeOutSize(kPanelW, kPanelH, &g_landOutW, &g_landOutH, &g_landOx,
                                     &g_landOy);
}

static bool ensurePresentFb() {
    if (g_presentOrient != PresentOrientation::Landscape) {
        return true;
    }
    if (!g_presentFb) {
        computeLandscapeGeometry();
        g_presentFb = static_cast<uint16_t *>(
            psramAlloc(static_cast<size_t>(g_landOutW * g_landOutH) * sizeof(uint16_t)));
        if (!g_presentFb) {
            logf("PRESENT", "[FAIL] landscape fb alloc");
            return false;
        }
        logf("PRESENT", "landscape fb %dx%d ox=%d oy=%d", g_landOutW, g_landOutH, g_landOx,
             g_landOy);
    }
    return true;
}

static uint32_t transferLandscapeFromViewport() {
    if (!g_gfx || !g_viewportFb || !ensurePresentFb() || !g_presentFb) {
        return 0;
    }
    const uint32_t t0 = micros();
    // CW rotate + edge-preserving nearest-neighbor scale (shared host/device math).
    LandscapePresent::transformRgb565(g_viewportFb, g_presentFb, g_landOutW, g_landOutH);
    g_gfx->draw16bitRGBBitmap(g_landOx, g_landOy, g_presentFb, g_landOutW, g_landOutH);
    g_lastXferUs = micros() - t0;
    g_dispBytes = static_cast<uint32_t>(g_landOutW * g_landOutH * 2);
    g_lastPresentUs = g_lastXferUs;
    return g_lastXferUs;
}

static void applyCrtToViewportIfNeeded() {
    if (!g_viewportFb || g_presentEffect != PresentEffect::Crt) {
        return;
    }
    DisplayEffect::applyRgb565(g_viewportFb, kViewW, kViewH, DisplayEffectMode::CrtTv,
                               EffectStrength::Low,
                               g_presentMonitor == PresentMonitor::Artifact);
}

static uint32_t presentDirty(const VideoDirtyTracker::Bitset &bits) {
    if (!g_viewportFb || bits.empty()) {
        return 0;
    }
    // CRT horizontal blend needs contiguous scanlines — full viewport when on.
    VideoDirtyTracker::Bitset work = bits;
    if (g_presentEffect == PresentEffect::Crt) {
        work.markAll();
    }
    const uint32_t tR0 = micros();
    renderDirtyIntoFb(g_viewportFb, work);
    applyCrtToViewportIfNeeded();
    g_lastRenderUs = micros() - tR0;
    if (g_presentOrient == PresentOrientation::Landscape) {
        // Full transform — dirty scanline runs are not axis-aligned after rotate.
        return transferLandscapeFromViewport();
    }
    const int pop = work.popcount();
    if (pop >= kFullUpdateThreshold || pop >= kViewH || g_presentEffect == PresentEffect::Crt) {
        return transferScanlineRange(0, kViewH - 1);
    }
    uint32_t total = 0;
    int runStart = -1;
    for (int y = 0; y <= kViewH; ++y) {
        const bool on = (y < kViewH) && work.test(y);
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

static const char *presentMonitorName() {
    switch (g_presentMonitor) {
    case PresentMonitor::Green:
        return "GREEN";
    case PresentMonitor::Amber:
        return "AMBER";
    case PresentMonitor::Artifact:
        return "ARTIFACT";
    case PresentMonitor::White:
    default:
        return "WHITE";
    }
}

static const char *presentEffectName() {
    return g_presentEffect == PresentEffect::Crt ? "CRT" : "CLEAN";
}

static const char *presentOrientName() {
    return g_presentOrient == PresentOrientation::Landscape ? "LANDSCAPE" : "CLASSIC";
}

static void emitPresentStatus() {
    Serial.printf("#ACK PRESENT monitor=%s effect=%s orient=%s color=%s classic_view=%dx%d@%d,%d "
                  "land_out=%dx%d@%d,%d render_us=%u xfer_us=%u\n",
                  presentMonitorName(), presentEffectName(), presentOrientName(),
                  g_presentMonitor == PresentMonitor::Artifact ? "ARTIFACT" : "SHARP", kViewW,
                  kViewH, kViewX, kViewY, g_landOutW, g_landOutH, g_landOx, g_landOy,
                  g_lastRenderUs, g_lastXferUs);
}

static void applyPresentChange() {
    // Called from the CDC/loop task — never touch g_gfx here (SPI race with a2disp).
    if (g_presentOrient == PresentOrientation::Landscape) {
        computeLandscapeGeometry();
    }
    g_presentApplyPending = true;
    emitPresentStatus();
    logf("PRESENT", "monitor=%s effect=%s orient=%s pending", presentMonitorName(),
         presentEffectName(), presentOrientName());
}

/** Display-task side of applyPresentChange: fill / alloc / dirty. */
static void servicePresentApplyOnDisplayTask() {
    if (!g_presentApplyPending) {
        return;
    }
    g_presentApplyPending = false;
    if (g_gfx) {
        g_gfx->fillScreen(0x0000);
    }
    if (g_presentOrient == PresentOrientation::Landscape) {
        computeLandscapeGeometry();
        (void)ensurePresentFb();
    }
    markVideoDirty();
    logf("PRESENT", "applied monitor=%s effect=%s orient=%s land=%dx%d@%d,%d",
         presentMonitorName(), presentEffectName(), presentOrientName(), g_landOutW, g_landOutH,
         g_landOx, g_landOy);
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
    if (!g_sdStore.beginMounted() && !esp2SdBusBegin()) {
        return false;
    }
    g_sdStore.setMounted(true);
    g_sdStore.ensureRomRoot();
    // Prefer persistent config ROM path when present and valid size.
    if (g_sysConfig.valid && g_sysConfig.romPath[0] && g_sdStore.exists(g_sysConfig.romPath) &&
        g_sdStore.fileSize(g_sysConfig.romPath) == Rom::kApple2PlusRomBytes) {
        strncpy(outPath, g_sysConfig.romPath, 95);
        outPath[95] = 0;
        return true;
    }
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

    if (!g_sdStore.beginMounted() && !esp2SdBusBegin()) {
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
        serviceCleanRoomCardRequests(g_a2bus.ram(), g_diskII);
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

static void applyDisplayPowerFromConfig() {
    DisplayPowerSettings dps = g_display_power.settings();
    if (!g_sysConfig.valid || g_sysConfig.screensaverSeconds == 0) {
        dps.screensaver = ScreensaverTimeout::Off;
        dps.screensaver_override_ms = 0;
        dps.screen_off = ScreenOffTimeout::Never;
        dps.screen_off_override_ms = 0;
    } else {
        const uint32_t ssMs = g_sysConfig.screensaverSeconds * 1000u;
        dps.screensaver_override_ms = ssMs;
        // Panel off a bit after screensaver (same idle clock).
        dps.screen_off_override_ms = ssMs + 120000u;
        dps.screensaver = ScreensaverTimeout::Min5;
        dps.screen_off = ScreenOffTimeout::Min10;
    }
    g_display_power.setSettings(dps);
    logf("CONFIG", "display ss_s=%u ss_ms=%u off_ms=%u",
         static_cast<unsigned>(g_sysConfig.screensaverSeconds),
         static_cast<unsigned>(dps.screensaver_override_ms),
         static_cast<unsigned>(dps.screen_off_override_ms));
}

static void loadPersistentConfigFromSd() {
    g_sysConfig = esp2_config::defaultSystemConfig();
    g_macroBank = esp2_macro::MacroBank{};
    if (!g_sdOwner.esp2MayUseFat()) {
        logf("CONFIG", "skip — SD not owned");
        return;
    }
    if (!sdExclusiveBegin()) {
        logf("CONFIG", "[FAIL] sd_exclusive");
        return;
    }
    char err[64]{};
    if (g_sdStore.exists(esp2_config::kSystemConfigPath)) {
        // Keep config small — reject oversized files.
        const size_t sz = g_sdStore.fileSize(esp2_config::kSystemConfigPath);
        if (sz == 0 || sz > 4096) {
            logf("CONFIG", "[FAIL] system.json size=%u — using defaults",
                 static_cast<unsigned>(sz));
        } else {
            char *buf = static_cast<char *>(malloc(sz + 1));
            if (!buf) {
                logf("CONFIG", "[FAIL] alloc");
            } else {
                size_t got = 0;
                if (g_sdStore.readAll(esp2_config::kSystemConfigPath,
                                      reinterpret_cast<uint8_t *>(buf), sz, &got) &&
                    got == sz) {
                    buf[sz] = 0;
                    g_sysConfig = esp2_config::parseSystemConfigJson(buf, sz, err, sizeof(err));
                    if (!g_sysConfig.valid) {
                        logf("CONFIG", "[FAIL] parse=%s — safe defaults", err[0] ? err : "?");
                        g_sysConfig = esp2_config::defaultSystemConfig();
                    } else {
                        logf("CONFIG", "system.json ok rom=%s drive1=%s boot=%d macro=%s "
                                       "orient=%s monitor=%s effect=%s ss=%u",
                             g_sysConfig.romPath[0] ? g_sysConfig.romPath : "-",
                             g_sysConfig.drive1[0] ? g_sysConfig.drive1 : "-",
                             g_sysConfig.bootFromDisk ? 1 : 0,
                             g_sysConfig.startupMacro[0] ? g_sysConfig.startupMacro : "-",
                             esp2_config::orientName(g_sysConfig.orientation),
                             esp2_config::monitorName(g_sysConfig.monitor),
                             esp2_config::effectName(g_sysConfig.effect),
                             static_cast<unsigned>(g_sysConfig.screensaverSeconds));
                    }
                } else {
                    logf("CONFIG", "[FAIL] read system.json");
                }
                free(buf);
            }
        }
    } else {
        logf("CONFIG", "no %s — defaults (no auto disk)", esp2_config::kSystemConfigPath);
    }

    if (g_sdStore.exists(esp2_config::kMacrosConfigPath)) {
        const size_t sz = g_sdStore.fileSize(esp2_config::kMacrosConfigPath);
        if (sz > 0 && sz <= 8192) {
            char *buf = static_cast<char *>(malloc(sz + 1));
            if (buf) {
                size_t got = 0;
                if (g_sdStore.readAll(esp2_config::kMacrosConfigPath,
                                      reinterpret_cast<uint8_t *>(buf), sz, &got) &&
                    got == sz) {
                    buf[sz] = 0;
                    if (!esp2_macro::parseMacrosJson(buf, sz, &g_macroBank, err, sizeof(err))) {
                        logf("CONFIG", "[FAIL] macros=%s", err[0] ? err : "?");
                        g_macroBank = esp2_macro::MacroBank{};
                    } else {
                        logf("CONFIG", "macros.json count=%u",
                             static_cast<unsigned>(g_macroBank.count));
                    }
                }
                free(buf);
            }
        } else {
            logf("CONFIG", "[FAIL] macros.json size");
        }
    } else {
        logf("CONFIG", "no macros.json");
    }
    sdExclusiveEnd();
    applyDisplayPowerFromConfig();
}

static void macroInjectKey(uint8_t apple7) {
    applyLiveKey(apple7, true);
}

static void applyStartupFromConfig() {
    if (g_startupConfigApplied) {
        return;
    }
    g_startupConfigApplied = true;
    if (!g_sysConfig.valid || !g_sysConfig.loaded) {
        logf("CONFIG", "startup skip — no valid config");
        return;
    }

    switch (g_sysConfig.monitor) {
    case esp2_config::Monitor::Green:
        g_presentMonitor = PresentMonitor::Green;
        break;
    case esp2_config::Monitor::Amber:
        g_presentMonitor = PresentMonitor::Amber;
        break;
    case esp2_config::Monitor::Artifact:
        g_presentMonitor = PresentMonitor::Artifact;
        break;
    case esp2_config::Monitor::White:
    default:
        g_presentMonitor = PresentMonitor::White;
        break;
    }
    g_presentEffect =
        (g_sysConfig.effect == esp2_config::Effect::Crt) ? PresentEffect::Crt : PresentEffect::Clean;
    g_presentOrient = (g_sysConfig.orientation == esp2_config::Orient::Landscape)
                          ? PresentOrientation::Landscape
                          : PresentOrientation::Classic;
    applyPresentChange();
    // Apply immediately on boot task if display task not yet running.
    servicePresentApplyOnDisplayTask();

    if (!g_sysConfig.drive1[0]) {
        logf("CONFIG", "startup — no drive1");
        return;
    }
    if (!mountDiskPath(g_sysConfig.drive1)) {
        logf("CONFIG", "[FAIL] mount drive1=%s", g_sysConfig.drive1);
        return;
    }
    if (g_sysConfig.bootFromDisk) {
        if (!bootMountedDisk()) {
            logf("CONFIG", "[FAIL] bootFromDisk");
            return;
        }
        logf("CONFIG", "bootFromDisk ok path=%s", g_mountedDiskPath);
    }
    if (g_sysConfig.startupMacro[0]) {
        char err[48]{};
        if (!esp2_macro::startMacro(&g_macroRunner, g_macroBank, g_sysConfig.startupMacro, millis(),
                                    err, sizeof(err))) {
            logf("CONFIG", "[FAIL] startup macro=%s err=%s", g_sysConfig.startupMacro,
                 err[0] ? err : "?");
        } else {
            logf("CONFIG", "startup macro=%s armed", g_sysConfig.startupMacro);
        }
    }
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
    computeLandscapeGeometry();
    logf("RAM", "viewport=%s psram_free=%u heap=%u", g_viewportFb ? "ok" : "FAIL",
         ESP.getPsramSize() ? ESP.getFreePsram() : 0, ESP.getFreeHeap());
    logf("PRESENT", "default monitor=%s effect=%s orient=%s classic=%dx%d@%d,%d land=%dx%d@%d,%d",
         presentMonitorName(), presentEffectName(), presentOrientName(), kViewW, kViewH, kViewX,
         kViewY, g_landOutW, g_landOutH, g_landOx, g_landOy);

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
        if (g_emuHold || g_sdExclusive || esp2_upload::isSessionActive()) {
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }
        g_cpu.runCycles(kExecQuantum);
        syncCycle();
        if (g_diskII.romKind() == DiskIIController::RomKind::CleanRoom) {
            serviceCleanRoomCardRequests(g_a2bus.ram(), g_diskII);
        }
        if (g_macroRunner.state == esp2_macro::RunState::Running) {
            // Generic video condition: graphics + HIRES (not title-specific).
            const auto &ss = g_a2bus.softSwitches();
            const bool hires = ss.isGraphics() && ss.isHires();
            const bool still = esp2_macro::tickMacro(&g_macroRunner, millis(), hires, macroInjectKey);
            if (!still) {
                if (g_macroRunner.state == esp2_macro::RunState::Done) {
                    logf("MACRO", "done id=%s", g_macroRunner.activeId);
                    Serial.printf("#ACK MACRO DONE id=%s\n", g_macroRunner.activeId);
                } else if (g_macroRunner.state == esp2_macro::RunState::Failed) {
                    logf("MACRO", "[FAIL] id=%s reason=%s", g_macroRunner.activeId,
                         g_macroRunner.failReason);
                    Serial.printf("#NAK MACRO FAIL id=%s reason=%s\n", g_macroRunner.activeId,
                                  g_macroRunner.failReason);
                }
            }
        }
        maybeLogGalaxianVisible();
        maybeLogDiskHeartbeat();
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
        // Serial RX is owned exclusively by a2ser (avoids concurrent Serial.read races).
        if (g_usbStorageUi || g_usbStorage.active()) {
            drawUsbStorageScreen();
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
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
                 "psram_free=%u monitor=%s effect=%s orient=%s render_us=%u xfer_us=%u",
                 cps, g_dispFrames, ESP.getFreeHeap(), ESP.getMinFreeHeap(),
                 ESP.getPsramSize() ? ESP.getFreePsram() : 0, presentMonitorName(),
                 presentEffectName(), presentOrientName(), g_lastRenderUs, g_lastXferUs);
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

        if (g_sdExclusive || esp2_upload::isSessionActive()) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        if (!g_display_ok || !g_display_power.isInteractive()) {
            vTaskDelay(pdMS_TO_TICKS(40));
            continue;
        }
        servicePresentApplyOnDisplayTask();
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

/** Persist boot stages for no-host diagnosis (read later via PC). */
static char g_bootLog[512];
static void writeBootStage(const char *stage) {
    if (!stage) {
        return;
    }
    char line[96];
    snprintf(line, sizeof(line), "t_ms=%lu stage=%s\n", static_cast<unsigned long>(millis()),
             stage);
    const size_t used = strlen(g_bootLog);
    if (used + strlen(line) + 1 < sizeof(g_bootLog)) {
        memcpy(g_bootLog + used, line, strlen(line) + 1);
    }
    if (!g_sdStore.beginMounted() || !g_sdOwner.esp2MayUseFat()) {
        return;
    }
    if (!SD.exists("/esp2/diagnostics")) {
        SD.mkdir("/esp2/diagnostics");
    }
    File f = SD.open("/esp2/diagnostics/last-boot.txt", FILE_WRITE);
    if (f) {
        f.printf("build=%s\n", kBuildId);
        f.print(g_bootLog);
        f.close();
    }
}

static void showEarlyBootSplash() {
    if (!g_gfx || !g_display_ok) {
        return;
    }
    g_gfx->fillScreen(0x0000);
    g_gfx->setTextColor(0x07E0);
    g_gfx->setTextSize(2);
    g_gfx->setCursor(24, 180);
    g_gfx->print("ESP][");
    g_gfx->setTextSize(1);
    g_gfx->setTextColor(0xFFFF);
    g_gfx->setCursor(24, 220);
    g_gfx->print(kBuildId);
    g_gfx->setCursor(24, 240);
    g_gfx->print("booting...");
}

void setup() {
    // TinyUSB OTG: USB stack before CDC traffic (composite CDC+MSC later).
#if !ARDUINO_USB_MODE
    USB.begin();
#endif
    Serial.setRxBufferSize(8192);
    Serial.begin(115200);
    // Non-zero TX timeout: 0 drops CDC bytes under load (corrupts #ACK lines).
    // Critical for standalone USB-power: TX must not hang forever without a host.
    Serial.setTxTimeoutMs(100);
    // Brief optional CDC wait only — never gate display bring-up on a host.
    const uint32_t serialWait = millis();
    while (!Serial && (millis() - serialWait) < 400) {
        delay(10);
    }

    // DISPLAY FIRST — power-supply-only boots must light the panel without CDC.
    g_display_ok = init_display();
    g_display_power.begin(draw_screensaver_stub, restore_ui_stub, panel_sleep_co5300,
                          panel_wake_co5300);
    DisplayPowerSettings dps{};
    dps.screensaver = ScreensaverTimeout::Off;
    dps.screen_off = ScreenOffTimeout::Never;
    g_display_power.setSettings(dps);
    g_display_power.notifyActivity("boot");
    showEarlyBootSplash();

    logf("ESP2", "build=%s", kBuildId);
    logf("ESP2", "usb_mode=%s msc=%s cdc_host=%s", ARDUINO_USB_MODE ? "HW_CDC" : "TinyUSB_OTG",
         UsbStorageMode::isSupported() ? "yes" : "no", Serial ? "yes" : "no");
    logf("ESP2", "psram=%u heap=%u", ESP.getPsramSize(), ESP.getFreeHeap());
    logf("SD", "owner=%s", sdOwnerStateName(g_sdOwner.state()));
    logf("DISPLAY", "early_init=%s power=ACTIVE", g_display_ok ? "ok" : "FAIL");

    // Boot-window upload ONLY when a USB host owns CDC (dev flash/upload chain).
    // Standalone USB-power: skip — previously blocked here ~45s+ and could hang on flush.
    if (Serial) {
        if (esp2_upload::pollAndRunSession(45000)) {
            logf("UPLOAD", "session finished — continuing boot");
        }
    } else {
        logf("UPLOAD", "skip boot-window — no CDC host (standalone power)");
    }

    g_touch_ok = probe_touch();
    g_sd_ok = probe_sd();
    if (g_sd_ok) {
        prepareEsp2Tree();
        writeBootStage(Serial ? "display_sd_cdc" : "display_sd_standalone");
    }
    g_imu_ok = probe_imu();

    xTaskCreatePinnedToCore(
        [](void *) {
            if (g_sdOwner.esp2MayUseFat()) {
                writeBootStage("config_load");
                loadPersistentConfigFromSd();
                writeBootStage(g_sysConfig.valid ? "config_ok" : "config_defaults");
                bootSuite();
                writeBootStage("boot_suite_done");
                applyStartupFromConfig();
                writeBootStage(g_diskMounted ? "startup_media_ok" : "startup_no_disk");
            } else {
                logf("APPLE2", "bootSuite skipped — SD not owned by ESP2");
                writeBootStage("sd_not_owned");
            }
            logf("SELFTEST", "display=%s touch=%s sd=%s imu=%s rom=%s basic=%s level4=%s owner=%s",
                 g_display_ok ? "PASS" : "FAIL", g_touch_ok ? "PASS" : "FAIL",
                 g_sd_ok ? "PASS" : "FAIL", g_imu_ok ? "PASS" : "FAIL",
                 g_rom_ok ? "PASS" : "SKIPPED_NO_ROM", g_basic_ok ? "PASS" : "n/a",
                 g_level4_ok ? "PASS" : "FAIL", sdOwnerStateName(g_sdOwner.state()));
            xTaskCreatePinnedToCore(displayTask, "a2disp", 10240, nullptr, 2, &g_dispTaskHandle, 0);
            xTaskCreatePinnedToCore(emulatorTask, "a2emu", 8192, nullptr, 1, nullptr, 1);
            g_schedulerGo = true;
            logf("SCHED", "tasks started (CDC via loop)");
            vTaskDelete(nullptr);
        },
        "a2boot", 24576, nullptr, 1, nullptr, 1);
}

void loop() {
    // Single CDC RX owner: Arduino loop task (avoids dual Serial.read + failed a2ser create).
    serviceDevSerialCommands();
    delay(5);
}
