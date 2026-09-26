#include "esp_bracket/gamepad_mapper.hpp"

namespace esp_bracket {

namespace {

int16_t applyDeadzone(int16_t v, int16_t deadzone) {
    if (v > -deadzone && v < deadzone) {
        return 0;
    }
    return v;
}

} // namespace

uint8_t GamepadMapper::axisToPaddle(int16_t axis, bool invert) const {
    int32_t v = applyDeadzone(axis, cfg_.deadzone);
    if (invert) {
        v = -v;
    }
    // Map −32768..32767 → paddleMin..paddleMax around paddleCenter.
    const int32_t span =
        static_cast<int32_t>(cfg_.paddleMax) - static_cast<int32_t>(cfg_.paddleMin);
    const int32_t mid = static_cast<int32_t>(cfg_.paddleCenter);
    // Scale: full deflection from center uses half int16 range.
    const int32_t scaled = mid + (v * (span / 2)) / 32767;
    if (scaled < cfg_.paddleMin) {
        return cfg_.paddleMin;
    }
    if (scaled > cfg_.paddleMax) {
        return cfg_.paddleMax;
    }
    return static_cast<uint8_t>(scaled);
}

JoystickState GamepadMapper::toJoystick(const GamepadState &pad) const {
    JoystickState j{};
    if (!pad.connected) {
        return j;
    }
    int16_t x = pad.leftX;
    int16_t y = pad.leftY;
    if (cfg_.digitalToPaddle) {
        if (pad.dpadLeft) {
            x = -32767;
        } else if (pad.dpadRight) {
            x = 32767;
        }
        if (pad.dpadUp) {
            y = -32767;
        } else if (pad.dpadDown) {
            y = 32767;
        }
    }
    x = applyDeadzone(x, cfg_.deadzone);
    y = applyDeadzone(y, cfg_.deadzone);
    if (cfg_.invertX) {
        x = static_cast<int16_t>(-x);
    }
    if (cfg_.invertY) {
        y = static_cast<int16_t>(-y);
    }
    j.x = x;
    j.y = y;
    if (cfg_.swapButtons) {
        j.button0 = pad.btnB;
        j.button1 = pad.btnA;
    } else {
        j.button0 = pad.btnA;
        j.button1 = pad.btnB;
    }
    return j;
}

PaddleState GamepadMapper::toPaddles(const GamepadState &pad) const {
    PaddleState p{};
    if (!pad.connected) {
        p.p0 = cfg_.paddleCenter;
        p.p1 = cfg_.paddleCenter;
        return p;
    }
    int16_t x = pad.leftX;
    int16_t y = pad.leftY;
    if (cfg_.digitalToPaddle) {
        if (pad.dpadLeft) {
            x = -32767;
        } else if (pad.dpadRight) {
            x = 32767;
        }
        if (pad.dpadUp) {
            y = -32767;
        } else if (pad.dpadDown) {
            y = 32767;
        }
    }
    p.p0 = axisToPaddle(x, cfg_.invertX);
    p.p1 = axisToPaddle(y, cfg_.invertY);
    if (cfg_.swapButtons) {
        p.button0 = pad.btnB;
        p.button1 = pad.btnA;
    } else {
        p.button0 = pad.btnA;
        p.button1 = pad.btnB;
    }
    return p;
}

} // namespace esp_bracket
