#include "esp_bracket/display_effect.hpp"

namespace esp_bracket {

void DisplayEffect::apply(uint8_t* rgb, int width, int height, size_t rgbBytes,
                          DisplayEffectMode mode, EffectStrength strength) {
    const size_t need = static_cast<size_t>(width) * height * 3;
    if (!rgb || rgbBytes < need || width <= 0 || height <= 0) {
        return;
    }
    if (mode == DisplayEffectMode::Sharp || strength == EffectStrength::Off) {
        return;
    }

    // Subtle horizontal blend (Monitor / CrtTv). No heavy CRT simulation yet.
    const int amount = (strength == EffectStrength::Low)    ? 1
                       : (strength == EffectStrength::Medium) ? 2
                                                              : 3;
    for (int y = 0; y < height; ++y) {
        for (int x = width - 1; x >= 1; --x) {
            uint8_t* cur = rgb + (static_cast<size_t>(y) * width + x) * 3;
            uint8_t* prev = cur - 3;
            for (int c = 0; c < 3; ++c) {
                const int v =
                    (static_cast<int>(cur[c]) * (8 - amount) + static_cast<int>(prev[c]) * amount) /
                    8;
                cur[c] = static_cast<uint8_t>(v);
            }
        }
        if (mode == DisplayEffectMode::CrtTv && (y & 1)) {
            // Very light scanline dim
            for (int x = 0; x < width; ++x) {
                uint8_t* p = rgb + (static_cast<size_t>(y) * width + x) * 3;
                p[0] = static_cast<uint8_t>((p[0] * 7) / 8);
                p[1] = static_cast<uint8_t>((p[1] * 7) / 8);
                p[2] = static_cast<uint8_t>((p[2] * 7) / 8);
            }
        }
    }
}

} // namespace esp_bracket
