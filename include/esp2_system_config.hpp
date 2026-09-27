#pragma once

/**
 * Persistent ESP][ device configuration (SD: /esp2/config/system.json).
 * Paths and settings only — never ROM/disk payloads.
 *
 * Presentation is three independent dimensions:
 *   orientation — classic | landscape
 *   monitor     — white | green | amber | artifact
 *   effect      — clean | crt
 *
 * Legacy `"color": "sharp"|"artifact"` maps to monitor white|artifact.
 */
#include <cstddef>
#include <cstdint>

namespace esp2_config {

static constexpr int kSchemaVersion = 1;
static constexpr const char *kSystemConfigPath = "/esp2/config/system.json";
static constexpr const char *kMacrosConfigPath = "/esp2/config/macros.json";
static constexpr size_t kPathMax = 96;
static constexpr size_t kIdMax = 48;

enum class Orient : uint8_t { Classic = 0, Landscape = 1 };

/** Monitor appearance (mutually exclusive). White ≡ former Sharp. */
enum class Monitor : uint8_t { White = 0, Green = 1, Amber = 2, Artifact = 3 };

/** Optional post-monitor display effect. Clean ≡ no CRT pass. */
enum class Effect : uint8_t { Clean = 0, Crt = 1 };

/** @deprecated Prefer Monitor — kept for existing call sites / aliases. */
enum class ColorMode : uint8_t { Sharp = 0, Artifact = 1 };

struct SystemConfig {
    int schemaVersion = kSchemaVersion;
    char romPath[kPathMax]{};
    char drive1[kPathMax]{};
    char drive2[kPathMax]{};
    bool hasDrive2 = false;
    bool bootFromDisk = false;
    char startupMacro[kIdMax]{};
    Orient orientation = Orient::Classic;
    Monitor monitor = Monitor::White;
    Effect effect = Effect::Clean;
    /** 0 = screensaver disabled; >0 = idle seconds before panel screensaver. */
    uint32_t screensaverSeconds = 0;
    bool loaded = false;
    bool valid = false;

    /** Legacy view of monitor as Sharp/Artifact (Green/Amber → Sharp). */
    ColorMode color() const {
        return monitor == Monitor::Artifact ? ColorMode::Artifact : ColorMode::Sharp;
    }
};

/** Safe defaults: preserve historical ESP][ boot (no auto disk / no macro). */
inline SystemConfig defaultSystemConfig() {
    SystemConfig c{};
    c.schemaVersion = kSchemaVersion;
    c.valid = true;
    c.loaded = false;
    return c;
}

/**
 * Parse system.json text. On failure returns defaults with valid=false.
 * Does not throw; never bricks the caller.
 */
SystemConfig parseSystemConfigJson(const char *json, size_t len, char *err, size_t errLen);

/** Validate paths/enums; may clear invalid fields and set valid=false if fatal. */
bool validateSystemConfig(SystemConfig *cfg, char *err, size_t errLen);

inline const char *orientName(Orient o) {
    return o == Orient::Landscape ? "landscape" : "classic";
}

inline const char *monitorName(Monitor m) {
    switch (m) {
    case Monitor::Green:
        return "green";
    case Monitor::Amber:
        return "amber";
    case Monitor::Artifact:
        return "artifact";
    case Monitor::White:
    default:
        return "white";
    }
}

inline const char *effectName(Effect e) {
    return e == Effect::Crt ? "crt" : "clean";
}

/** Legacy JSON/UI alias: white→sharp, artifact→artifact. */
inline const char *colorName(Monitor m) {
    return m == Monitor::Artifact ? "artifact" : "sharp";
}

inline const char *colorName(ColorMode c) {
    return c == ColorMode::Artifact ? "artifact" : "sharp";
}

} // namespace esp2_config
