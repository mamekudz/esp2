#include "esp_bracket/disk_ii_cleanroom.hpp"

#include "esp_bracket/disk_ii_encoding.hpp"
#include "esp_bracket/media_types.hpp"

#include <cstring>

namespace esp_bracket {

namespace {

void bne(uint8_t *d, size_t &i, size_t target) {
    d[i++] = 0xD0;
    // Relative to PC after this 2-byte instruction (= offset-byte address + 1).
    d[i] = static_cast<uint8_t>(static_cast<int>(target) - static_cast<int>(i + 1));
    ++i;
}
void bpl(uint8_t *d, size_t &i, size_t target) {
    d[i++] = 0x10;
    d[i] = static_cast<uint8_t>(static_cast<int>(target) - static_cast<int>(i + 1));
    ++i;
}

} // namespace

bool generateCleanRoomDiskIICard(uint8_t *prom256, uint8_t *expansion2048) {
    if (!prom256 || !expansion2048) {
        return false;
    }
    std::memset(prom256, 0xEA, 256);
    std::memset(expansion2048, 0xEA, 2048);

    // Expansion $C800: denibble service request (ESP][ clean-room — NOT Apple).
    // Protocol: STA $03FA = $DE; wait until $03FA = 0; JMP $0800.
    // Host/firmware runs DiskIIEncoding::decodeSector($0900 → $0800).
    {
        uint8_t *d = expansion2048;
        size_t i = 0;
        auto b = [&](uint8_t v) { d[i++] = v; };
        b(0xA9);
        b(0xDE);
        b(0x8D);
        b(0xFA);
        b(0x03); // STA $03FA
        const size_t wait = i;
        b(0xAD);
        b(0xFA);
        b(0x03);
        bne(d, i, wait);
        b(0x4C);
        b(0x00);
        b(0x08); // JMP $0800
    }

    // PROM $C600: realistic Disk II find T0S0 + copy 343 nibbles → $0900 → JMP $C800
    {
        uint8_t *d = prom256;
        size_t i = 0;
        auto b = [&](uint8_t v) { d[i++] = v; };

        b(0xAD);
        b(0xE9);
        b(0xC0);
        b(0xAD);
        b(0xEA);
        b(0xC0);
        b(0xAD);
        b(0xEE);
        b(0xC0);

        b(0xA2);
        b(0x50);
        const size_t recal = i;
        b(0xAD);
        b(0xE1);
        b(0xC0);
        b(0xAD);
        b(0xE0);
        b(0xC0);
        b(0xCA);
        bne(d, i, recal);

        auto rd = [&]() {
            const size_t L = i;
            b(0xAD);
            b(0xEC);
            b(0xC0);
            bpl(d, i, L);
        };

        const size_t findD5 = i;
        rd();
        b(0xC9);
        b(0xD5);
        bne(d, i, findD5);
        rd();
        b(0xC9);
        b(0xAA);
        bne(d, i, findD5);
        rd();
        b(0xC9);
        b(0x96);
        bne(d, i, findD5);

        auto decByte = [&]() {
            rd();
            b(0x29);
            b(0x55);
            b(0x0A);
            b(0x85);
            b(0x02);
            rd();
            b(0x29);
            b(0x55);
            b(0x05);
            b(0x02);
        };

        decByte();
        decByte();
        b(0xC9);
        b(0x00);
        bne(d, i, findD5);
        decByte();
        b(0xC9);
        b(0x00);
        bne(d, i, findD5);
        decByte();

        const size_t findDat = i;
        rd();
        b(0xC9);
        b(0xD5);
        bne(d, i, findDat);
        rd();
        b(0xC9);
        b(0xAA);
        bne(d, i, findDat);
        rd();
        b(0xC9);
        b(0xAD);
        bne(d, i, findDat);

        b(0xA0);
        b(0x00);
        const size_t c1 = i;
        rd();
        b(0x99);
        b(0x00);
        b(0x09);
        b(0xC8);
        bne(d, i, c1);

        b(0xA0);
        b(0x00);
        const size_t c2 = i;
        rd();
        b(0x99);
        b(0x00);
        b(0x0A);
        b(0xC8);
        b(0xC0);
        b(0x57);
        bne(d, i, c2);

        b(0x4C);
        b(0x00);
        b(0xC8); // JMP $C800 denibble service
    }

    return true;
}

bool generateEsp2BootTestImage(uint8_t *dst, size_t dstCap) {
    if (!dst || dstCap < kDos33ImageBytes) {
        return false;
    }
    std::memset(dst, 0, kDos33ImageBytes);
    uint8_t *sec = dst;
    size_t j = 0;
    auto b = [&](uint8_t v) { sec[j++] = v; };

    b(0x8D);
    b(0x51);
    b(0xC0);
    b(0x8D);
    b(0x54);
    b(0xC0);

    auto put = [&](const char *msg, uint16_t base) {
        uint16_t col = 0;
        for (const char *p = msg; *p; ++p, ++col) {
            const uint8_t ch = static_cast<uint8_t>(0x80u | static_cast<uint8_t>(*p));
            const uint16_t addr = static_cast<uint16_t>(base + col);
            b(0xA9);
            b(ch);
            b(0x8D);
            b(static_cast<uint8_t>(addr & 0xFF));
            b(static_cast<uint8_t>((addr >> 8) & 0xFF));
        }
    };
    put("ESP][ LEVEL 4", 0x0400);
    put("DISK II BOOT OK", 0x0480);

    b(0xA9);
    b(0x4C);
    b(0x8D);
    b(0xFE);
    b(0x03);
    b(0xA9);
    b(0x34);
    b(0x8D);
    b(0xFF);
    b(0x03);

    const uint16_t loop = static_cast<uint16_t>(0x0800 + j);
    b(0x4C);
    b(static_cast<uint8_t>(loop & 0xFF));
    b(static_cast<uint8_t>((loop >> 8) & 0xFF));
    return true;
}

/** Service $03FA=$DE → decode $0900→$0800, clear $03FA. */
bool serviceCleanRoomDenibbleRequest(uint8_t *ram) {
    if (!ram || ram[0x03FA] != 0xDE) {
        return false;
    }
    uint8_t out[256];
    if (!DiskIIEncoding::decodeSector(ram + 0x0900, out)) {
        ram[0x03FA] = 0xFF; // error
        return false;
    }
    std::memcpy(ram + 0x0800, out, 256);
    ram[0x03FA] = 0x00;
    return true;
}

} // namespace esp_bracket
