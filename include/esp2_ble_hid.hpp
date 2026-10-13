#pragma once

/**
 * ESP][ BLE HID host (apple2_text) — scan / pair / reconnect / HID input.
 *
 * BlueShift-mediated devices appear as ordinary BLE HID peripherals.
 * No BlueShift-specific protocol or mapping belongs here.
 */

#include "esp2_bluetooth_config.hpp"
#include "esp_bracket/ble_ui_model.hpp"
#include "esp_bracket/input_types.hpp"
#include "esp_bracket/local_control.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace esp2_ble {

static constexpr size_t kScanMax = 24;
static constexpr uint32_t kDefaultScanSec = 8;

/** UI / diagnostics link state (maps to esp_bracket::BleLinkState). */
using LinkState = esp_bracket::BleLinkState;

enum class ScanDeviceKind : uint8_t {
    Unknown = 0,
    Keyboard = 1,
    Gamepad = 2,
    OtherHid = 3,
    /** Keyboard + pointer/gamepad caps on one HID link (composite). */
    Composite = 4,
};

/** How integrated pointer / pad reports map into Apple II GameIo. */
enum class PadMapMode : uint8_t {
    Off = 0,
    Joystick = 1, // X/Y → paddles 0/1 as joystick axes + click → PB0
    Paddle = 2,   // primary axis → paddle 0 (relative accumulate or absolute)
};

struct DiscoveredDevice {
    char name[40]{};
    char address[20]{};
    uint8_t addrType = 0;
    int8_t rssi = 0;
    ScanDeviceKind kind = ScanDeviceKind::Unknown;
    bool known = false;
    bool connected = false;
};

struct ConnectedCaps {
    bool keyboard = false;
    bool mouse = false;
    bool gamepad = false;
    bool joystick = false;
};

/** Live HID pad decode for the on-device joystick test. */
struct PadDiag {
    bool connected = false;
    int16_t leftX = 0;
    int16_t leftY = 0;
    uint16_t buttons = 0;
    uint8_t hat = 0x0F;
    uint8_t p0 = 128;
    uint8_t p1 = 128;
    bool pb0 = false;
    bool pb1 = false;
    uint8_t lastLen = 0;
    uint8_t lastRaw[16]{};
    uint32_t notifyAgeMs = 0;
};

struct BleHostStatus {
    bool ready = false;
    bool scanning = false;
    bool opBusy = false;
    LinkState lastOpState = LinkState::KnownOffline;
    bool connected = false;
    char connectedAddr[20]{};
    char connectedName[40]{};
    esp2_ble_config::DeviceType connectedType = esp2_ble_config::DeviceType::Unknown;
    size_t knownCount = 0;
    size_t discoveredCount = 0;
    /** Documented simultaneous active connection budget (measured/assumed). */
    uint8_t maxSimultaneousConnections = 2;
};

using LocalActionHandler = void (*)(esp_bracket::LocalAction action, const char *source);

bool init();
bool ready();

/** Blocking scan; fills internal discovered list (CDC / µGulp). */
bool scan(uint32_t durationSec = kDefaultScanSec);

/** Non-blocking scan for on-device UI. Returns false if already scanning/busy. */
bool beginScanAsync(uint32_t durationSec = kDefaultScanSec);
void cancelScanAsync();
bool scanAsyncActive();

/** Background connect/pair/disconnect (FreeRTOS worker). */
bool requestConnect(const char *address);
bool requestPair(const char *address);
bool requestDisconnect(const char *address = nullptr);
/** Connect first `autoReconnect` known device (non-blocking). */
bool requestAutoReconnect();

size_t discoveredCount();
bool getDiscovered(size_t index, DiscoveredDevice *out);

const esp2_ble_config::BluetoothConfig &config();
void setConfig(const esp2_ble_config::BluetoothConfig &cfg);

/** Persist current config to SD path (requires exclusive SD access). */
bool saveConfigToSd();

/** Load from SD; missing/invalid → defaults (hotkeys still usable). */
bool loadConfigFromSd();

/**
 * Pair + connect + bond attempt for address from last scan (or known).
 * On success upserts known device metadata and saves when SD available.
 */
bool pairAddress(const char *address);

bool connectAddress(const char *address);
void disconnect();
bool forgetAddress(const char *address);

BleHostStatus status();

LinkState linkStateForAddress(const char *address);
bool connectedCaps(ConnectedCaps *out);

/**
 * Poll HID notifications → KeyboardEvent / GamepadState → local control
 * engine → optional Apple II inject callbacks.
 * Call from main loop (non-blocking).
 */
void service(uint32_t nowMs);

void setLocalActionHandler(LocalActionHandler handler);

/**
 * Apple II inject hooks (set by main.cpp).
 * key: apple7 + pressed; pad: normalized paddles/buttons.
 * When local chord is consuming, pad inject is suppressed.
 */
using KeyInjectFn = void (*)(uint8_t apple7, bool pressed);
using PadInjectFn = void (*)(uint8_t p0, uint8_t p1, bool pb0, bool pb1, bool pb2);
void setInjectors(KeyInjectFn keyFn, PadInjectFn padFn);

/** Latest normalized gamepad / virtual pad (for diagnostics). */
esp_bracket::GamepadState latestGamepad();
PadDiag latestPadDiag();

/** Rate-limited HID report monitor over CDC (`#BLE HID …`). */
void setHidMonitor(bool enabled);
bool hidMonitorEnabled();

void setPadMapMode(PadMapMode mode);
PadMapMode padMapMode();

/** Relative-pad sensitivity (pixels→axis units per HID count). Clamp 1..32. */
void setPadSensitivity(uint8_t sensitivity);
uint8_t padSensitivity();

/** Recenter virtual absolute X/Y (relative mouse/touchpad accumulate). */
void recenterVirtualPad();

inline const char *kindName(ScanDeviceKind k) {
    switch (k) {
    case ScanDeviceKind::Keyboard:
        return "keyboard";
    case ScanDeviceKind::Gamepad:
        return "gamepad";
    case ScanDeviceKind::OtherHid:
        return "hid";
    case ScanDeviceKind::Composite:
        return "composite";
    default:
        return "unknown";
    }
}

inline const char *padMapModeName(PadMapMode m) {
    switch (m) {
    case PadMapMode::Joystick:
        return "joystick";
    case PadMapMode::Paddle:
        return "paddle";
    default:
        return "off";
    }
}

inline PadMapMode parsePadMapMode(const char *s) {
    if (!s) {
        return PadMapMode::Off;
    }
    if (strcmp(s, "joystick") == 0 || strcmp(s, "joy") == 0) {
        return PadMapMode::Joystick;
    }
    if (strcmp(s, "paddle") == 0 || strcmp(s, "pdl") == 0) {
        return PadMapMode::Paddle;
    }
    return PadMapMode::Off;
}

} // namespace esp2_ble
