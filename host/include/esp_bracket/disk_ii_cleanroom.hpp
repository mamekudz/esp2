#pragma once

#include <cstddef>
#include <cstdint>

namespace esp_bracket {

/**
 * ESP][ project-owned clean-room Disk II card firmware.
 * NOT Apple's Disk II ROM — independently implemented from documented
 * Disk II soft-switch / nibble-field behavior.
 *
 * Layout:
 *   PROM $C600–$C6FF (256): motor/drive, find T0S0, copy 343 nibbles → $0900, JMP $C800
 *   EXP  $C800: denibble service handshake ($03FA=$DE → host DiskIIEncoding → $0800)
 */
bool generateCleanRoomDiskIICard(uint8_t *prom256, uint8_t *expansion2048);

/** Level-4 boot disk: T0S0 payload displays success + markers $03FE/$03FF. */
bool generateEsp2BootTestImage(uint8_t *dst, size_t dstCap);

/**
 * Clean-room expansion denibble service: if ram[$03FA]==$DE, decode
 * ram[$0900..] → ram[$0800..] and clear $03FA. Returns true if serviced.
 */
bool serviceCleanRoomDenibbleRequest(uint8_t *ram);

} // namespace esp_bracket
