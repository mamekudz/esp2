#include "esp_bracket/rom.hpp"

#include <cstring>

namespace esp_bracket {

namespace {

void writeRomByte(uint8_t *rom, size_t romSize, uint16_t absAddr, uint8_t v) {
    if (absAddr < Rom::kMapBase) {
        return;
    }
    const size_t off = static_cast<size_t>(absAddr - Rom::kMapBase);
    if (off < romSize) {
        rom[off] = v;
    }
}

void writeRomWord(uint8_t *rom, size_t romSize, uint16_t absAddr, uint16_t v) {
    writeRomByte(rom, romSize, absAddr, static_cast<uint8_t>(v & 0xFF));
    writeRomByte(rom, romSize, static_cast<uint16_t>(absAddr + 1),
                 static_cast<uint8_t>((v >> 8) & 0xFF));
}

} // namespace

/**
 * Synthetic 12K ROM (NOT an Apple ROM).
 *
 * Reset vector → small program at $E000 that:
 *   - clears text soft switches toward TEXT/PAGE1
 *   - writes "ESP][ HOST TEST" into text page 1 via known cell addresses
 *   - then loops (JMP *)
 *
 * Useful for host bring-up without copyrighted firmware.
 */
RomError generateSyntheticRom(uint8_t *dst, size_t dstSize) {
    if (!dst || dstSize < Rom::kApple2PlusRomBytes) {
        return RomError::InvalidSize;
    }
    std::memset(dst, 0xEA, Rom::kApple2PlusRomBytes); // fill with NOP

    // Program at $E000
    const uint16_t entry = 0xE000;
    size_t i = 0;
    auto emit = [&](uint8_t b) {
        writeRomByte(dst, Rom::kApple2PlusRomBytes, static_cast<uint16_t>(entry + i), b);
        ++i;
    };

    // LDA #$8E / STA $400 ... for each character via absolute stores.
    // Soft switches: STA $C051 (TEXT), STA $C054 (PAGE1)
    emit(0x8D);
    emit(0x51);
    emit(0xC0); // STA $C051
    emit(0x8D);
    emit(0x54);
    emit(0xC0); // STA $C054

    const char *msg = "ESP][ HOST TEST";
    // Place on text row 0 using classic layout: addr = $400 + col
    uint16_t col = 0;
    for (const char *p = msg; *p; ++p, ++col) {
        const uint8_t ch = static_cast<uint8_t>(0x80u | static_cast<uint8_t>(*p));
        const uint16_t addr = static_cast<uint16_t>(0x0400 + col);
        emit(0xA9);
        emit(ch); // LDA #ch
        emit(0x8D);
        emit(static_cast<uint8_t>(addr & 0xFF));
        emit(static_cast<uint8_t>((addr >> 8) & 0xFF)); // STA abs
    }

    // Infinite loop
    const uint16_t loopAddr = static_cast<uint16_t>(entry + i);
    emit(0x4C);
    emit(static_cast<uint8_t>(loopAddr & 0xFF));
    emit(static_cast<uint8_t>((loopAddr >> 8) & 0xFF)); // JMP loop

    // Reset / IRQ / NMI vectors at end of ROM ($FFFA–$FFFF)
    writeRomWord(dst, Rom::kApple2PlusRomBytes, 0xFFFA, entry); // NMI
    writeRomWord(dst, Rom::kApple2PlusRomBytes, 0xFFFC, entry); // RESET
    writeRomWord(dst, Rom::kApple2PlusRomBytes, 0xFFFE, entry); // IRQ/BRK

    return RomError::Ok;
}

} // namespace esp_bracket
