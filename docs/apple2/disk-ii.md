# Disk II controller (ESP][ host)

Status labels: **HOST_VERIFIED** / **APPROXIMATE** / **DEFERRED** / **NOT_IMPLEMENTED**

Profile: Apple II / II+ with Disk II in **Slot 6** (`machine-profile.md`).

## Architecture

```
AppleIIBus
   └─ SlotDevice (slot 6)
         └─ DiskIIController
               ├─ SlotRom (synthetic | user-supplied)
               ├─ Drive[1]
               │     └─ abstract DiskImage / nibble tracks
               └─ Drive[2]
```

Controller does **not** know filesystem paths, microSD, or ESP32 APIs.

## Slot-6 mapping

| Range | Role |
| --- | --- |
| `$C0E0–$C0EF` | Disk II soft switches (slot I/O) |
| `$C600–$C6FF` | Card PROM (256 bytes) |
| `$C800–$CFFF` | Expansion ROM window (selection latch; Disk II rarely needs it) |

Soft-switch offsets within the slot window (`$0–$F`):

| Off | Name | Access | Effect |
| --- | --- | --- | --- |
| `$0/$1` | PHASE0 off/on | any | Stepper magnet 0 |
| `$2/$3` | PHASE1 off/on | any | Stepper magnet 1 |
| `$4/$5` | PHASE2 off/on | any | Stepper magnet 2 |
| `$6/$7` | PHASE3 off/on | any | Stepper magnet 3 |
| `$8/$9` | MOTOR off/on | any | Spindle motor |
| `$A/$B` | DRIVE1 / DRIVE2 | any | Select active drive |
| `$C` | Q6L | read often data | Q6=0; in read mode returns/shifts latch |
| `$D` | Q6H | any | Q6=1 |
| `$E` | Q7L | any | Q7=0 (read) |
| `$F` | Q7H | any | Q7=1 (write) |

**Any access** (read or write) updates switch state for phase/motor/drive/Q6/Q7.
Data returns on appropriate reads of `$C` when motor on + read mode.

## Drive / stepper state

| State | Model |
| --- | --- |
| Motor | ON / OFF — **APPROXIMATE**: instant on/off (no spin-up delay yet) |
| Drive select | Drive 1 or 2; independent media & head position |
| Stepper | 4 phase magnets; position in **quarter-tracks** (0…139 for 35 tracks) |
| Write protect | From media flags; writes ignored when protected |
| Rotation | Cycle-based nibble index — **APPROXIMATE** ~32 CPU cycles / nibble |

Quarter-tracks chosen for future WOZ compatibility (do not redesign for WOZ).

Stepper: phase transitions move ±1 quarter-track when adjacent phases engage
(simplified but sequence-aware — not `track++` on every write).

## Q6 / Q7 / latch

Read path (V1):

1. MOTOR ON, DRIVE selected, disk inserted
2. Q7L (read), Q6L reads shift data register
3. Latch advances with rotation (~32 cycles / nibble)
4. **Bit7 ready protocol (HOST_VERIFIED):** first read in a nibble window
   returns the disk byte with bit7 set; further reads in the same window
   return bit7 clear so `LDA $C0EC / BPL *-3` waits for the next nibble

Write path: architecture present; V1 defaults to **read-only** (WP sense correct;
writes do not mutate media unless explicitly enabled later).

CPU bus accesses sync `accessCycle` from fake6502 `liveCycles` so rotation
advances during `runCycles` batches.

## Media abstraction

```
logical DSK/DO/PO sectors
        │
   TrackBuilder (6-and-2 + address/data fields)
        │
   nibble track buffer (cached per track)
        │
   DiskIIController rotation / latch
```

- **DSK/DO**: DOS 3.3 sector order in file → physical interleave in track builder — **HOST_VERIFIED**
- **PO**: ProDOS sector order remapped before build — **HOST_VERIFIED** (floppy-sized)
- **NIB**: raw nibble tracks — architecture reserved / optional
- **WOZ**: must remain bitstream/timing capable — **DEFERRED** (not forced through sectors)

### Track cache

Lazy per-track conversion with a small fixed cache (2 tracks × 2 drives) —
ESP32-friendly; no full-disk nibble preconversion.

## Timing

| Parameter | Value | Label |
| --- | --- | --- |
| Cycles per nibble | 32 | APPROXIMATE |
| Bits per nibble | 8 (stored as disk byte) | standard nibble stream |
| Host wall clock | unused in tests | — |

fake6502 is instruction-level; Disk II uses `accessCycle` from the bus.

## ROM policy

| Kind | In repo? |
| --- | --- |
| Apple Disk II PROM | **Never** |
| User-supplied PROM (256 B) | `--slot6-rom` / `--slot6 <path>` — hash via `slot6_rom_database.json` |
| ESP][ synthetic Slot-6 ROM | Yes — Level-2 softswitch / marker helper |
| ESP][ clean-room Slot-6 card | Yes — Level-4 realistic boot PROM + `$C800` denibble service |

Synthetic and clean-room firmware are **not** Apple's Disk II ROM and must not
reproduce Apple ROM byte sequences.

### Clean-room Level-4 boot (HOST_VERIFIED)

Origin: **ESP][ project-owned clean-room Slot-6 boot ROM**

1. PROM `$C600`: motor on, Drive 1, Q7L, recalibrate pulse, search address
   field `D5 AA 96` for track 0 / sector 0, find data `D5 AA AD`, copy 343
   nibbles to `$0900`, `JMP $C800`.
2. Expansion `$C800`: request denibble via `STA $03FA,#$DE`; host/firmware
   runs `DiskIIEncoding::decodeSector` into `$0800`, clears `$03FA`; `JMP $0800`.
3. Boot sector (project `Esp2BootTest`) writes text + markers `$03FE=$4C`,
   `$03FF=$34`.

The denibble handshake is an explicit ESP][ expansion service (APPROXIMATE vs
Apple 6502 RWTS decode) — Disk II **reads** remain authentic nibble I/O.

Metadata DB: `host/data/slot6_rom_database.json` (hashes only; entries empty until
reliable provenance is recorded).

## Activity events (language-neutral)

`DiskIIActivity`: MotorOn/Off, DriveSelect, Step, Read, WriteAttempt, Insert, Eject.

`DiskIIError` (language-neutral): Ok, NoDisk, WriteProtected, TrackNotFound,
AddressFieldNotFound, DataFieldNotFound, ChecksumError, InvalidNibble,
RotationTimeout, InvalidImage.

## Cross-checks

Compared conceptually with apple2js / EWM Disk II behavior and published
DOS 3.3 track layout notes. Disagreements on timing remain labeled APPROXIMATE.

Optional external tools (not required for CI): a2kit, DiskM8 (GPL-3.0 external only).

## Boot targets

### Level 2 — synthetic (retained)

Synthetic Slot-6 ROM + `Esp2DiskTest` + host `DiskIIHostBoot::readSector` →
`ESP][ DISK BOOT OK` / `$03FF=$A5`.

### Level 4 — clean-room (current)

Clean-room Slot-6 + `Esp2BootTest` (.dsk / .po / nib fixture) → 6502 path only →
`ESP][ LEVEL 4` / `DISK II BOOT OK` / `$03FE=$4C` `$03FF=$34`.

**DEFERRED:** WOZ, writable disks, copy-protection titles, bundling Apple Disk II ROM.

### Approximations

| Topic | Status |
| --- | --- |
| Motor spin-up/down delay | APPROXIMATE (instant) |
| Cycles / nibble | APPROXIMATE (32) |
| Self-sync gaps | APPROXIMATE (`0xFF` runs) |
| Clean-room denibble | APPROXIMATE (host `DiskIIEncoding` service) |
| Write path | Read-only V1; WP sense correct |
| WOZ | Architecture ready; backend DEFERRED |
