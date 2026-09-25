#pragma once

#include <cstdint>
#include <cstddef>

namespace esp_bracket {

/**
 * Apple II HiRes memory decoder (280×192).
 * Scanline addresses are non-linear; each of 40 bytes contributes 7 pixels
 * plus a high-bit (phase) flag.
 */
class HgrDecoder {
public:
    static constexpr int kWidth = 280;
    static constexpr int kHeight = 192;
    static constexpr uint16_t kPage1Base = 0x2000;
    static constexpr uint16_t kPage2Base = 0x4000;

    /** Classic HGR line base: y in 0..191 → offset from page base. */
    static uint16_t lineAddress(uint16_t pageBase, int y);

    /**
     * Decode into bits[y*280+x] = 0/1 and highBit[y*40 + byteCol] = byte bit7.
     */
    static void decode(const uint8_t* ram, uint16_t pageBase,
                       uint8_t bits280x192[kWidth * kHeight],
                       uint8_t highBit40x192[40 * kHeight]);

    /** Set a single logical pixel (affects bit inside HGR byte). */
    static void setPixel(uint8_t* ram, uint16_t pageBase, int x, int y, bool on);

    /** Write deterministic patterns for golden/regression tests. */
    static void writePattern(uint8_t* ram, uint16_t pageBase, const char* kind);
};

} // namespace esp_bracket
