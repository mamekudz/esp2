#pragma once

#include <cstdint>

namespace esp_bracket {

struct GamepadState {
    bool connected;
    int16_t leftX;
    int16_t leftY;
    int16_t rightX;
    int16_t rightY;
    bool dpadUp;
    bool dpadDown;
    bool dpadLeft;
    bool dpadRight;
    bool btnA;
    bool btnB;
    bool btnX;
    bool btnY;
    bool btnL1;
    bool btnR1;
    bool btnL2;
    bool btnR2;
    bool btnStart;
    bool btnSelect;
    bool btnHome;
    uint8_t batteryPercent; // 0..100, 255=unknown
};

struct KeyboardEvent {
    uint16_t hidUsage;
    bool pressed;
    bool shift;
    bool ctrl;
    bool alt;
    bool meta;
};

enum class HotkeyAction : uint8_t {
    None = 0,
    OpenControlScreen,
    ToggleVideoMode,
    ToggleEffect,
    PauseResume,
    AppleReset,
    EspRestart
};

/** Returns true if consumed as emulator hotkey (must not reach Apple II). */
using HotkeyInterceptor = HotkeyAction (*)(const KeyboardEvent& ev);

struct VirtualKeyDef {
    const char* id;      // stable id, not localized glyph
    int16_t row;
    int16_t col;
    int16_t rowSpan;
    int16_t colSpan;
    uint8_t appleKeycode; // placeholder mapping
};

} // namespace esp_bracket
