#pragma once

/**
 * Local ESP][ control actions + hotkey/chord binding engine.
 *
 * Evaluated BEFORE Apple II forwarding. Matching triggers are consumed.
 * Transport-agnostic: BLE HID, future physical 5-way, and CDC (when
 * KeyboardEvent/GamepadState are supplied) share this engine.
 *
 * BlueShift devices are ordinary BLE HID — no BlueShift branches here.
 */

#include "esp_bracket/input_types.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace esp_bracket {

/** Stable local action ids (V1). */
enum class LocalAction : uint8_t {
    None = 0,
    ResetAppleIi = 1,
    OpenSettings = 2,
};

inline LocalAction hotkeyToLocal(HotkeyAction a) {
    switch (a) {
    case HotkeyAction::AppleReset:
        return LocalAction::ResetAppleIi;
    case HotkeyAction::OpenControlScreen: // same value as OpenSettings
        return LocalAction::OpenSettings;
    default:
        return LocalAction::None;
    }
}

/** Normalized gamepad button bits for chord matching (not raw HID). */
enum GamepadButtonBit : uint32_t {
    GpA = 1u << 0,
    GpB = 1u << 1,
    GpX = 1u << 2,
    GpY = 1u << 3,
    GpL1 = 1u << 4,
    GpR1 = 1u << 5,
    GpL2 = 1u << 6,
    GpR2 = 1u << 7,
    GpStart = 1u << 8,
    GpSelect = 1u << 9,
    GpHome = 1u << 10,
    GpDpadUp = 1u << 11,
    GpDpadDown = 1u << 12,
    GpDpadLeft = 1u << 13,
    GpDpadRight = 1u << 14,
};

inline uint32_t gamepadButtonMask(const GamepadState& s) {
    uint32_t m = 0;
    if (s.btnA) m |= GpA;
    if (s.btnB) m |= GpB;
    if (s.btnX) m |= GpX;
    if (s.btnY) m |= GpY;
    if (s.btnL1) m |= GpL1;
    if (s.btnR1) m |= GpR1;
    if (s.btnL2) m |= GpL2;
    if (s.btnR2) m |= GpR2;
    if (s.btnStart) m |= GpStart;
    if (s.btnSelect) m |= GpSelect;
    if (s.btnHome) m |= GpHome;
    if (s.dpadUp) m |= GpDpadUp;
    if (s.dpadDown) m |= GpDpadDown;
    if (s.dpadLeft) m |= GpDpadLeft;
    if (s.dpadRight) m |= GpDpadRight;
    return m;
}

struct KeyboardBinding {
    bool ctrl = false;
    bool alt = false;
    bool shift = false;
    bool meta = false;
    /** USB HID usage (Keyboard/Keypad page) — e.g. 0x15 = R, 0x16 = S. */
    uint16_t hidUsage = 0;
    /** Optional ASCII fallback (A–Z / 0–9); 0 = unused. */
    char ascii = 0;
    bool valid = false;
};

struct GamepadChordBinding {
    uint32_t buttons = 0; // required bits (exact subset match: all must be down)
    uint32_t holdMs = 1500;
    bool valid = false;
};

struct LocalBindingSet {
    KeyboardBinding keyboard{};
    GamepadChordBinding gamepad{};
};

struct LocalControlConfig {
    LocalBindingSet resetAppleIi{};
    LocalBindingSet openSettings{};
};

/** Sensible V1 defaults — editable via µGulp / bluetooth.json. */
inline LocalControlConfig defaultLocalControlConfig() {
    LocalControlConfig c{};
    // Ctrl+Alt+R → ResetAppleIi
    c.resetAppleIi.keyboard = {true, true, false, false, 0x15, 'R', true};
    // Start+B hold 1.5s → ResetAppleIi
    c.resetAppleIi.gamepad = {GpStart | GpB, 1500, true};
    // Ctrl+Alt+S → OpenSettings
    c.openSettings.keyboard = {true, true, false, false, 0x16, 'S', true};
    // Start+Select hold 1.5s → OpenSettings
    c.openSettings.gamepad = {GpStart | GpSelect, 1500, true};
    return c;
}

struct ChordHoldState {
    LocalAction armed = LocalAction::None;
    uint32_t sinceMs = 0;
    bool fired = false;
};

/**
 * Match a keyboard event against a binding.
 * Requires press edge (pressed=true). Modifiers must match exactly for
 * ctrl/alt/shift/meta flags that the binding cares about (required true → must
 * be down; required false → must be up).
 */
inline bool keyboardBindingMatches(const KeyboardBinding& b, const KeyboardEvent& ev) {
    if (!b.valid || !ev.pressed) {
        return false;
    }
    if (b.ctrl != ev.ctrl || b.alt != ev.alt || b.shift != ev.shift || b.meta != ev.meta) {
        return false;
    }
    if (b.hidUsage != 0 && ev.hidUsage == b.hidUsage) {
        return true;
    }
    if (b.ascii != 0 && ev.hidUsage == 0) {
        // ASCII fallback path (e.g. host tests): compare via hidUsage unused.
        return false;
    }
    return false;
}

/** Exact chord: every required bit is down; extra buttons are allowed. */
inline bool gamepadChordMatches(const GamepadChordBinding& b, uint32_t mask) {
    if (!b.valid || b.buttons == 0) {
        return false;
    }
    return (mask & b.buttons) == b.buttons;
}

class LocalControlEngine {
public:
    void setConfig(const LocalControlConfig& cfg) {
        cfg_ = sanitizeConfig(cfg);
    }

    const LocalControlConfig& config() const { return cfg_; }

    /**
     * Evaluate keyboard event. Returns action if consumed; LocalAction::None
     * means forward to Apple II.
     */
    LocalAction evaluateKeyboard(const KeyboardEvent& ev) {
        if (!ev.pressed) {
            return LocalAction::None;
        }
        if (keyboardBindingMatches(cfg_.resetAppleIi.keyboard, ev)) {
            return LocalAction::ResetAppleIi;
        }
        if (keyboardBindingMatches(cfg_.openSettings.keyboard, ev)) {
            return LocalAction::OpenSettings;
        }
        return LocalAction::None;
    }

    /**
     * Evaluate gamepad state with wall-clock ms. Non-blocking hold timing.
     * Returns action once when hold completes; subsequent frames while held
     * return None until chord is released and re-armed.
     */
    LocalAction evaluateGamepad(const GamepadState& state, uint32_t nowMs) {
        if (!state.connected) {
            chord_ = {};
            return LocalAction::None;
        }
        const uint32_t mask = gamepadButtonMask(state);

        LocalAction match = LocalAction::None;
        uint32_t needHold = 0;
        if (gamepadChordMatches(cfg_.resetAppleIi.gamepad, mask)) {
            match = LocalAction::ResetAppleIi;
            needHold = cfg_.resetAppleIi.gamepad.holdMs;
        } else if (gamepadChordMatches(cfg_.openSettings.gamepad, mask)) {
            match = LocalAction::OpenSettings;
            needHold = cfg_.openSettings.gamepad.holdMs;
        }

        if (match == LocalAction::None) {
            chord_ = {};
            return LocalAction::None;
        }

        // Partial → full: (re)arm when match changes.
        if (chord_.armed != match) {
            chord_.armed = match;
            chord_.sinceMs = nowMs;
            chord_.fired = false;
            return LocalAction::None;
        }

        if (chord_.fired) {
            return LocalAction::None;
        }

        const uint32_t hold = needHold == 0 ? 1 : needHold;
        if (nowMs - chord_.sinceMs >= hold) {
            chord_.fired = true;
            return match;
        }
        return LocalAction::None;
    }

    /** True while a chord is armed but not yet fired (suppress game input). */
    bool chordConsumingGamepad(const GamepadState& state) const {
        if (!state.connected || chord_.armed == LocalAction::None) {
            return false;
        }
        const uint32_t mask = gamepadButtonMask(state);
        if (chord_.armed == LocalAction::ResetAppleIi) {
            return gamepadChordMatches(cfg_.resetAppleIi.gamepad, mask);
        }
        if (chord_.armed == LocalAction::OpenSettings) {
            return gamepadChordMatches(cfg_.openSettings.gamepad, mask);
        }
        return false;
    }

    void resetChord() { chord_ = {}; }

private:
    static LocalControlConfig sanitizeConfig(const LocalControlConfig& in) {
        LocalControlConfig d = defaultLocalControlConfig();
        // Keep provided valid bindings; fall back per-slot if malformed/empty.
        if (in.resetAppleIi.keyboard.valid) {
            d.resetAppleIi.keyboard = in.resetAppleIi.keyboard;
        }
        if (in.resetAppleIi.gamepad.valid && in.resetAppleIi.gamepad.buttons != 0) {
            d.resetAppleIi.gamepad = in.resetAppleIi.gamepad;
            if (d.resetAppleIi.gamepad.holdMs < 200) {
                d.resetAppleIi.gamepad.holdMs = 200;
            }
            if (d.resetAppleIi.gamepad.holdMs > 10000) {
                d.resetAppleIi.gamepad.holdMs = 10000;
            }
        }
        if (in.openSettings.keyboard.valid) {
            d.openSettings.keyboard = in.openSettings.keyboard;
        }
        if (in.openSettings.gamepad.valid && in.openSettings.gamepad.buttons != 0) {
            d.openSettings.gamepad = in.openSettings.gamepad;
            if (d.openSettings.gamepad.holdMs < 200) {
                d.openSettings.gamepad.holdMs = 200;
            }
            if (d.openSettings.gamepad.holdMs > 10000) {
                d.openSettings.gamepad.holdMs = 10000;
            }
        }
        return d;
    }

    LocalControlConfig cfg_ = defaultLocalControlConfig();
    ChordHoldState chord_{};
};

inline const char* localActionName(LocalAction a) {
    switch (a) {
    case LocalAction::ResetAppleIi:
        return "RESET_APPLE_II";
    case LocalAction::OpenSettings:
        return "OPEN_SETTINGS";
    default:
        return "NONE";
    }
}

/** Human-readable keyboard binding (en-US machine string for diagnostics/forms). */
inline void formatKeyboardBinding(const KeyboardBinding& b, char* out, size_t outLen) {
    if (!out || outLen == 0) {
        return;
    }
    if (!b.valid) {
        std::snprintf(out, outLen, "(none)");
        return;
    }
    char key[8]{};
    if (b.ascii) {
        key[0] = static_cast<char>(b.ascii >= 'a' && b.ascii <= 'z' ? b.ascii - 32 : b.ascii);
        key[1] = 0;
    } else {
        std::snprintf(key, sizeof(key), "0x%02X", static_cast<unsigned>(b.hidUsage));
    }
    std::snprintf(out, outLen, "%s%s%s%s%s", b.ctrl ? "Ctrl+" : "", b.alt ? "Alt+" : "",
                  b.shift ? "Shift+" : "", b.meta ? "GUI+" : "", key);
}

} // namespace esp_bracket
