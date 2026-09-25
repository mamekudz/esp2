#pragma once

#include <cstdint>

namespace esp_bracket {

/**
 * Apple II keyboard latch at $C000 / clear at $C010.
 *
 * Latch layout: bit7 = strobe pending, bits0–6 = ASCII character.
 * keyDown accepts either 7-bit ASCII or Apple-style value with high bit set.
 */
class Keyboard {
  public:
    Keyboard() { reset(); }

    void reset();

    /** Queue a key. Sets strobe (bit7). Bits0–6 hold ASCII. */
    void keyDown(uint8_t appleAsciiWithHighBitSemantics);

    /** Optional; Apple II hardware does not clear on key-up. No-op by default. */
    void keyUp(uint8_t /*normalizedKey*/);

    /** Read $C000 — returns latch (bit7 set while strobe pending). */
    uint8_t readData() const;

    /** Access $C010 — clears strobe bit, leaves ASCII bits. */
    void clearStrobe();

    bool strobePending() const { return (latch_ & 0x80u) != 0; }
    uint8_t ascii7() const { return static_cast<uint8_t>(latch_ & 0x7Fu); }
    uint8_t latch() const { return latch_; }

  private:
    uint8_t latch_ = 0;
};

} // namespace esp_bracket
