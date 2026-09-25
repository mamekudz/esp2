#pragma once

#include <cstddef>
#include <cstdint>

namespace esp_bracket {

/** Minimal PPM/PGM writers for host golden/visual inspection. */
class Ppm {
  public:
    static bool writeP6(const char *path, int width, int height, const uint8_t *rgb888,
                        size_t rgbBytes);

    static bool writeP5(const char *path, int width, int height, const uint8_t *gray,
                        size_t grayBytes);
};

} // namespace esp_bracket
