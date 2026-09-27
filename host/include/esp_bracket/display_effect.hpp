#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_bracket/types.hpp"

namespace esp_bracket {

/**
 * Lightweight display-effect pass (Clean / Monitor / CrtTv).
 *
 * Clean/Sharp + Off → no-op (safe default).
 * CrtTv V1: subtle horizontal softness + scanline dim + optional chroma bleed.
 * No barrel distortion, no multi-pass bloom, no persistence buffer.
 */
class DisplayEffect {
  public:
    /** RGB888 host path (existing). */
    static void apply(uint8_t *rgb, int width, int height, size_t rgbBytes, DisplayEffectMode mode,
                      EffectStrength strength);

    /**
     * In-place RGB565 (firmware viewport / host tests).
     * @param chromaBleed extra R/B horizontal bleed (Artifact Color + CRT only)
     */
    static void applyRgb565(uint16_t *fb, int width, int height, DisplayEffectMode mode,
                            EffectStrength strength, bool chromaBleed = false);
};

} // namespace esp_bracket
