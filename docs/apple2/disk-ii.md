# Disk II controller (ESP][)

Status labels: **HOST_VERIFIED** / **ESP32_PHYSICALLY_VERIFIED** / **APPROXIMATE** /
**DEFERRED** / **NOT_IMPLEMENTED**

Profile: Apple II / II+ with Disk II in **Slot 6** (`machine-profile.md`).

## Architecture

```
AppleIIBus
   └─ SlotDevice (slot 6)
         └─ DiskIIController
               ├─ SlotRom (synthetic | clean-room | user-supplied)
               ├─ Drive[1]
               │     └─ NibbleTrackMedia (Dos33NibbleImage / NibTrackImage)
               └─ Drive[2]

Esp32SdStorageBackend  (platform only)
   └─ readAll(/esp2/disks/Esp2BootTest.dsk)
         └─ PSRAM image bytes
               └─ Dos33NibbleImage (lazy 2-slot track cache)
```

Controller does **not** know filesystem paths, microSD, or ESP32 APIs.
Platform I/O stops at `StorageBackend` / firmware mount helper.

## Slot-6 mapping

| Range | Role |
| --- | --- |
| `$C0E0–$C0EF` | Disk II soft switches (slot I/O) |
| `$C600–$C6FF` | Card PROM (256 bytes) |
| `$C800–$CFFF` | Expansion ROM window (clean-room denibble service) |

Soft-switch offsets within the slot window (`$0–$F`):

| Off | Name | Access | Effect |
| --- | --- | --- | --- |
| `$0/$1` | PHASE0 off/on | any | Stepper magnet 0 |
| `$2/$3` | PHASE1 off/on | any | Stepper magnet 1 |
| `$4/$5` | PHASE2 off/on | any | Stepper magnet 2 |
| `$6/$7` | PHASE3 off/on | any | Stepper magnet 3 |
| `$8/$9` | MOTOR off/on | any | Spindle motor |
| `$A/$B` | DRIVE1 / DRIVE2 | any | Select active drive |
| `$C` | Q6L | read often data | Q6=0; in read mode returns/shifts latch (`$C0EC`) |
| `$D` | Q6H | any | Q6=1 |
| `$E` | Q7L | any | Q7=0 (read) |
| `$F` | Q7H | any | Q7=1 (write) |

## Media / cache

- **DSK/DO**: DOS 3.3 sector order — **HOST_VERIFIED** + **ESP32_PHYSICALLY_VERIFIED**
- **PO**: ProDOS sector order remap — **HOST_VERIFIED** (ESP32 exercised from PSRAM copy)
- **NIB**: raw nibble tracks — **HOST_VERIFIED** (ESP32 exercised from converted PSRAM)
- **WOZ**: **DEFERRED**

Lazy per-track 6-and-2 conversion with a **2-slot LRU** nibble cache. Full 143 360-byte
image held in **PSRAM** after one SD read. Emulated rotation timing is cycle-based —
**not** derived from SD latency.

Physical SD path (PART E):

    /esp2/disks/Esp2BootTest.dsk

Firmware may seed this project-owned image once if missing. Mount is **read-only**.

## Clean-room Level-4 / DOS boot0

Origin: **ESP][ project-owned clean-room Slot-6 firmware** (not Apple Disk II ROM).

Vector layout matches documented Disk II PROM offsets (code is project-owned):

1. PROM `$C600` → `JMP $C800` (boot)
2. PROM `$C65C` → `JMP $C900` (sector-read; DOS boot0 `JMP $Cn5C`)
3. EXP `$C800`: motor/drive, find T0S0, copy 343 nibbles → `$0900`, set
   `$2B=$60`, `$27=$09`, **`LDY #$00`**, denibble handshake `$03FA=$DE` →
   **`JMP $0801`** (Disk II convention: `$0800` is the sector-count byte, not
   executable code)
4. EXP `$C900`: sector-read handshake `$03F9=$D1` using ZP `$41` (track),
   `$3D` (sector), `$27` (dest page) → **`LDY #$00`** → **`JMP $0801`**

`LDY #0` matches Disk II PROM sector-store exit (Y wraps after 256 stores).
DOS / title boot loaders often index with Y after `$Cn5C` returns.

`Esp2BootTest` T0S0 also follows the `$0800` parameter / `$0801` entry convention.

Evidence:

| Gate | Status |
| --- | --- |
| Host Level-4 | **HOST_VERIFIED** |
| ESP32 Level-4 (DSK from microSD) | **ESP32_PHYSICALLY_VERIFIED** (PART E) |

## Timing

| Parameter | Value | Label |
| --- | --- | --- |
| Cycles per nibble | 32 | APPROXIMATE |
| ESP32 DSK SD read (143360 B) | see serial `sd_read_us` | ESP32 PHYSICAL MEASURED |
| Track build (cache miss) | ~1 ms class | ESP32 PHYSICAL MEASURED |

## Boot targets

### Level 2 — synthetic (retained)

Synthetic Slot-6 + `Esp2DiskTest` + host sector helper.

### Level 4 — clean-room (current)

Clean-room Slot-6 + `Esp2BootTest` → 6502 path only →
`ESP][ LEVEL 4` / `DISK II BOOT OK` / `$03FE=$4C` `$03FF=$34`.

**DEFERRED:** WOZ, writable disks, Apple Disk II ROM, Level-5 commercial titles.
