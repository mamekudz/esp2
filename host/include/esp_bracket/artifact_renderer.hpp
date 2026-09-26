#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_bracket/types.hpp"

namespace esp_bracket {

/**
 * Deterministic educational composite / mono renderer.
 *
 * Models digital HGR bit/phase → color classification (adjacent pair + high-bit
 * phase). This is **not** a full analog NTSC electrical signal chain.
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

    /** Deterministic RGB888 → RGB565 (same on host and ESP32). */
    static uint16_t toRgb565(Rgb c);

    /**
     * Render CompositeColor or monochrome modes into RGB888.
     * Monochrome uses luminance from bits only — never desaturates composite.
     */
    static void render(const uint8_t bits280x192[kWidth * kHeight],
                       const uint8_t highBit40x192[40 * kHeight], VideoColorMode mode,
                       uint8_t *rgbOut, size_t rgbBytes);

    /**
     * One scanline → RGB565 (280 pixels). Same compositePair semantics as
     * full-frame RGB888 render (exact after toRgb565 quantization).
     */
    static void renderScanlineRgb565(const uint8_t bits280[kWidth], const uint8_t highBit40[40],
                                     VideoColorMode mode, uint16_t outRgb565[kWidth]);

    static Rgb monoColor(VideoColorMode mode, bool on);
};

} // namespace esp_bracket
