#include "esp_bracket/artifact_renderer.hpp"

namespace esp_bracket {

ArtifactRenderer::Rgb ArtifactRenderer::compositePair(bool b0, bool b1,
                                                      bool phaseOdd) {
    if (!b0 && !b1) {
        return {0, 0, 0};
    }
    if (b0 && b1) {
        return {255, 255, 255};
    }
    if (b0 && !b1) {
        return phaseOdd ? Rgb{32, 64, 255} : Rgb{192, 32, 192}; // blue / purple
    }
    // !b0 && b1
    return phaseOdd ? Rgb{255, 128, 0} : Rgb{32, 192, 32}; // orange / green
}

ArtifactRenderer::Rgb ArtifactRenderer::monoColor(VideoColorMode mode, bool on) {
    if (!on) {
        return {0, 0, 0};
    }
    switch (mode) {
    case VideoColorMode::MonochromeGreen:
        return {32, 255, 32};
    case VideoColorMode::MonochromeAmber:
        return {255, 176, 32};
    case VideoColorMode::MonochromeWhite:
    case VideoColorMode::CompositeColor:
    default:
        return {255, 255, 255};
    }
}

void ArtifactRenderer::render(const uint8_t bits280x192[kWidth * kHeight],
                              const uint8_t highBit40x192[40 * kHeight],
                              VideoColorMode mode, uint8_t* rgbOut,
                              size_t rgbBytes) {
    const size_t need = static_cast<size_t>(kWidth * kHeight * 3);
    if (!rgbOut || rgbBytes < need) {
        return;
    }

    const bool mono = mode != VideoColorMode::CompositeColor;

    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            Rgb c{};
            if (mono) {
                c = monoColor(mode, bits280x192[y * kWidth + x] != 0);
            } else {
                const int pair = x & ~1;
                const bool b0 = bits280x192[y * kWidth + pair] != 0;
                const bool b1 =
                    (pair + 1 < kWidth) && bits280x192[y * kWidth + pair + 1] != 0;
                const int bx = pair / 7;
                const bool phase = highBit40x192[y * 40 + bx] != 0;
                // Odd column groups use phase from high bit; educational model.
                c = compositePair(b0, b1, phase);
            }
            uint8_t* p = rgbOut + (static_cast<size_t>(y) * kWidth + x) * 3;
            p[0] = c.r;
            p[1] = c.g;
            p[2] = c.b;
        }
    }
}

} // namespace esp_bracket
