#pragma once

#include <cstdint>

namespace esp_bracket {

enum class VideoColorMode : uint8_t {
    CompositeColor = 0,
    MonochromeWhite,
    MonochromeGreen,
    MonochromeAmber
};

enum class DisplayEffectMode : uint8_t { Sharp = 0, Monitor, CrtTv };

enum class EffectStrength : uint8_t { Off = 0, Low, Medium, High };

enum class DisplayUiMode : uint8_t {
    LandscapeStandalone = 0,
    PortraitApple2Case,
    ControlScreen
};

} // namespace esp_bracket
