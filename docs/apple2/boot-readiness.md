# Boot readiness (host)

Machine-readable companion: `boot-readiness.json`

## Levels

| Level | Meaning | Current |
| --- | --- | --- |
| 0 | Synthetic CPU tests | met |
| 1 | Synthetic ESP][ test ROM | met |
| 2 | Disk II synthetic boot (no Apple ROM/DOS) | met |
| 3 | User-supplied II/II+ ROM + interactive text path | **CURRENT** (architecture HOST_VERIFIED; real ROM optional) |
| 4 | Authentic Disk II + user Disk II ROM path | partial / DEFERRED pieces |
| 5 | Target games | DEFERRED |

Level 3 **does not** require proprietary ROMs in CI. Optional
`apple2:rom-test` reports `SKIPPED_NO_ROM` when none is present.

## Subsystem gate

| Subsystem | Status |
| --- | --- |
| CPU | PASS |
| RAM | PASS |
| ROM abstraction + loader/hash | PASS |
| keyboard / strobe | PASS |
| speaker | PASS |
| cassette | PASS (logical) |
| video switches | PASS |
| text screen extract / flash model | PASS |
| paddles / buttons / annunciators | PASS |
| slot dispatch | PASS |
| Slot-6 ROM modes (none/synthetic/user) | PASS |
| Disk II synthetic boot | PASS |
| Language Card | DEFERRED (not required for II+ ROM Applesoft) |
| floating bus | APPROXIMATE |
| WOZ / full write path / 6502 RWTS | DEFERRED |

Labels are **HOST_VERIFIED** for PASS rows — not PHYSICALLY_VERIFIED.
