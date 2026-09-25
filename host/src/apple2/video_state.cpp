#include "esp_bracket/video_state.hpp"

namespace esp_bracket {

AppleIIVideoState videoStateFromSoftSwitches(const SoftSwitches &sw) {
    AppleIIVideoState vs;
    vs.text = sw.isText();
    vs.mixed = sw.isMixed();
    vs.page2 = sw.isPage2();
    vs.hires = sw.isHires();
    return vs;
}

VideoFrameState toVideoFrameState(const AppleIIVideoState &vs, VideoColorMode colorMode) {
    VideoFrameState frame{};
    frame.textPage = vs.page2 ? 2 : 1;
    frame.hires = vs.hires && !vs.text;
    frame.mixed = vs.mixed;
    frame.page2 = vs.page2;
    frame.colorMode = colorMode;
    return frame;
}

} // namespace esp_bracket
