#pragma once

#include <cstddef>
#include <cstdint>

namespace esp_bracket {

/**
 * Build a standard DOS 3.3 16-sector nibble track.
 * Logical sectors[16][256] are DOS order (as in .dsk).
 */
class DiskIITrackBuilder {
  public:
    static constexpr int kSectors = 16;
    static constexpr int kSectorBytes = 256;
    /** Typical generated track length (gaps + 16 sectors). */
    static constexpr size_t kMaxTrackNibbles = 6656;

    /**
     * Build track into out[]. Returns nibble count, or 0 on failure.
     * volume is the DOS volume number in address fields (often 254).
     */
    static size_t buildTrack(uint8_t trackNumber, uint8_t volume,
                             const uint8_t sectors[kSectors][kSectorBytes], uint8_t *out,
                             size_t outCap);

    /** DOS logical sector → physical order index on track. */
    static int logicalToPhysical(int logicalSector);

    /** .po block order → DOS logical sector within track (floppy-sized). */
    static int poSectorToLogical(int poSector);
};

} // namespace esp_bracket
