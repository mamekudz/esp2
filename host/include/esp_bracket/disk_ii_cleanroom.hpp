#pragma once

#include <cstddef>
#include <cstdint>

namespace esp_bracket {

class DiskIIController;

/**
 * ESP][ project-owned clean-room Disk II card firmware.
 * NOT Apple's Disk II ROM — independently implemented from documented
 * Disk II soft-switch / nibble-field / DOS boot0 calling conventions.
 *
 * Layout (Apple Disk II–compatible vector offsets, project-owned code):
 *   PROM $C600: JMP $C800 — boot entry
 *   PROM $C65C: JMP $C900 — sector-read entry (DOS boot0 JMP $Cn5C)
 *   EXP  $C800: find T0S0 → denibble → set $2B=$60, $27=$09, Y=0 → JMP $0801
 *   EXP  $C900: sector-read handshake ($03F9=$D1) → Y=0 → JMP $0801
 *
 * Boot entry uses JMP $0801 (Disk II convention: $0800 holds sector-count byte).
 * $2B/$27 init matches documented Disk II PROM → DOS boot0 handshake so boot0
 * can patch JMP $Cn5C (not JMP $005C). Y=0 matches PROM sector-store exit
 * (256-byte index wrap); boot loaders index with Y after $Cn5C returns.
 */
bool generateCleanRoomDiskIICard(uint8_t *prom256, uint8_t *expansion2048);

/** Level-4 boot disk: T0S0 payload displays success + markers $03FE/$03FF. */
bool generateEsp2BootTestImage(uint8_t *dst, size_t dstCap);

/**
 * Clean-room expansion denibble service: if ram[$03FA]==$DE, decode
 * ram[$0900..] → ram[$0800..] and clear $03FA. Returns true if serviced.
 */
bool serviceCleanRoomDenibbleRequest(uint8_t *ram);

/**
 * Clean-room sector-read service for DOS boot0 ($Cn5C path):
 * if ram[$03F9]==$D1, decode track=$41 sector=$3D into page $27.
 */
bool serviceCleanRoomSectorRequest(uint8_t *ram, DiskIIController &disk);

/** Service denibble + sector-read handshakes. */
bool serviceCleanRoomCardRequests(uint8_t *ram, DiskIIController &disk);

} // namespace esp_bracket
