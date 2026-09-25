#include "esp_bracket/hgr_decoder.hpp"

#include <cstring>

namespace esp_bracket {

uint16_t HgrDecoder::lineAddress(uint16_t pageBase, int y) {
    // Apple II HGR: group of 8 lines interleaved within 1K banks,
    // three 64-line thirds, 40 bytes per line.
    const int y0 = y & 7;
    const int y1 = (y >> 3) & 7;
    const int y2 = (y >> 6) & 3;
    const uint16_t off = static_cast<uint16_t>((y0 << 10) | (y1 << 7) | (y2 * 40));
    return static_cast<uint16_t>(pageBase + off);
}

void HgrDecoder::decode(const uint8_t* ram, uint16_t pageBase,
                        uint8_t bits280x192[kWidth * kHeight],
                        uint8_t highBit40x192[40 * kHeight]) {
    for (int y = 0; y < kHeight; ++y) {
        const uint16_t base = lineAddress(pageBase, y);
        for (int bx = 0; bx < 40; ++bx) {
            const uint8_t byte = ram[base + bx];
            highBit40x192[y * 40 + bx] = static_cast<uint8_t>((byte >> 7) & 1u);
            for (int bit = 0; bit < 7; ++bit) {
                const int x = bx * 7 + bit;
                bits280x192[y * kWidth + x] =
                    static_cast<uint8_t>((byte >> bit) & 1u);
            }
        }
    }
}

void HgrDecoder::setPixel(uint8_t* ram, uint16_t pageBase, int x, int y, bool on) {
    if (x < 0 || x >= kWidth || y < 0 || y >= kHeight) {
        return;
    }
    const int bx = x / 7;
    const int bit = x % 7;
    const uint16_t addr = static_cast<uint16_t>(lineAddress(pageBase, y) + bx);
    if (on) {
        ram[addr] = static_cast<uint8_t>(ram[addr] | (1u << bit));
    } else {
        ram[addr] = static_cast<uint8_t>(ram[addr] & ~(1u << bit));
    }
}

void HgrDecoder::writePattern(uint8_t* ram, uint16_t pageBase, const char* kind) {
    // Clear 8K HGR page
    std::memset(ram + pageBase, 0x00, 0x2000);
    if (!kind) {
        return;
    }
    if (std::strcmp(kind, "alt1010") == 0) {
        for (int y = 0; y < kHeight; ++y) {
            for (int x = 0; x < kWidth; ++x) {
                setPixel(ram, pageBase, x, y, (x & 1) == 0);
            }
        }
    } else if (std::strcmp(kind, "alt0101") == 0) {
        for (int y = 0; y < kHeight; ++y) {
            for (int x = 0; x < kWidth; ++x) {
                setPixel(ram, pageBase, x, y, (x & 1) == 1);
            }
        }
    } else if (std::strcmp(kind, "isolated") == 0) {
        for (int y = 0; y < kHeight; y += 7) {
            for (int x = 0; x < kWidth; x += 7) {
                setPixel(ram, pageBase, x, y, true);
            }
        }
    } else if (std::strcmp(kind, "hline") == 0) {
        for (int x = 0; x < kWidth; ++x) {
            setPixel(ram, pageBase, x, 96, true);
        }
    } else if (std::strcmp(kind, "vline") == 0) {
        for (int y = 0; y < kHeight; ++y) {
            setPixel(ram, pageBase, 140, y, true);
        }
    } else if (std::strcmp(kind, "checker") == 0) {
        for (int y = 0; y < kHeight; ++y) {
            for (int x = 0; x < kWidth; ++x) {
                setPixel(ram, pageBase, x, y, ((x / 4) ^ (y / 4)) & 1);
            }
        }
    } else if (std::strcmp(kind, "white_run") == 0) {
        for (int y = 0; y < kHeight; ++y) {
            for (int x = 40; x < 240; ++x) {
                setPixel(ram, pageBase, x, y, true);
            }
        }
    } else if (std::strcmp(kind, "highbit") == 0) {
        for (int y = 0; y < kHeight; ++y) {
            const uint16_t base = lineAddress(pageBase, y);
            for (int bx = 0; bx < 40; ++bx) {
                uint8_t v = 0x55; // alt pixels
                if (bx & 1) {
                    v |= 0x80; // phase
                }
                ram[base + bx] = v;
            }
        }
    }
}

} // namespace esp_bracket
