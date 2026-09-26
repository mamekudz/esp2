#pragma once

#include <cstddef>
#include <cstdint>

namespace esp_bracket {

/**
 * Host key → Apple II keyboard latch value (high bit will be set by Keyboard).
 * Emulator hotkeys are NOT mapped here — intercept before calling.
 */
class AppleIIKeyMap {
  public:
    /**
     * Map host Unicode/ASCII codepoint + modifiers to Apple ASCII (7-bit).
     * Returns false if unmapped.
     * ctrl: Control held (A–Z → $01–$1A).
     */
    static bool mapHostKey(int hostCode, bool shift, bool ctrl, uint8_t *outApple7);

    /** Convenience: printable ASCII byte → Apple 7-bit (identity for most). */
    static uint8_t fromAscii(char c);
};

} // namespace esp_bracket
