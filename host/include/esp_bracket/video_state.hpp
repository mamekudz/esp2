#pragma once

#include <cstdint>

#include "esp_bracket/apple2_machine.hpp"
#include "esp_bracket/soft_switches.hpp"
#include "esp_bracket/types.hpp"

namespace esp_bracket {

/** Snapshot of Apple II video soft-switch state. */
struct AppleIIVideoState {
    bool text = true;
    bool mixed = false;
    bool page2 = false;
    bool hires = false;

    bool isGraphics() const { return !text; }
    bool isLores() const { return !hires; }

    uint16_t textPageBase() const { return page2 ? 0x0800u : 0x0400u; }
    uint16_t hgrPageBase() const { return page2 ? 0x4000u : 0x2000u; }
};

AppleIIVideoState videoStateFromSoftSwitches(const SoftSwitches &sw);

/** Map to machine-facing VideoFrameState (colorMode supplied by caller). */
VideoFrameState toVideoFrameState(const AppleIIVideoState &vs, VideoColorMode colorMode);

} // namespace esp_bracket
