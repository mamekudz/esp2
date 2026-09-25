#pragma once

#include <cstdint>

#include "esp_bracket/types.hpp"

namespace esp_bracket {

enum class DiagUserAnswer : uint8_t { None = 0, Ok, NotOk };

enum class ControlScreenSection : uint8_t {
    Library = 0,
    Drive1,
    Drive2,
    Bluetooth,
    Keyboard,
    Gamepad,
    Audio,
    Video,
    Effect,
    ResetApple,
    RestartEsp,
    PauseResume,
    Diagnostics,
    Settings
};

struct ControlScreenState {
    ControlScreenSection section;
    bool paused;
    VideoColorMode colorMode;
    DisplayEffectMode effectMode;
    EffectStrength effectStrength;
    bool drive1Inserted;
    bool drive2Inserted;
};

} // namespace esp_bracket
