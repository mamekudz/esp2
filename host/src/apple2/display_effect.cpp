#include "esp_bracket/display_effect.hpp"

namespace esp_bracket {
namespace {

int blendAmount(EffectStrength strength) {
    switch (strength) {
    case EffectStrength::Low:
        return 1;
    case EffectStrength::Medium:
        return 2;
    case EffectStrength::High:
        return 3;
    case EffectStrength::Off:
    default:
        return 0;
    }
}

inline void unpack565(uint16_t c, int *r, int *g, int *b) {
    *r = (c >> 11) & 0x1F;
    *g = (c >> 5) & 0x3F;
    *b = c & 0x1F;
}

inline uint16_t pack565(int r, int g, int b) {
    if (r < 0) {
        r = 0;
    }
    if (g < 0) {
        g = 0;
    }
    if (b < 0) {
        b = 0;
    }
    if (r > 31) {
        r = 31;
    }
    if (g > 63) {
        g = 63;
    }
    if (b > 31) {
        b = 31;
    }
    return static_cast<uint16_t>((r << 11) | (g << 5) | b);
}

} // namespace

void DisplayEffect::apply(uint8_t *rgb, int width, int height, size_t rgbBytes,
                          DisplayEffectMode mode, EffectStrength strength) {
    const size_t need = static_cast<size_t>(width) * height * 3;
    if (!rgb || rgbBytes < need || width <= 0 || height <= 0) {
        return;
    }
    if (mode == DisplayEffectMode::Sharp || strength == EffectStrength::Off) {
        return;
    }

    const int amount = blendAmount(strength);
    if (amount <= 0) {
        return;
    }
    for (int y = 0; y < height; ++y) {
        for (int x = width - 1; x >= 1; --x) {
            uint8_t *cur = rgb + (static_cast<size_t>(y) * width + x) * 3;
            uint8_t *prev = cur - 3;
            for (int c = 0; c < 3; ++c) {
                const int v =
                    (static_cast<int>(cur[c]) * (8 - amount) + static_cast<int>(prev[c]) * amount) /
                    8;
                cur[c] = static_cast<uint8_t>(v);
            }
        }
        if (mode == DisplayEffectMode::CrtTv && (y & 1)) {
            for (int x = 0; x < width; ++x) {
                uint8_t *p = rgb + (static_cast<size_t>(y) * width + x) * 3;
                p[0] = static_cast<uint8_t>((p[0] * 7) / 8);
                p[1] = static_cast<uint8_t>((p[1] * 7) / 8);
                p[2] = static_cast<uint8_t>((p[2] * 7) / 8);
            }
        }
    }
}

void DisplayEffect::applyRgb565(uint16_t *fb, int width, int height, DisplayEffectMode mode,
                                EffectStrength strength, bool chromaBleed) {
    if (!fb || width <= 0 || height <= 0) {
        return;
    }
    if (mode == DisplayEffectMode::Sharp || strength == EffectStrength::Off) {
        return;
    }

    const int amount = blendAmount(strength);
    if (amount <= 0) {
        return;
    }
    // Softness weight; chroma bleed adds a little more on R/B only (Artifact+CRT).
    const int chromaAmt = chromaBleed ? (amount + 1) : amount;

    for (int y = 0; y < height; ++y) {
        uint16_t *row = fb + static_cast<size_t>(y) * width;
        for (int x = width - 1; x >= 1; --x) {
            int r, g, b, pr, pg, pb;
            unpack565(row[x], &r, &g, &b);
            unpack565(row[x - 1], &pr, &pg, &pb);
            const int nr = (r * (8 - chromaAmt) + pr * chromaAmt) / 8;
            const int ng = (g * (8 - amount) + pg * amount) / 8;
            const int nb = (b * (8 - chromaAmt) + pb * chromaAmt) / 8;
            row[x] = pack565(nr, ng, nb);
        }
        if (mode == DisplayEffectMode::CrtTv && (y & 1)) {
            // Subtle scanline: ~12.5% dim — not every-other-line black.
            for (int x = 0; x < width; ++x) {
                int r, g, b;
                unpack565(row[x], &r, &g, &b);
                row[x] = pack565((r * 7) / 8, (g * 7) / 8, (b * 7) / 8);
            }
        }
    }
}

} // namespace esp_bracket
