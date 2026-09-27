#include "esp_bracket/disk_ii_cleanroom.hpp"

#include "esp_bracket/disk_ii_boot.hpp"
#include "esp_bracket/disk_ii_controller.hpp"
#include "esp_bracket/disk_ii_encoding.hpp"
#include "esp_bracket/media_types.hpp"

#include <cstring>

namespace esp_bracket {

namespace {

/** Handshake: denibble $0900 → $0800 (boot stage). */
constexpr uint8_t kDenibbleReq = 0xDE;
/** Handshake: read sector ($41 track, $3D sector) → page $27. */
constexpr uint8_t kSectorReq = 0xD1;

void bne(uint8_t *d, size_t &i, size_t target) {
    d[i++] = 0xD0;
    d[i] = static_cast<uint8_t>(static_cast<int>(target) - static_cast<int>(i + 1));
    ++i;
}
void bpl(uint8_t *d, size_t &i, size_t target) {
    d[i++] = 0x10;
    d[i] = static_cast<uint8_t>(static_cast<int>(target) - static_cast<int>(i + 1));
    ++i;
}

/** Emit compact LDA abs / BPL spin-read of $C0EC. */
void emitRd(uint8_t *d, size_t &i) {
    const size_t L = i;
    d[i++] = 0xAD;
    d[i++] = 0xEC;
    d[i++] = 0xC0;
    bpl(d, i, L);
}

void emitDecOddEven(uint8_t *d, size_t &i) {
    emitRd(d, i);
    d[i++] = 0x29;
    d[i++] = 0x55;
    d[i++] = 0x0A;
    d[i++] = 0x85;
    d[i++] = 0x02;
    emitRd(d, i);
    d[i++] = 0x29;
    d[i++] = 0x55;
    d[i++] = 0x05;
    d[i++] = 0x02;
}

/**
 * Shared find-address+data prologue for track/sector in A/Y or fixed T0S0.
 * On entry: expects motor already on. Leaves nibbles at $0900 (343 bytes).
 * Uses ZP $02 scratch. For T0S0: compare decoded track==0 sector==0.
 */
size_t emitFindCopyT0S0(uint8_t *d, size_t i) {
    auto b = [&](uint8_t v) { d[i++] = v; };

    b(0xAD);
    b(0xE9);
    b(0xC0); // motor on
    b(0xAD);
    b(0xEA);
    b(0xC0); // drive 1
    b(0xAD);
    b(0xEE);
    b(0xC0); // Q7L read

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

    const size_t findD5 = i;
    emitRd(d, i);
    b(0xC9);
    b(0xD5);
    bne(d, i, findD5);
    emitRd(d, i);
    b(0xC9);
    b(0xAA);
    bne(d, i, findD5);
    emitRd(d, i);
    b(0xC9);
    b(0x96);
    bne(d, i, findD5);

    emitDecOddEven(d, i); // volume
    emitDecOddEven(d, i); // track
    b(0xC9);
    b(0x00);
    bne(d, i, findD5);
    emitDecOddEven(d, i); // sector
    b(0xC9);
    b(0x00);
    bne(d, i, findD5);
    emitDecOddEven(d, i); // checksum (ignored match)

    const size_t findDat = i;
    emitRd(d, i);
    b(0xC9);
    b(0xD5);
    bne(d, i, findDat);
    emitRd(d, i);
    b(0xC9);
    b(0xAA);
    bne(d, i, findDat);
    emitRd(d, i);
    b(0xC9);
    b(0xAD);
    bne(d, i, findDat);

    b(0xA0);
    b(0x00);
    const size_t c1 = i;
    emitRd(d, i);
    b(0x99);
    b(0x00);
    b(0x09);
    b(0xC8);
    bne(d, i, c1);

    b(0xA0);
    b(0x00);
    const size_t c2 = i;
    emitRd(d, i);
    b(0x99);
    b(0x00);
    b(0x0A);
    b(0xC8);
    b(0xC0);
    b(0x57);
    bne(d, i, c2);

    return i;
}

} // namespace

bool generateCleanRoomDiskIICard(uint8_t *prom256, uint8_t *expansion2048) {
    if (!prom256 || !expansion2048) {
        return false;
    }
    // PROM: Apple Disk II–compatible vector layout (project-owned code, not Apple bytes).
    // $C600 → expansion boot; $C65C → expansion sector-read (DOS boot0 JMP $Cn5C).
    std::memset(prom256, 0x60, 256); // RTS filler (safe if stray calls)
    prom256[0x00] = 0x4C;
    prom256[0x01] = 0x00;
    prom256[0x02] = 0xC8; // JMP $C800 boot
    prom256[0x5C] = 0x4C;
    prom256[0x5D] = 0x00;
    prom256[0x5E] = 0xC9; // JMP $C900 sector-read

    std::memset(expansion2048, 0x60, 2048);

    // ----- $C800: boot T0S0 → denibble handshake → JMP $0801 (Disk II convention) -----
    {
        uint8_t *d = expansion2048;
        size_t i = emitFindCopyT0S0(d, 0);
        // Disk II PROM convention before entering boot sector:
        //   $2B = slot*16 ($60 for slot 6)
        //   $27 = $09 (page param so boot0 patches JMP $Cn5C)
        d[i++] = 0xA9;
        d[i++] = 0x60;
        d[i++] = 0x85;
        d[i++] = 0x2B; // LDA #$60 / STA $2B
        d[i++] = 0xA9;
        d[i++] = 0x09;
        d[i++] = 0x85;
        d[i++] = 0x27; // LDA #$09 / STA $27
        // DOS boot0 uses ZP $41 as track and $3D as sector. After T0S0 the
        // controller is on track 0; real PROM-era state leaves track=0. Do not
        // rely on power-on RAM zeros (Ones/garbage fill must still boot).
        d[i++] = 0xA9;
        d[i++] = 0x00;
        d[i++] = 0x85;
        d[i++] = 0x41; // LDA #0 / STA $41 (track)
        d[i++] = 0x85;
        d[i++] = 0x3D; // STA $3D (sector scratch; boot0 reloads before $Cn5C)
        // Match Disk II PROM exit: Y=0 after sector/nibble store (boot0/boot1 rely on it).
        d[i++] = 0xA0;
        d[i++] = 0x00; // LDY #$00
        // Denibble handshake at $03FA
        d[i++] = 0xA9;
        d[i++] = kDenibbleReq;
        d[i++] = 0x8D;
        d[i++] = 0xFA;
        d[i++] = 0x03; // STA $03FA
        const size_t wait = i;
        d[i++] = 0xAD;
        d[i++] = 0xFA;
        d[i++] = 0x03;
        bne(d, i, wait);
        // Authentic Disk II entry: code starts at $0801 (byte $0800 = sector count).
        d[i++] = 0x4C;
        d[i++] = 0x01;
        d[i++] = 0x08; // JMP $0801
    }

    // ----- $C900: sector-read for DOS boot0 (params in ZP, then JMP $0801) -----
    // Protocol: STA $03F9 = $D1; host fills page ($27) from track ($41) sector ($3D);
    // wait until $03F9 cleared; LDY #0; JMP $0801 (same re-entry as Apple Disk II PROM).
    {
        uint8_t *d = expansion2048 + 0x100; // $C900
        size_t i = 0;
        d[i++] = 0xA9;
        d[i++] = kSectorReq;
        d[i++] = 0x8D;
        d[i++] = 0xF9;
        d[i++] = 0x03; // STA $03F9
        const size_t wait = i;
        d[i++] = 0xAD;
        d[i++] = 0xF9;
        d[i++] = 0x03;
        bne(d, i, wait);
        d[i++] = 0xA0;
        d[i++] = 0x00; // LDY #$00 — PROM sector store leaves Y wrapped to 0
        d[i++] = 0x4C;
        d[i++] = 0x01;
        d[i++] = 0x08; // JMP $0801
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

    // DOS 3.3 convention: $0800 = parameter byte; execution starts at $0801.
    b(0x01);

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

bool serviceCleanRoomDenibbleRequest(uint8_t *ram) {
    if (!ram || ram[0x03FA] != kDenibbleReq) {
        return false;
    }
    uint8_t out[256];
    if (!DiskIIEncoding::decodeSector(ram + 0x0900, out)) {
        ram[0x03FA] = 0xFF;
        return false;
    }
    std::memcpy(ram + 0x0800, out, 256);
    ram[0x03FA] = 0x00;
    return true;
}

bool serviceCleanRoomSectorRequest(uint8_t *ram, DiskIIController &disk) {
    if (!ram || ram[0x03F9] != kSectorReq) {
        return false;
    }
    const uint8_t track = ram[0x41];
    const uint8_t sector = ram[0x3D];
    const uint8_t page = ram[0x27];
    if (page < 0x02 || page >= 0xC0 || track >= 35 || sector >= 16) {
        ram[0x03F9] = 0xFF;
        return false;
    }

    NibbleTrackMedia *media = disk.media(1);
    if (!media || !media->inserted()) {
        ram[0x03F9] = 0xFF;
        return false;
    }

    // Decode from nibble track (no RTTI — works for DSK/PO/NIB on host + ESP32).
    size_t len = 0;
    const uint8_t *stream = media->trackNibbles(static_cast<int>(track), &len);
    uint8_t out[256];
    if (!stream || !DiskIIHostBoot::decodeSectorFromStream(stream, len, track, sector, out)) {
        ram[0x03F9] = 0xFF;
        return false;
    }
    std::memcpy(ram + (static_cast<size_t>(page) << 8), out, 256);
    ram[0x03F9] = 0x00;
    return true;
}

bool serviceCleanRoomCardRequests(uint8_t *ram, DiskIIController &disk) {
    bool any = false;
    if (serviceCleanRoomDenibbleRequest(ram)) {
        any = true;
    }
    if (serviceCleanRoomSectorRequest(ram, disk)) {
        any = true;
    }
    return any;
}

} // namespace esp_bracket
