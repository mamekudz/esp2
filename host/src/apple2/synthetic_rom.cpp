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

RomError generateSyntheticEsp32TextPortRom(uint8_t *dst, size_t dstSize) {
    if (!dst || dstSize < Rom::kApple2PlusRomBytes) {
        return RomError::InvalidSize;
    }
    std::memset(dst, 0xEA, Rom::kApple2PlusRomBytes);

    const uint16_t entry = 0xE000;
    size_t i = 0;
    auto emit = [&](uint8_t b) {
        writeRomByte(dst, Rom::kApple2PlusRomBytes, static_cast<uint16_t>(entry + i), b);
        ++i;
    };

    // Soft switches: TEXT + PAGE1
    emit(0x8D);
    emit(0x51);
    emit(0xC0);
    emit(0x8D);
    emit(0x54);
    emit(0xC0);

    auto putRow = [&](int row, const char *msg) {
        for (uint16_t col = 0; msg[col]; ++col) {
            const uint8_t ch = static_cast<uint8_t>(0x80u | static_cast<uint8_t>(msg[col]));
            // Classic layout: pageBase + ((row&7)<<7) + ((row>>3)*40) + col
            const uint16_t addr =
                static_cast<uint16_t>(0x0400 + ((row & 7) << 7) + ((row >> 3) * 40) + col);
            emit(0xA9);
            emit(ch);
            emit(0x8D);
            emit(static_cast<uint8_t>(addr & 0xFF));
            emit(static_cast<uint8_t>((addr >> 8) & 0xFF));
        }
    };

    putRow(0, "ESP][");
    putRow(2, "ESP32-S3 PORT TEST");
    putRow(4, "6502        OK");
    putRow(5, "RAM         OK");
    putRow(6, "BUS         OK");
    putRow(7, "TEXT        OK");
    putRow(9, "CYCLES 000000000");

    // Markers $03FC–$03FF = A2 TX 1 (does not collide with Level-2/4 markers)
    emit(0xA9);
    emit(kEsp32TextPortMarker0);
    emit(0x8D);
    emit(0xFC);
    emit(0x03);
    emit(0xA9);
    emit(kEsp32TextPortMarker1);
    emit(0x8D);
    emit(0xFD);
    emit(0x03);
    emit(0xA9);
    emit(kEsp32TextPortMarker2);
    emit(0x8D);
    emit(0xFE);
    emit(0x03);
    emit(0xA9);
    emit(kEsp32TextPortMarker3);
    emit(0x8D);
    emit(0xFF);
    emit(0x03);

    const uint16_t loopAddr = static_cast<uint16_t>(entry + i);
    emit(0x4C);
    emit(static_cast<uint8_t>(loopAddr & 0xFF));
    emit(static_cast<uint8_t>((loopAddr >> 8) & 0xFF));

    writeRomWord(dst, Rom::kApple2PlusRomBytes, 0xFFFA, entry);
    writeRomWord(dst, Rom::kApple2PlusRomBytes, 0xFFFC, entry);
    writeRomWord(dst, Rom::kApple2PlusRomBytes, 0xFFFE, entry);
    return RomError::Ok;
}

RomError generateSyntheticVideoPipelineRom(uint8_t *dst, size_t dstSize) {
    // Start from text-port ROM (banner + idle), then add HGR clear + VP marker.
    const RomError base = generateSyntheticEsp32TextPortRom(dst, dstSize);
    if (base != RomError::Ok) {
        return base;
    }

    // $E800: clear HGR page 1 ($2000-$3FFF) via zero-page pointer — every store
    // is a real 6502 write (dirty tracking may coalesce display work).
    const uint16_t clr = kEsp32HgrClearRoutine;
    size_t i = 0;
    auto emitAt = [&](uint8_t b) {
        writeRomByte(dst, Rom::kApple2PlusRomBytes, static_cast<uint16_t>(clr + i), b);
        ++i;
    };
    emitAt(0xA9);
    emitAt(0x00); // LDA #0
    emitAt(0x85);
    emitAt(0x00); // STA $00
    emitAt(0xA9);
    emitAt(0x20); // LDA #$20
    emitAt(0x85);
    emitAt(0x01); // STA $01
    emitAt(0xA0);
    emitAt(0x00); // LDY #0
    emitAt(0xA9);
    emitAt(0x00); // LDA #0
    // clr_loop:
    const uint16_t loop = static_cast<uint16_t>(clr + i);
    emitAt(0x91);
    emitAt(0x00); // STA ($00),Y
    emitAt(0xC8); // INY
    emitAt(0xD0);
    emitAt(0xFB); // BNE clr_loop (-5)
    emitAt(0xE6);
    emitAt(0x01); // INC $01
    emitAt(0xA5);
    emitAt(0x01); // LDA $01
    emitAt(0xC9);
    emitAt(0x40); // CMP #$40
    emitAt(0xD0);
    emitAt(0xF3); // BNE clr_loop
    // Park here after clear (no RTS — avoids bogus stack return in diagnostic).
    const uint16_t done = static_cast<uint16_t>(clr + i);
    emitAt(0x4C);
    emitAt(static_cast<uint8_t>(done & 0xFF));
    emitAt(static_cast<uint8_t>((done >> 8) & 0xFF));
    (void)loop;

    // Video-pipeline marker (distinct from text-port A2TX1).
    writeRomByte(dst, Rom::kApple2PlusRomBytes, 0xE000 + 0, 0xEA); // keep entry
    // Write marker via short stub prepended? Simpler: store constants into ROM
    // image at a data table; firmware verifies after 6502 STA sequence.
    // Emit at end of clear: also write marker bytes with 6502 before RTS —
    // replace RTS with marker STAs then RTS.
    // Re-emit clear ending: already emitted RTS. Patch: overwrite last RTS area.
    // Instead write marker with dedicated tiny routine at $E840.
    i = 0;
    const uint16_t mk = 0xE840;
    auto emitMk = [&](uint8_t b) {
        writeRomByte(dst, Rom::kApple2PlusRomBytes, static_cast<uint16_t>(mk + i), b);
        ++i;
    };
    emitMk(0xA9);
    emitMk(kEsp32VideoPipeMarker0);
    emitMk(0x8D);
    emitMk(0xF8);
    emitMk(0x03);
    emitMk(0xA9);
    emitMk(kEsp32VideoPipeMarker1);
    emitMk(0x8D);
    emitMk(0xF9);
    emitMk(0x03);
    emitMk(0xA9);
    emitMk(kEsp32VideoPipeMarker2);
    emitMk(0x8D);
    emitMk(0xFA);
    emitMk(0x03);
    emitMk(0xA9);
    emitMk(kEsp32VideoPipeMarker3);
    emitMk(0x8D);
    emitMk(0xFB);
    emitMk(0x03);
    emitMk(0x60);

    return RomError::Ok;
}

} // namespace esp_bracket
