#include "esp_bracket/keyboard.hpp"

namespace esp_bracket {

void Keyboard::reset() {
    latch_ = 0;
}

void Keyboard::keyDown(uint8_t appleAsciiWithHighBitSemantics) {
    const uint8_t ascii7 = static_cast<uint8_t>(appleAsciiWithHighBitSemantics & 0x7Fu);
    latch_ = static_cast<uint8_t>(0x80u | ascii7);
}

void Keyboard::keyUp(uint8_t /*normalizedKey*/) {
    // Apple II keyboard hardware does not clear the latch on key-up.
}

uint8_t Keyboard::readData() const {
    return latch_;
}

void Keyboard::clearStrobe() {
    latch_ = static_cast<uint8_t>(latch_ & 0x7Fu);
}

} // namespace esp_bracket
