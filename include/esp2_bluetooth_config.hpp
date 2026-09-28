#pragma once

/**
 * Persistent BLE known-devices + local control bindings.
 * Device path: /esp2/config/bluetooth.json
 *
 * Sibling of system.json / macros.json — same config architecture.
 * Invalid/missing file → safe defaults (no known devices, default hotkeys).
 */

#include "esp_bracket/local_control.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace esp2_ble_config {

static constexpr int kSchemaVersion = 1;
static constexpr const char *kBluetoothConfigPath = "/esp2/config/bluetooth.json";
static constexpr size_t kMaxKnownDevices = 8;
static constexpr size_t kNameMax = 40;
static constexpr size_t kAddrMax = 20;

enum class DeviceType : uint8_t {
    Unknown = 0,
    Keyboard = 1,
    Gamepad = 2,
};

struct KnownDevice {
    char address[kAddrMax]{};
    uint8_t addrType = 0; // 0=public, 1=random
    char name[kNameMax]{};
    DeviceType type = DeviceType::Unknown;
    bool autoReconnect = true;
    bool bonded = false;
};

struct BluetoothConfig {
    int schemaVersion = kSchemaVersion;
    KnownDevice known[kMaxKnownDevices]{};
    size_t knownCount = 0;
    esp_bracket::LocalControlConfig control = esp_bracket::defaultLocalControlConfig();
    bool loaded = false;
    bool valid = false;
};

inline BluetoothConfig defaultBluetoothConfig() {
    BluetoothConfig c{};
    c.schemaVersion = kSchemaVersion;
    c.control = esp_bracket::defaultLocalControlConfig();
    c.valid = true;
    c.loaded = false;
    return c;
}

inline const char *deviceTypeName(DeviceType t) {
    switch (t) {
    case DeviceType::Keyboard:
        return "keyboard";
    case DeviceType::Gamepad:
        return "gamepad";
    default:
        return "unknown";
    }
}

inline DeviceType parseDeviceType(const char *s) {
    if (!s) {
        return DeviceType::Unknown;
    }
    if (strcmp(s, "keyboard") == 0) {
        return DeviceType::Keyboard;
    }
    if (strcmp(s, "gamepad") == 0 || strcmp(s, "joystick") == 0) {
        return DeviceType::Gamepad;
    }
    return DeviceType::Unknown;
}

/** Map A–Z / a–z to HID keyboard usage (0x04–0x1D). 0 on failure. */
inline uint16_t asciiToHidUsage(char c) {
    if (c >= 'a' && c <= 'z') {
        c = static_cast<char>(c - 32);
    }
    if (c >= 'A' && c <= 'Z') {
        return static_cast<uint16_t>(0x04 + (c - 'A'));
    }
    return 0;
}

/**
 * Parse bluetooth.json. On failure returns defaults with valid=false
 * (callers should still apply defaultLocalControlConfig for usability).
 */
BluetoothConfig parseBluetoothConfigJson(const char *json, size_t len, char *err, size_t errLen);

/** Upsert known device by address (case-insensitive). */
bool upsertKnownDevice(BluetoothConfig *cfg, const KnownDevice &dev);

/** Remove known device by address. Returns true if removed. */
bool forgetKnownDevice(BluetoothConfig *cfg, const char *address);

/** Find known device index or -1. */
int findKnownDevice(const BluetoothConfig &cfg, const char *address);

/**
 * Serialize config to JSON into out (NUL-terminated). Returns bytes written
 * excluding NUL, or 0 on failure.
 */
size_t serializeBluetoothConfigJson(const BluetoothConfig &cfg, char *out, size_t outLen);

} // namespace esp2_ble_config
