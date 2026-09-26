#include "esp_bracket/key_map.hpp"

namespace esp_bracket {

uint8_t AppleIIKeyMap::fromAscii(char c) {
    return static_cast<uint8_t>(c & 0x7F);
}

bool AppleIIKeyMap::mapHostKey(int hostCode, bool shift, bool ctrl, uint8_t *outApple7) {
    if (!outApple7) {
        return false;
    }
    // Special keys (host conventions)
    if (hostCode == '\r' || hostCode == '\n' || hostCode == 0x100 /*Return*/) {
        *outApple7 = 0x0D;
        return true;
    }
    if (hostCode == 0x1B) { // Escape
        *outApple7 = 0x1B;
        return true;
    }
    if (hostCode == 0x08 || hostCode == 0x7F) { // Backspace / Delete → left arrow
        *outApple7 = 0x08;
        return true;
    }

    int c = hostCode;
    if (c >= 0 && c < 128) {
        if (ctrl) {
            if (c >= 'a' && c <= 'z') {
                c = c - 'a' + 1;
            } else if (c >= 'A' && c <= 'Z') {
                c = c - 'A' + 1;
            } else if (c >= 1 && c <= 26) {
                // already control
            } else {
                return false;
            }
            *outApple7 = static_cast<uint8_t>(c & 0x7F);
            return true;
        }
        // Apple II typically uses uppercase for letters in many contexts;
        // pass through ASCII as typed (shift already applied by host).
        (void)shift;
        *outApple7 = static_cast<uint8_t>(c & 0x7F);
        return true;
    }
    return false;
}

} // namespace esp_bracket
