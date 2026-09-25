#include "esp_bracket/lores_decoder.hpp"
#include "esp_bracket/text_decoder.hpp"

namespace esp_bracket {

void LoresDecoder::colorRgb(uint8_t nibble, uint8_t* r, uint8_t* g, uint8_t* b) {
    static const uint8_t kPalette[16][3] = {
        {0x00, 0x00, 0x00}, // 0 black
        {0x8A, 0x19, 0x41}, // 1 magenta
        {0x2F, 0x2C, 0xA4}, // 2 dark blue
        {0xD4, 0x3A, 0xE3}, // 3 purple
        {0x0F, 0x73, 0x40}, // 4 dark green
        {0x73, 0x73, 0x73}, // 5 gray1
        {0x34, 0x8F, 0xE3}, // 6 medium blue
        {0xA0, 0xB0, 0xF0}, // 7 light blue
        {0x64, 0x4C, 0x00}, // 8 brown
        {0xE2, 0x7C, 0x00}, // 9 orange
        {0x73, 0x73, 0x73}, // A gray2
        {0xF0, 0xA0, 0xB0}, // B pink
        {0x3C, 0xC0, 0x4E}, // C green
        {0xD0, 0xD0, 0x20}, // D yellow
        {0xA0, 0xE0, 0x90}, // E aqua
        {0xFF, 0xFF, 0xFF}, // F white
    };
    const uint8_t i = static_cast<uint8_t>(nibble & 0x0Fu);
    *r = kPalette[i][0];
    *g = kPalette[i][1];
    *b = kPalette[i][2];
}

void LoresDecoder::decode(const uint8_t* ram, uint16_t pageBase,
                          uint8_t out40x48[kRows * kCols]) {
    for (int trow = 0; trow < 24; ++trow) {
        for (int col = 0; col < kCols; ++col) {
            const uint16_t addr = TextDecoder::cellAddress(pageBase, trow, col);
            const uint8_t v = ram[addr];
            out40x48[(trow * 2) * kCols + col] = static_cast<uint8_t>(v & 0x0Fu);
            out40x48[(trow * 2 + 1) * kCols + col] =
                static_cast<uint8_t>((v >> 4) & 0x0Fu);
        }
    }
}

void LoresDecoder::renderRgb888(const uint8_t blocks40x48[kRows * kCols],
                                uint8_t* rgbOut, size_t rgbBytes) {
    const size_t need = static_cast<size_t>(kRgbW * kRgbH * 3);
    if (!rgbOut || rgbBytes < need) {
        return;
    }
    for (int by = 0; by < kRows; ++by) {
        for (int bx = 0; bx < kCols; ++bx) {
            uint8_t r, g, b;
            colorRgb(blocks40x48[by * kCols + bx], &r, &g, &b);
            for (int dy = 0; dy < kBlockH; ++dy) {
                for (int dx = 0; dx < kBlockW; ++dx) {
                    const int px = bx * kBlockW + dx;
                    const int py = by * kBlockH + dy;
                    uint8_t* p = rgbOut + (static_cast<size_t>(py) * kRgbW + px) * 3;
                    p[0] = r;
                    p[1] = g;
                    p[2] = b;
                }
            }
        }
    }
}

void LoresDecoder::writeTestPattern(uint8_t* ram, uint16_t pageBase) {
    for (int trow = 0; trow < 24; ++trow) {
        for (int col = 0; col < kCols; ++col) {
            const uint8_t lo = static_cast<uint8_t>((trow * 2 + col) & 0x0F);
            const uint8_t hi = static_cast<uint8_t>((trow * 2 + 1 + col) & 0x0F);
            ram[TextDecoder::cellAddress(pageBase, trow, col)] =
                static_cast<uint8_t>(lo | (hi << 4));
        }
    }
}

} // namespace esp_bracket
