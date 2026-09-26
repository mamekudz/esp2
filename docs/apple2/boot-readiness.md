# Boot readiness (host)

Machine-readable companion: `boot-readiness.json`

## Levels

| Level | Meaning | Current |
| --- | --- | --- |
| 0 | Synthetic CPU tests | met |
| 1 | Synthetic ESP][ test ROM | met |
| 2 | Disk II synthetic boot (no Apple ROM/DOS) | met |
| 3 | User-supplied II/II+ ROM + interactive text path | met |
| 4 | Realistic 6502 Slot-6 Disk II boot (clean-room) | **CURRENT** |
| 5 | Target games | DEFERRED |

Level 4 **does not** require proprietary Apple Disk II ROM or DOS in CI.
Optional user Slot-6 PROM via `--slot6-rom` / `--slot6 <path>`; missing assets
report `SKIPPED_NO_SLOT6_ROM` and are not CI failures.

## Level 4 definition

ESP][ executes a realistic Apple II / II+ disk boot path:

```
CPU → Apple II bus → Slot 6 → Disk II ROM interface → DiskIIController
  → nibble/track stream → standard disk image (DSK/DO, PO, NIB)
```

without host-side sector shortcuts (`DiskIIHostBoot::readSector` is Level-2 only).

Required evidence (all **HOST_VERIFIED**):

- Clean-room Slot-6 PROM runs as real 6502 code at `$C600`
- Address/data field search via `$C0EC` latch (bit7 ready protocol)
- 343 data nibbles copied to `$0900`, then denibble service → `$0800`
- Project-owned `Esp2BootTest` markers `$03FE=$4C` / `$03FF=$34`
- Screen text `ESP][ LEVEL 4` / `DISK II BOOT OK`
- Rotation-phase variants succeed
- Motor on / Drive 1 from softswitches (not host-forced)

Not required for Level 4: WOZ, writes, copy-protection, Apple IIe, ProDOS HDD, ESP32.

## Subsystem gate

| Subsystem | Status |
| --- | --- |
| CPU | PASS |
| RAM | PASS |
| ROM abstraction + loader/hash | PASS |
| keyboard / strobe | PASS |
| speaker | PASS (cycle edges + PCM integrate) |
| cassette | PASS (logical) |
| video switches | PASS |
| text screen extract / flash model | PASS |
| paddles / buttons / annunciators | PASS |
| slot dispatch | PASS |
| Slot-6 ROM modes (none/synthetic/cleanroom/user) | PASS |
| Disk II synthetic boot (Level 2) | PASS |
| Disk II clean-room 6502 boot (Level 4) | PASS |
| Disk II PO / NIB Level-4 path | PASS |
| Language Card | DEFERRED (not required for II+ ROM Applesoft) |
| floating bus | APPROXIMATE |
| WOZ / full write path | DEFERRED |
| Apple Disk II ROM bytes in repo | NEVER |

Labels are **HOST_VERIFIED** for PASS rows — not PHYSICALLY_VERIFIED.
