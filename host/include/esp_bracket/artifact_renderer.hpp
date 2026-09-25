#pragma once

#include <cstdint>
#include <cstddef>

#include "esp_bracket/types.hpp"

namespace esp_bracket {

/**
 * Deterministic educational composite / mono renderer.
 * Not a full NTSC electrical simulation.
 */
class ArtifactRenderer {
public:
    static constexpr int kWidth = 280;
    static constexpr int kHeight = 192;

    struct Rgb {
        uint8_t r, g, b;
    };

    /** Pair classification from adjacent bits + phase (from high bit). */
    static Rgb compositePair(bool b0, bool b1, bool phaseOdd);

    /**
     * Render CompositeColor or monochrome modes into RGB888.
     * Monochrome uses luminance from bits only — never desaturates composite.
     */
    static void render(const uint8_t bits280x192[kWidth * kHeight],
                       const uint8_t highBit40x192[40 * kHeight],
                       VideoColorMode mode, uint8_t* rgbOut, size_t rgbBytes);

    static Rgb monoColor(VideoColorMode mode, bool on);
};

} // namespace esp_bracket
