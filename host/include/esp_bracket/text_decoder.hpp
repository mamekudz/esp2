#pragma once

#include <cstddef>
#include <cstdint>

namespace esp_bracket {

/** Classic Apple II text memory: 40 columns × 24 rows, non-linear layout. */
class TextDecoder {
  public:
    static constexpr int kCols = 40;
    static constexpr int kRows = 24;
    static constexpr uint16_t kPage1Base = 0x0400;
    static constexpr uint16_t kPage2Base = 0x0800;

    /** addr = pageBase + ((row & 7) << 7) + ((row >> 3) * 40) + col */
    static uint16_t cellAddress(uint16_t pageBase, int row, int col);

    /**
     * Decode 40×24 screen. Each out[row*40+col]:
     *   bits0–6 = character code, bit7 = 1 if normal (high bit set in RAM),
     *   bit7 = 0 means inverse/flash style (high bit clear in Apple RAM).
     */
    static void decodeScreen(const uint8_t *ram, uint16_t pageBase,
                             uint8_t out40x24[kRows * kCols]);

    /** Write ASCII (7-bit) into text page with high bit set (normal). */
    static void writeTextScreen(uint8_t *ram, uint16_t pageBase, const char *text, int startRow = 0,
                                int startCol = 0);

    /**
     * Render decoded chars with project-owned 5×7 glyphs into RGB888.
     * Cell is 6×8 (1px gap). Buffer size: 240×192 pixels.
     */
    static constexpr int kGlyphW = 5;
    static constexpr int kGlyphH = 7;
    static constexpr int kCellW = 6;
    static constexpr int kCellH = 8;
    static constexpr int kRgbW = kCols * kCellW; // 240
    static constexpr int kRgbH = kRows * kCellH; // 192

    static void renderRgb888(const uint8_t chars40x24[kRows * kCols], uint8_t *rgbOut,
                             size_t rgbBytes, uint8_t fgR = 255, uint8_t fgG = 255,
                             uint8_t fgB = 255, uint8_t bgR = 0, uint8_t bgG = 0, uint8_t bgB = 0);

    /** Glyph row bitmask (5 LSBits), project-owned — NOT Apple char ROM. */
    static uint8_t glyphRow(uint8_t ascii7, int row);
};

} // namespace esp_bracket
