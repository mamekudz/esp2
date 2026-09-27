#pragma once

/**
 * Central phosphor / monitor appearance peaks for ESP][ presentation.
 *
 * These are V1 AMOLED-tuned defaults — not historical CRT calibration.
 * Keep literals here; do not scatter RGB565 constants in render loops.
 *
 * Defaults (peak “on” phosphor):
 *   White  — 255, 255, 255
 *   Green  —  40, 255,  72   (P1-ish green on CO5300)
 *   Amber  — 255, 168,  40   (warm amber)
 */

#include <cstdint>

#include "esp_bracket/types.hpp"

namespace esp_bracket {

struct PhosphorPeak {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

inline constexpr PhosphorPeak kPhosphorWhite{255, 255, 255};
inline constexpr PhosphorPeak kPhosphorGreen{40, 255, 72};
inline constexpr PhosphorPeak kPhosphorAmber{255, 168, 40};

/** Peak for monochrome monitor modes; CompositeColor returns white (unused). */
PhosphorPeak phosphorPeak(VideoColorMode mode);

/**
 * Map 0..255 luminance through a phosphor peak → RGB888.
 * Preserves intensity steps (not a single fixed “on” color).
 */
void mapLuminanceToPhosphor(VideoColorMode mode, uint8_t luminance, uint8_t *r, uint8_t *g,
                            uint8_t *b);

uint16_t mapLuminanceToPhosphorRgb565(VideoColorMode mode, uint8_t luminance);

/** Cheap Rec.601-ish luminance from RGB888 (integer). */
uint8_t luminanceFromRgb888(uint8_t r, uint8_t g, uint8_t b);

} // namespace esp_bracket
