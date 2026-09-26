# Boot readiness (host)

Machine-readable companion: `boot-readiness.json`

## Levels

| Level | Meaning | Current |
| --- | --- | --- |
| 0 | Synthetic CPU tests | met |
| 1 | Synthetic ESP][ test ROM | met |
| 2 | Disk II synthetic boot (no Apple ROM/DOS) | met |
| 3 | User-supplied II/II+ ROM + interactive text path | met |
| 4 | Realistic 6502 Slot-6 Disk II boot (clean-room) | met |
| 5 | Real user-supplied software, interactive, no title hacks | **READY_FOR_REAL_SOFTWARE_TEST** |

Level 5 is **not** PASS until a legal local ROM + media configuration boots
interactively through the normal machine architecture. Preparation provides the
harness (`docs/apple2/compatibility-testing.md`). Missing assets →
`SKIPPED_MISSING_USER_ASSETS` (successful preparation outcome).

## Subsystem gate

| Subsystem | Status |
| --- | --- |
| CPU | PASS |
| RAM | PASS |
| ROM abstraction + loader/hash | PASS |
| keyboard / strobe | PASS |
| speaker | PASS |
| video switches / text / LoRes / HGR / artifact | PASS |
| Slot-6 modes + Level-4 clean-room boot | PASS |
| Compatibility harness | PASS (no proprietary media required) |
| Language Card | DEFERRED |
| WOZ / writes | DEFERRED |
| ESP32 emulator integration | NEXT (port readiness gate) |

Labels are **HOST_VERIFIED** for PASS rows — not PHYSICALLY_VERIFIED.
