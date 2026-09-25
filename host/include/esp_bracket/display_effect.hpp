#pragma once

#include <cstdint>
#include <cstddef>

#include "esp_bracket/types.hpp"

namespace esp_bracket {

/**
 * Lightweight host display-effect pass (Sharp / Monitor / CrtTv).
 * Correctness of Apple II decode comes first — effects are subtle stubs.
 */
class DisplayEffect {
public:
    static void apply(uint8_t* rgb, int width, int height, size_t rgbBytes,
                      DisplayEffectMode mode, EffectStrength strength);
};

} // namespace esp_bracket
