#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_bracket/apple2_bus.hpp"
#include "esp_bracket/text_decoder.hpp"

namespace esp_bracket {

enum class TextCellStyle : uint8_t { Normal = 0, Inverse, Flash };

struct TextCell {
    char ch; // printable ASCII (flash resolved or raw glyph code & 0x7F)
    TextCellStyle style;
};

/**
 * Logical 24×40 text screen (independent of glyph artwork).
 * Flash phase is test-controlled (0 = show flash chars, 1 = blank flash chars).
 */
class TextScreen {
  public:
    static constexpr int kCols = TextDecoder::kCols;
    static constexpr int kRows = TextDecoder::kRows;

    TextCell cells[kRows][kCols]{};

    static TextScreen fromBus(const Apple2Bus &bus, int flashPhase = 0);
    static TextScreen fromRam(const uint8_t *ram, uint16_t pageBase, int flashPhase = 0);

    /** Flatten to NUL-terminated lines joined by '\n' into buf (needs >= 24*41). */
    void toLines(char *buf, size_t bufCap) const;

    /** Case-sensitive substring search across rows (spaces preserved). */
    bool contains(const char *needle) const;

    /** Row as C string into out[41]. */
    void rowString(int row, char out41[41]) const;
};

/**
 * Classify Apple text RAM byte into style + 7-bit code.
 * flashPhase: 0 = flashing chars visible, 1 = flashing chars blanked.
 */
void classifyTextByte(uint8_t ramByte, int flashPhase, char *outCh, TextCellStyle *outStyle);

} // namespace esp_bracket
