#include "esp_bracket/disk_ii_boot.hpp"

#include "esp_bracket/apple2_bus.hpp"
#include "esp_bracket/disk_ii_encoding.hpp"

namespace esp_bracket {

namespace {

bool matchAddr(const uint8_t *p, uint8_t track, uint8_t sector) {
    // p points at byte after D5 AA 96
    const uint8_t vol = DiskIIEncoding::decodeOddEven(p[0], p[1]);
    const uint8_t tr = DiskIIEncoding::decodeOddEven(p[2], p[3]);
    const uint8_t se = DiskIIEncoding::decodeOddEven(p[4], p[5]);
    const uint8_t ck = DiskIIEncoding::decodeOddEven(p[6], p[7]);
    (void)vol;
    if (tr != track || se != sector) {
        return false;
    }
    return ck == static_cast<uint8_t>(vol ^ tr ^ se);
}

} // namespace

bool DiskIIHostBoot::decodeSectorFromStream(const uint8_t *stream, size_t len, uint8_t wantTrack,
                                            uint8_t wantSector, uint8_t out[256]) {
    if (!stream || !out || len < 400) {
        return false;
    }
    for (size_t i = 0; i + 12 < len; ++i) {
        if (stream[i] != 0xD5 || stream[i + 1] != 0xAA || stream[i + 2] != 0x96) {
            continue;
        }
        if (!matchAddr(stream + i + 3, wantTrack, wantSector)) {
            continue;
        }
        // Find data prologue after address epilogue
        for (size_t j = i + 12; j + 3 + DiskIIEncoding::kDataNibbles < len; ++j) {
            if (stream[j] == 0xD5 && stream[j + 1] == 0xAA && stream[j + 2] == 0xAD) {
                return DiskIIEncoding::decodeSector(stream + j + 3, out);
            }
            // Don't scan forever past this address field
            if (j > i + 50 && stream[j] == 0xD5 && stream[j + 1] == 0xAA && stream[j + 2] == 0x96) {
                break;
            }
        }
    }
    return false;
}

bool DiskIIHostBoot::readSector(Apple2Bus &bus, DiskIIController &disk, uint8_t track,
                                uint8_t sector, uint8_t out[256]) {
    DiskIIDriveState &st = disk.driveState(1);
    st.quarterTrack = static_cast<int>(track) * 4;

    bus.read(0xC0E8); // motor off — restart spin base
    bus.setAccessCycle(bus.accessCycle() + 10);
    bus.read(0xC0E9); // motor on
    bus.read(0xC0EA); // drive 1
    bus.read(0xC0EE); // Q7L
    bus.read(0xC0EC); // Q6L settle

    // Capture nibble stream via data register (cycle-tied rotation).
    static uint8_t stream[8192];
    size_t n = 0;
    uint32_t cycle = bus.accessCycle();
    for (size_t i = 0; i < sizeof(stream); ++i) {
        cycle += DiskIIController::kCyclesPerNibble;
        bus.setAccessCycle(cycle);
        stream[n++] = bus.read(0xC0EC);
    }
    return decodeSectorFromStream(stream, n, track, sector, out);
}

} // namespace esp_bracket
