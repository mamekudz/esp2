#pragma once

#include <cstdint>

#include "esp_bracket/types.hpp"

namespace esp_bracket {

struct Rect {
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
};

struct ViewportConfig {
    Rect physicalDisplay;
    Rect visibleEnclosure;
    Rect appleLogical;
    DisplayUiMode mode;
};

inline void mapAppleToPhysical(const ViewportConfig& cfg, int16_t ax, int16_t ay,
                               int16_t* px, int16_t* py) {
    if (!px || !py || cfg.appleLogical.w <= 0 || cfg.appleLogical.h <= 0) {
        return;
    }
    const int32_t nx = (int32_t)(ax - cfg.appleLogical.x);
    const int32_t ny = (int32_t)(ay - cfg.appleLogical.y);
    *px = (int16_t)(cfg.visibleEnclosure.x +
                    (nx * cfg.visibleEnclosure.w) / cfg.appleLogical.w);
    *py = (int16_t)(cfg.visibleEnclosure.y +
                    (ny * cfg.visibleEnclosure.h) / cfg.appleLogical.h);
}

} // namespace esp_bracket
