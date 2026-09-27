#include "esp_bracket/phosphor.hpp"

#include "esp_bracket/artifact_renderer.hpp"

namespace esp_bracket {

PhosphorPeak phosphorPeak(VideoColorMode mode) {
    switch (mode) {
    case VideoColorMode::MonochromeGreen:
        return kPhosphorGreen;
    case VideoColorMode::MonochromeAmber:
        return kPhosphorAmber;
    case VideoColorMode::MonochromeWhite:
    case VideoColorMode::CompositeColor:
    default:
        return kPhosphorWhite;
    }
}

void mapLuminanceToPhosphor(VideoColorMode mode, uint8_t luminance, uint8_t *r, uint8_t *g,
                            uint8_t *b) {
    if (!r || !g || !b) {
        return;
    }
    if (luminance == 0) {
        *r = *g = *b = 0;
        return;
    }
    const PhosphorPeak peak = phosphorPeak(mode);
    *r = static_cast<uint8_t>((static_cast<uint16_t>(peak.r) * luminance) / 255u);
    *g = static_cast<uint8_t>((static_cast<uint16_t>(peak.g) * luminance) / 255u);
    *b = static_cast<uint8_t>((static_cast<uint16_t>(peak.b) * luminance) / 255u);
}

uint16_t mapLuminanceToPhosphorRgb565(VideoColorMode mode, uint8_t luminance) {
    uint8_t r = 0, g = 0, b = 0;
    mapLuminanceToPhosphor(mode, luminance, &r, &g, &b);
    return ArtifactRenderer::toRgb565({r, g, b});
}

uint8_t luminanceFromRgb888(uint8_t r, uint8_t g, uint8_t b) {
    // (2R + 5G + B) / 8 — green-weighted, integer-only
    return static_cast<uint8_t>(
        (static_cast<uint16_t>(r) * 2u + static_cast<uint16_t>(g) * 5u + static_cast<uint16_t>(b)) /
        8u);
}

} // namespace esp_bracket
