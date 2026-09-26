#pragma once

#include <cstddef>
#include <cstdint>

namespace esp_bracket {

/** Apple DOS 3.3 6-and-2 encode/decode helpers (no heap). */
class DiskIIEncoding {
  public:
    static constexpr size_t kSectorBytes = 256;
    static constexpr size_t kDataNibbles = 343; // 342 data + checksum nibble

    /** 6-bit → disk nibble (valid write values). */
    static uint8_t encode6and2Nibble(uint8_t sixBit);

    /** Disk nibble → 6-bit (0xFF if invalid). */
    static uint8_t decode6and2Nibble(uint8_t diskNibble);

    /**
     * Encode 256-byte sector → 343 nibbles (342 + checksum).
     * out must hold >= 343 bytes.
     */
    static bool encodeSector(const uint8_t data[kSectorBytes], uint8_t outNibbles[kDataNibbles]);

    /** Decode 343 nibbles → 256 bytes. */
    static bool decodeSector(const uint8_t inNibbles[kDataNibbles], uint8_t data[kSectorBytes]);

    /** Odd/even address-field byte pair (DOS 3.3). */
    static void encodeOddEven(uint8_t value, uint8_t *oddOut, uint8_t *evenOut);
    static uint8_t decodeOddEven(uint8_t odd, uint8_t even);
};

} // namespace esp_bracket
