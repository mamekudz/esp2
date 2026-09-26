#include "esp_bracket/text_screen.hpp"

#include <cstring>

namespace esp_bracket {

void classifyTextByte(uint8_t ramByte, int flashPhase, char *outCh, TextCellStyle *outStyle) {
    // Apple II text:
    //  $00–$3F inverse (uppercase set)
    //  $40–$7F flashing
    //  $80–$FF normal (high bit set)
    TextCellStyle style = TextCellStyle::Normal;
    uint8_t code = ramByte;
    if (ramByte < 0x40) {
        style = TextCellStyle::Inverse;
        code = static_cast<uint8_t>(ramByte + 0x40); // map to ASCII-ish uppercase
    } else if (ramByte < 0x80) {
        style = TextCellStyle::Flash;
        code = ramByte; // already ASCII range for many glyphs
        if ((flashPhase & 1) != 0) {
            code = ' ';
        }
    } else {
        style = TextCellStyle::Normal;
        code = static_cast<uint8_t>(ramByte & 0x7Fu);
    }
    char ch = static_cast<char>(code & 0x7Fu);
    if (ch < 0x20 || ch > 0x7E) {
        ch = ' ';
    }
    if (outCh) {
        *outCh = ch;
    }
    if (outStyle) {
        *outStyle = style;
    }
}

TextScreen TextScreen::fromRam(const uint8_t *ram, uint16_t pageBase, int flashPhase) {
    TextScreen s{};
    if (!ram) {
        return s;
    }
    for (int r = 0; r < kRows; ++r) {
        for (int c = 0; c < kCols; ++c) {
            const uint16_t addr = TextDecoder::cellAddress(pageBase, r, c);
            classifyTextByte(ram[addr], flashPhase, &s.cells[r][c].ch, &s.cells[r][c].style);
        }
    }
    return s;
}

TextScreen TextScreen::fromBus(const Apple2Bus &bus, int flashPhase) {
    const uint16_t base =
        bus.softSwitches().isPage2() ? TextDecoder::kPage2Base : TextDecoder::kPage1Base;
    // When GRAPHICS without MIXED, text page may still be readable for diagnostics.
    return fromRam(bus.ram(), base, flashPhase);
}

void TextScreen::rowString(int row, char out41[41]) const {
    if (!out41) {
        return;
    }
    if (row < 0 || row >= kRows) {
        out41[0] = '\0';
        return;
    }
    for (int c = 0; c < kCols; ++c) {
        out41[c] = cells[row][c].ch;
    }
    out41[kCols] = '\0';
}

void TextScreen::toLines(char *buf, size_t bufCap) const {
    if (!buf || bufCap == 0) {
        return;
    }
    size_t o = 0;
    for (int r = 0; r < kRows; ++r) {
        for (int c = 0; c < kCols; ++c) {
            if (o + 1 >= bufCap) {
                buf[o] = '\0';
                return;
            }
            buf[o++] = cells[r][c].ch;
        }
        if (r + 1 < kRows) {
            if (o + 1 >= bufCap) {
                buf[o] = '\0';
                return;
            }
            buf[o++] = '\n';
        }
    }
    if (o < bufCap) {
        buf[o] = '\0';
    } else {
        buf[bufCap - 1] = '\0';
    }
}

bool TextScreen::contains(const char *needle) const {
    if (!needle || !needle[0]) {
        return true;
    }
    char flat[kRows * (kCols + 1)];
    toLines(flat, sizeof(flat));
    return std::strstr(flat, needle) != nullptr;
}

} // namespace esp_bracket
