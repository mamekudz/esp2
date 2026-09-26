#pragma once

#include <cstdint>

namespace esp_bracket {

enum class SdOwnerState : uint8_t {
    Esp2OwnsSd = 0,
    TransitionToUsb,
    UsbOwnsSd,
    TransitionToEsp2,
    Error,
};

const char *sdOwnerStateName(SdOwnerState s);

/**
 * Exclusive microSD ownership — never allow ESP FAT and Windows MSC
 * to mount the same volume read/write at once.
 */
class SdOwnership {
  public:
    SdOwnerState state() const { return state_; }
    bool esp2MayUseFat() const { return state_ == SdOwnerState::Esp2OwnsSd; }
    bool usbOwnsBlock() const { return state_ == SdOwnerState::UsbOwnsSd; }

    void setState(SdOwnerState s);
    void setError(const char *reason);

    const char *lastError() const { return lastError_; }

  private:
    SdOwnerState state_ = SdOwnerState::Esp2OwnsSd;
    char lastError_[96]{};
};

} // namespace esp_bracket
