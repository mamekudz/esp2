#include "esp_bracket/disk_ii_track.hpp"
#include "esp_bracket/disk_ii_encoding.hpp"

namespace esp_bracket {

namespace {

// Physical sector order on a DOS 3.3 track (skew).
constexpr int kPhysOrder[16] = {0x0, 0x7, 0xE, 0x6, 0xD, 0x5, 0xC, 0x4,
                                0xB, 0x3, 0xA, 0x2, 0x9, 0x1, 0x8, 0xF};

void append(uint8_t *&p, uint8_t *end, uint8_t v) {
    if (p < end) {
        *p++ = v;
    }
}

void appendGap(uint8_t *&p, uint8_t *end, int count, uint8_t fill = 0xFF) {
    for (int i = 0; i < count; ++i) {
        append(p, end, fill);
    }
}

void appendSelfSync(uint8_t *&p, uint8_t *end, int count) {
    // Simplified self-sync as 0xFF run (common host approximation).
    appendGap(p, end, count, 0xFF);
}

} // namespace

int DiskIITrackBuilder::logicalToPhysical(int logicalSector) {
    for (int i = 0; i < 16; ++i) {
        if (kPhysOrder[i] == logicalSector) {
            return i;
        }
    }
    return logicalSector & 0x0F;
}

int DiskIITrackBuilder::poSectorToLogical(int poSector) {
    // ProDOS order on 16-sector floppy: common mapping used by emulators.
    static const int kPoToDos[16] = {0x0, 0x8, 0x1, 0x9, 0x2, 0xA, 0x3, 0xB,
                                     0x4, 0xC, 0x5, 0xD, 0x6, 0xE, 0x7, 0xF};
    return kPoToDos[poSector & 0x0F];
}

size_t DiskIITrackBuilder::buildTrack(uint8_t trackNumber, uint8_t volume,
                                      const uint8_t sectors[kSectors][kSectorBytes], uint8_t *out,
                                      size_t outCap) {
    if (!out || outCap < 512) {
        return 0;
    }
    uint8_t *p = out;
    uint8_t *end = out + outCap;

    appendSelfSync(p, end, 64);

    for (int phys = 0; phys < 16; ++phys) {
        const int logical = kPhysOrder[phys];
        appendSelfSync(p, end, 14);

        // Address field
        append(p, end, 0xD5);
        append(p, end, 0xAA);
        append(p, end, 0x96);
        uint8_t o, e;
        DiskIIEncoding::encodeOddEven(volume, &o, &e);
        append(p, end, o);
        append(p, end, e);
        DiskIIEncoding::encodeOddEven(trackNumber, &o, &e);
        append(p, end, o);
        append(p, end, e);
        DiskIIEncoding::encodeOddEven(static_cast<uint8_t>(logical), &o, &e);
        append(p, end, o);
        append(p, end, e);
        const uint8_t csum = static_cast<uint8_t>(volume ^ trackNumber ^ logical);
        DiskIIEncoding::encodeOddEven(csum, &o, &e);
        append(p, end, o);
        append(p, end, e);
        append(p, end, 0xDE);
        append(p, end, 0xAA);
        append(p, end, 0xEB);

        appendSelfSync(p, end, 5);

        // Data field
        append(p, end, 0xD5);
        append(p, end, 0xAA);
        append(p, end, 0xAD);
        uint8_t nib[DiskIIEncoding::kDataNibbles];
        if (!DiskIIEncoding::encodeSector(sectors[logical], nib)) {
            return 0;
        }
        for (size_t i = 0; i < DiskIIEncoding::kDataNibbles; ++i) {
            append(p, end, nib[i]);
        }
        append(p, end, 0xDE);
        append(p, end, 0xAA);
        append(p, end, 0xEB);
    }

    appendSelfSync(p, end, 64);
    return static_cast<size_t>(p - out);
}

} // namespace esp_bracket
