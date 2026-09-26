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

    // Secondary I/O exercise entry at $E080 (JSR targets / host tests):
    // BIT $C030; BIT $C010; BIT $C070; LDA $C061; RTS
    const uint16_t ioEntry = 0xE080;
    writeRomByte(dst, Rom::kApple2PlusRomBytes, ioEntry, 0x2C); // BIT abs
    writeRomByte(dst, Rom::kApple2PlusRomBytes, static_cast<uint16_t>(ioEntry + 1), 0x30);
    writeRomByte(dst, Rom::kApple2PlusRomBytes, static_cast<uint16_t>(ioEntry + 2), 0xC0);
    writeRomByte(dst, Rom::kApple2PlusRomBytes, static_cast<uint16_t>(ioEntry + 3), 0x2C);
    writeRomByte(dst, Rom::kApple2PlusRomBytes, static_cast<uint16_t>(ioEntry + 4), 0x10);
    writeRomByte(dst, Rom::kApple2PlusRomBytes, static_cast<uint16_t>(ioEntry + 5), 0xC0);
    writeRomByte(dst, Rom::kApple2PlusRomBytes, static_cast<uint16_t>(ioEntry + 6), 0x2C);
    writeRomByte(dst, Rom::kApple2PlusRomBytes, static_cast<uint16_t>(ioEntry + 7), 0x70);
    writeRomByte(dst, Rom::kApple2PlusRomBytes, static_cast<uint16_t>(ioEntry + 8), 0xC0);
    writeRomByte(dst, Rom::kApple2PlusRomBytes, static_cast<uint16_t>(ioEntry + 9), 0xAD);
    writeRomByte(dst, Rom::kApple2PlusRomBytes, static_cast<uint16_t>(ioEntry + 10), 0x61);
    writeRomByte(dst, Rom::kApple2PlusRomBytes, static_cast<uint16_t>(ioEntry + 11), 0xC0);
    writeRomByte(dst, Rom::kApple2PlusRomBytes, static_cast<uint16_t>(ioEntry + 12), 0x60); // RTS

    // Disk-boot continuation at $E200 (entered via synthetic Slot-6 ROM JMP).
    // Marker $03FE=$EB then idle — host test completes sector load via Disk II latch.
    {
        const uint16_t diskBoot = 0xE200;
        writeRomByte(dst, Rom::kApple2PlusRomBytes, diskBoot, 0xA9); // LDA #$EB
        writeRomByte(dst, Rom::kApple2PlusRomBytes, static_cast<uint16_t>(diskBoot + 1), 0xEB);
        writeRomByte(dst, Rom::kApple2PlusRomBytes, static_cast<uint16_t>(diskBoot + 2),
                     0x8D); // STA $03FE
        writeRomByte(dst, Rom::kApple2PlusRomBytes, static_cast<uint16_t>(diskBoot + 3), 0xFE);
        writeRomByte(dst, Rom::kApple2PlusRomBytes, static_cast<uint16_t>(diskBoot + 4), 0x03);
        writeRomByte(dst, Rom::kApple2PlusRomBytes, static_cast<uint16_t>(diskBoot + 5),
                     0x4C); // JMP $E205
        writeRomByte(dst, Rom::kApple2PlusRomBytes, static_cast<uint16_t>(diskBoot + 6), 0x05);
        writeRomByte(dst, Rom::kApple2PlusRomBytes, static_cast<uint16_t>(diskBoot + 7), 0xE2);
    }

    // Reset / IRQ / NMI vectors at end of ROM ($FFFA–$FFFF)
    writeRomWord(dst, Rom::kApple2PlusRomBytes, 0xFFFA, entry); // NMI
    writeRomWord(dst, Rom::kApple2PlusRomBytes, 0xFFFC, entry); // RESET
    writeRomWord(dst, Rom::kApple2PlusRomBytes, 0xFFFE, entry); // IRQ/BRK

    return RomError::Ok;
}

} // namespace esp_bracket
