#pragma once

#include "esp_bracket/apple2_machine.hpp"
#include "esp_bracket/input_types.hpp"

#include <cstdint>

namespace esp_bracket {

/** Configurable GamepadState → Apple II joystick / paddle mapping. */
struct GamepadMapConfig {
    int16_t deadzone = 2500; // out of int16 full scale
    bool invertX = false;
    bool invertY = false;
    bool digitalToPaddle = true;
    uint8_t paddleMin = 0;
    uint8_t paddleCenter = 128;
    uint8_t paddleMax = 255;
    /** A → PB0, B → PB1 by default. */
    bool swapButtons = false;
};

class GamepadMapper {
  public:
    void setConfig(const GamepadMapConfig &cfg) { cfg_ = cfg; }
    const GamepadMapConfig &config() const { return cfg_; }

    JoystickState toJoystick(const GamepadState &pad) const;
    PaddleState toPaddles(const GamepadState &pad) const;

    /** Map axis −32768..32767 → 0..255 with deadzone/center/invert. */
    uint8_t axisToPaddle(int16_t axis, bool invert) const;

  private:
    GamepadMapConfig cfg_{};
};

} // namespace esp_bracket
