#pragma once

#include <cstdint>

/** Minimal FT3168 touch read for apple2_text settings UI (Waveshare panel coords). */
namespace esp2_touch {

bool initAfterDisplay();
bool ready();

struct Sample {
    bool contact = false;
    bool pressed = false;
    bool released = false;
    bool wakeConsumed = false;
    uint16_t x = 0;
    uint16_t y = 0;
};

/** Poll once; call notifyActivity on display power before interpreting wakeConsumed. */
Sample poll();

} // namespace esp2_touch
