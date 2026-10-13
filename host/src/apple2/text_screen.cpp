#include "esp_bracket/text_screen.hpp"

#include <cstring>

namespace esp_bracket {

void classifyTextByte(uint8_t ramByte, int flashPhase, char *outCh, TextCellStyle *outStyle) {
    uint8_t ascii7 = 0x20;
    bool inverse = false;
    TextDecoder::mapAppleTextByte(ramByte, flashPhase, &ascii7, &inverse);
    TextCellStyle style = TextCellStyle::Normal;
    if (ramByte < 0x40) {
        style = TextCellStyle::Inverse;
    } else if (ramByte < 0x80) {
        style = TextCellStyle::Flash;
    }
    if (outCh) {
        *outCh = static_cast<char>(ascii7);
    }
    if (outStyle) {
        *outStyle = style;
    }
    (void)inverse;
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
