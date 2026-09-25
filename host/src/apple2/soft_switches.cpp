#include "esp_bracket/soft_switches.hpp"

namespace esp_bracket {

void SoftSwitches::reset() {
    text_ = true;
    mixed_ = false;
    page2_ = false;
    hires_ = false;
}

bool SoftSwitches::access(uint16_t address) {
    switch (address) {
    case kAddrGraphics:
        text_ = false;
        return true;
    case kAddrText:
        text_ = true;
        return true;
    case kAddrFull:
        mixed_ = false;
        return true;
    case kAddrMixed:
        mixed_ = true;
        return true;
    case kAddrPage1:
        page2_ = false;
        return true;
    case kAddrPage2:
        page2_ = true;
        return true;
    case kAddrLores:
        hires_ = false;
        return true;
    case kAddrHires:
        hires_ = true;
        return true;
    default:
        return false;
    }
}

} // namespace esp_bracket
