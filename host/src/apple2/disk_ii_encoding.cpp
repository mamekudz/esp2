#include "esp_bracket/disk_ii_encoding.hpp"

#include <cstring>

namespace esp_bracket {

namespace {

constexpr uint8_t kDiskByte[64] = {
    0x96, 0x97, 0x9A, 0x9B, 0x9D, 0x9E, 0x9F, 0xA6, 0xA7, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB2, 0xB3,
    0xB4, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xCB, 0xCD, 0xCE, 0xCF, 0xD3,
    0xD6, 0xD7, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 0xE5, 0xE6, 0xE7, 0xE9, 0xEA, 0xEB, 0xEC,
    0xED, 0xEE, 0xEF, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF,
};

uint8_t invertDiskByte(uint8_t n) {
    for (uint8_t i = 0; i < 64; ++i) {
        if (kDiskByte[i] == n) {
            return i;
        }
    }
    return 0xFF;
}

} // namespace

uint8_t DiskIIEncoding::encode6and2Nibble(uint8_t sixBit) {
    return kDiskByte[sixBit & 0x3Fu];
}

uint8_t DiskIIEncoding::decode6and2Nibble(uint8_t diskNibble) {
    return invertDiskByte(diskNibble);
}

void DiskIIEncoding::encodeOddEven(uint8_t value, uint8_t *oddOut, uint8_t *evenOut) {
    *oddOut = static_cast<uint8_t>(((value >> 1) & 0x55u) | 0xAAu);
    *evenOut = static_cast<uint8_t>((value & 0x55u) | 0xAAu);
}

uint8_t DiskIIEncoding::decodeOddEven(uint8_t odd, uint8_t even) {
    return static_cast<uint8_t>(((odd & 0x55u) << 1) | (even & 0x55u));
}

bool DiskIIEncoding::encodeSector(const uint8_t data[kSectorBytes],
                                  uint8_t outNibbles[kDataNibbles]) {
    // AppleWin-style Code62 prenibble (public technical algorithm).
    uint8_t nib[342];
    uint8_t offset = 0xAC;
    int idx = 0;

    while (offset != 0x02) {
        uint8_t value = 0;
#define ADDVALUE(a)                                                                                \
    value = static_cast<uint8_t>((value << 2) | (((a) & 0x01u) << 1) | (((a) & 0x02u) >> 1))
        ADDVALUE(data[offset]);
        offset = static_cast<uint8_t>(offset - 0x56);
        ADDVALUE(data[offset]);
        offset = static_cast<uint8_t>(offset - 0x56);
        ADDVALUE(data[offset]);
        offset = static_cast<uint8_t>(offset - 0x53);
#undef ADDVALUE
        nib[idx++] = static_cast<uint8_t>(value << 2);
    }
    nib[idx - 2] &= 0x3F;
    nib[idx - 1] &= 0x3F;
    for (int i = 0; i < 256; ++i) {
        nib[idx++] = data[i];
    }

    uint8_t saved = 0;
    for (int i = 0; i < 342; ++i) {
        const uint8_t x = static_cast<uint8_t>(saved ^ nib[i]);
        outNibbles[i] = encode6and2Nibble(static_cast<uint8_t>(x >> 2));
        saved = nib[i];
    }
    outNibbles[342] = encode6and2Nibble(static_cast<uint8_t>(saved >> 2));
    return true;
}

bool DiskIIEncoding::decodeSector(const uint8_t inNibbles[kDataNibbles],
                                  uint8_t data[kSectorBytes]) {
    uint8_t sb[343];
    for (int i = 0; i < 343; ++i) {
        const uint8_t v = decode6and2Nibble(inNibbles[i]);
        if (v == 0xFF) {
            return false;
        }
        sb[i] = static_cast<uint8_t>(v << 2);
    }

    uint8_t nib[342];
    uint8_t saved = 0;
    for (int i = 0; i < 342; ++i) {
        nib[i] = static_cast<uint8_t>(saved ^ sb[i]);
        saved = nib[i];
    }
    if (sb[342] != saved) {
        return false;
    }

    for (int i = 0; i < 256; ++i) {
        data[i] = static_cast<uint8_t>(nib[i + 0x56] & 0xFCu);
    }

    const uint8_t *lowbits = nib;
    uint8_t offset = 0xAC;
    while (offset != 0x02) {
        if (offset >= 0xAC) {
            data[offset] = static_cast<uint8_t>(
                (data[offset] & 0xFCu) | ((lowbits[0] & 0x80u) >> 7) | ((lowbits[0] & 0x40u) >> 5));
        }
        offset = static_cast<uint8_t>(offset - 0x56);
        data[offset] = static_cast<uint8_t>((data[offset] & 0xFCu) | ((lowbits[0] & 0x20u) >> 5) |
                                            ((lowbits[0] & 0x10u) >> 3));
        offset = static_cast<uint8_t>(offset - 0x56);
        data[offset] = static_cast<uint8_t>((data[offset] & 0xFCu) | ((lowbits[0] & 0x08u) >> 3) |
                                            ((lowbits[0] & 0x04u) >> 1));
        offset = static_cast<uint8_t>(offset - 0x53);
        ++lowbits;
    }
    return true;
}

} // namespace esp_bracket
