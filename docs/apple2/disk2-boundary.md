# Disk II controller boundary

Status: **HOST_VERIFIED** for Slot-6 Disk II + DSK/PO nibble path + synthetic boot.
Authoritative design notes: `docs/apple2/disk-ii.md`.

## Slot integration

- Typical Disk II card in **Slot 6**.
- Soft switches `$C0E0–$C0EF`.
- Card ROM `$C600–$C6FF` (synthetic or user-supplied — never Apple firmware in-repo).

ESP][ keeps Disk II behind `DiskIIController` (`SlotDevice`) — the CPU bus
dispatches I/O; the core must not open files.

## Implemented (host)

- Soft switches, stepper (quarter-tracks), motor, drive select, Q6/Q7, latch
- Cycle-based rotation (~32 cycles/nibble, APPROXIMATE)
- 6-and-2 + address fields + DOS 3.3 track builder
- DSK/DO and floppy-sized PO → nibble tracks (lazy 2-slot cache)
- Optional NIB track image load
- Synthetic Slot-6 ROM + `Esp2DiskTest.dsk` → `ESP][ DISK BOOT OK`

## Deferred

- Full 6502 RWTS inside Slot ROM
- WOZ bitstream backend
- Write path mutating media
- Motor spin-up delay realism

## What NOT to do

- Do not implement a fake “load DOS file” API inside the CPU bus.
- Do not block the 6502 step loop on FAT/SD I/O — use bounded queues.
- Do not assume one title == one file (multi-disk already in media tooling).
- Do not commit Apple Disk II ROM or DOS system tracks.
