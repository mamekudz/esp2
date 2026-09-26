# Boot readiness (host + ESP32)

Machine-readable companion: `boot-readiness.json`

## Levels

| Level | Meaning | Current |
| --- | --- | --- |
| 0 | Synthetic CPU tests | met |
| 1 | Synthetic ESP][ test ROM | met |
| 2 | Disk II synthetic boot (no Apple ROM/DOS) | met |
| 3 | User-supplied II/II+ ROM + interactive text path | met (host) |
| 4 | Realistic 6502 Slot-6 Disk II boot (clean-room) | **HOST_VERIFIED** + **ESP32_PHYSICALLY_VERIFIED** |
| 5 | Real user-supplied software, interactive, no title hacks | **READY_FOR_REAL_SOFTWARE_TEST** (not PASS) |

Level 5 is **not** PASS until a legal local ROM + media configuration boots
interactively. Do **not** promote Level 5 from PART E.

## Subsystem gate

| Subsystem | Status |
| --- | --- |
| CPU | PASS |
| RAM | PASS |
| ROM abstraction + loader/hash | PASS |
| keyboard / strobe | PASS |
| speaker | PASS |
| video switches / text / LoRes / HGR / artifact | PASS (host + ESP32 video) |
| Slot-6 modes + Level-4 clean-room boot | PASS host; **ESP32 DSK physical PASS** |
| Compatibility harness | PASS (no proprietary media required) |
| Language Card | DEFERRED |
| WOZ / writes | DEFERRED |
| ESP32 Disk II + microSD | **PASS** (PART E) |

## ESP32 Level-4 evidence (project-owned)

Path: `/esp2/disks/Esp2BootTest.dsk` → `Esp32SdStorageBackend` → PSRAM
`Dos33NibbleImage` → `DiskIIController` → `$C600` clean-room PROM → `$C0EC`
nibble path → denibble `$C800` → markers `$03FE=$4C` `$03FF=$34` + text via
Apple II video RAM → CO5300.

Rotational start variants (0, 37, 128, 777) required — boot must not depend on
sector alignment.
