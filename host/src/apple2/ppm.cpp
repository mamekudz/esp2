#include "esp_bracket/ppm.hpp"

#include <cstdio>

namespace esp_bracket {

bool Ppm::writeP6(const char *path, int width, int height, const uint8_t *rgb888, size_t rgbBytes) {
    const size_t need = static_cast<size_t>(width) * height * 3;
    if (!path || !rgb888 || rgbBytes < need || width <= 0 || height <= 0) {
        return false;
    }
    FILE *f = std::fopen(path, "wb");
    if (!f) {
        return false;
    }
    std::fprintf(f, "P6\n%d %d\n255\n", width, height);
    const bool ok = std::fwrite(rgb888, 1, need, f) == need;
    std::fclose(f);
    return ok;
}

bool Ppm::writeP5(const char *path, int width, int height, const uint8_t *gray, size_t grayBytes) {
    const size_t need = static_cast<size_t>(width) * height;
    if (!path || !gray || grayBytes < need || width <= 0 || height <= 0) {
        return false;
    }
    FILE *f = std::fopen(path, "wb");
    if (!f) {
        return false;
    }
    std::fprintf(f, "P5\n%d %d\n255\n", width, height);
    const bool ok = std::fwrite(gray, 1, need, f) == need;
    std::fclose(f);
    return ok;
}

} // namespace esp_bracket
