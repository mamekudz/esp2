#pragma once

/**
 * Persistent ESP][ device configuration (SD: /esp2/config/system.json).
 * Paths and settings only — never ROM/disk payloads.
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
    ColorMode color = ColorMode::Sharp;
    /** 0 = screensaver disabled; >0 = idle seconds before panel screensaver. */
    uint32_t screensaverSeconds = 0;
    bool loaded = false;
    bool valid = false;
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

inline const char *colorName(ColorMode c) {
    return c == ColorMode::Artifact ? "artifact" : "sharp";
}

} // namespace esp2_config
