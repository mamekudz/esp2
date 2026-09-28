#pragma once

/**
 * ESP][ BLE HID host (apple2_text) — scan / pair / reconnect / HID input.
 *
 * BlueShift-mediated devices appear as ordinary BLE HID peripherals.
 * No BlueShift-specific protocol or mapping belongs here.
 */

#include "esp2_bluetooth_config.hpp"
#include "esp_bracket/input_types.hpp"
#include "esp_bracket/local_control.hpp"

#include <cstddef>
#include <cstdint>

namespace esp2_ble {

static constexpr size_t kScanMax = 16;
static constexpr uint32_t kDefaultScanSec = 8;

enum class ScanDeviceKind : uint8_t {
    Unknown = 0,
    Keyboard = 1,
    Gamepad = 2,
    OtherHid = 3,
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

struct BleHostStatus {
    bool ready = false;
    bool scanning = false;
    bool connected = false;
    char connectedAddr[20]{};
    char connectedName[40]{};
    esp2_ble_config::DeviceType connectedType = esp2_ble_config::DeviceType::Unknown;
    size_t knownCount = 0;
    size_t discoveredCount = 0;
    /** Documented simultaneous active connection budget (measured/assumed). */
    uint8_t maxSimultaneousConnections = 1;
};

using LocalActionHandler = void (*)(esp_bracket::LocalAction action, const char *source);

bool init();
bool ready();

/** Blocking scan; fills internal discovered list. */
bool scan(uint32_t durationSec = kDefaultScanSec);

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

/** Latest normalized gamepad (for diagnostics). */
esp_bracket::GamepadState latestGamepad();

inline const char *kindName(ScanDeviceKind k) {
    switch (k) {
    case ScanDeviceKind::Keyboard:
        return "keyboard";
    case ScanDeviceKind::Gamepad:
        return "gamepad";
    case ScanDeviceKind::OtherHid:
        return "hid";
    default:
        return "unknown";
    }
}

} // namespace esp2_ble
