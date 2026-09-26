#include "sd_ownership.hpp"

#include <cstdio>
#include <cstring>

namespace esp_bracket {

const char *sdOwnerStateName(SdOwnerState s) {
    switch (s) {
    case SdOwnerState::Esp2OwnsSd:
        return "ESP2_OWNS_SD";
    case SdOwnerState::TransitionToUsb:
        return "TRANSITION_TO_USB";
    case SdOwnerState::UsbOwnsSd:
        return "USB_OWNS_SD";
    case SdOwnerState::TransitionToEsp2:
        return "TRANSITION_TO_ESP2";
    case SdOwnerState::Error:
        return "ERROR";
    }
    return "UNKNOWN";
}

void SdOwnership::setState(SdOwnerState s) {
    state_ = s;
    if (s != SdOwnerState::Error) {
        lastError_[0] = 0;
    }
}

void SdOwnership::setError(const char *reason) {
    state_ = SdOwnerState::Error;
    if (reason) {
        std::snprintf(lastError_, sizeof(lastError_), "%s", reason);
    }
}

} // namespace esp_bracket
