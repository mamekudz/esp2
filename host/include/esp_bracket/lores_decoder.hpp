#pragma once

#include <cstdint>
#include <cstddef>

namespace esp_bracket {

/** Apple II LoRes: 40×48 blocks sharing text-page memory. */
class LoresDecoder {
public:
    static constexpr int kCols = 40;
    static constexpr int kRows = 48; // 24 text rows × 2 nibbles

    /** Classic 16 LoRes colors as RGB888. */
    static void colorRgb(uint8_t nibble, uint8_t* r, uint8_t* g, uint8_t* b);

    /**
     * Decode LoRes into out[row*40+col] = color nibble 0..15.
     * Upper nibble = even visual row within text row; lower = odd.
     */
    static void decode(const uint8_t* ram, uint16_t pageBase,
                       uint8_t out40x48[kRows * kCols]);

    /** Render 40×48 → RGB888 scaled 7×4 per block → 280×192. */
    static constexpr int kBlockW = 7;
    static constexpr int kBlockH = 4;
    static constexpr int kRgbW = kCols * kBlockW; // 280
    static constexpr int kRgbH = kRows * kBlockH; // 192

    static void renderRgb888(const uint8_t blocks40x48[kRows * kCols],
                             uint8_t* rgbOut, size_t rgbBytes);

    /** Fill deterministic palette test pattern into text page memory. */
    static void writeTestPattern(uint8_t* ram, uint16_t pageBase);
};

} // namespace esp_bracket
