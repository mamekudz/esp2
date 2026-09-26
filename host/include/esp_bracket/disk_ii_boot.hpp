#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_bracket/disk_ii_controller.hpp"
#include "esp_bracket/disk_ii_encoding.hpp"

namespace esp_bracket {

class Apple2Bus;

/**
 * Host-side sector read through Disk II softswitches + nibble stream.
 * Used until a full 6502 RWTS lives in Slot ROM (DEFERRED).
 * Still exercises motor / Q7 / Q6 / latch / rotation — no sector-register shortcut.
 */
class DiskIIHostBoot {
  public:
    /**
     * Read one DOS-order logical sector via controller data register.
     * Requires media attached; leaves motor on.
     */
    static bool readSector(Apple2Bus &bus, DiskIIController &disk, uint8_t track, uint8_t sector,
                           uint8_t out[256]);

    /** Find address+data field in a captured nibble stream; decode to out. */
    static bool decodeSectorFromStream(const uint8_t *stream, size_t len, uint8_t wantTrack,
                                       uint8_t wantSector, uint8_t out[256]);
};

} // namespace esp_bracket
