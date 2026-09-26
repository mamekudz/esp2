# Boot readiness (host)

Machine-readable companion: `boot-readiness.json`

## Levels

| Level | Meaning | Current |
| --- | --- | --- |
| 0 | Synthetic CPU tests | met |
| 1 | Synthetic ESP][ test ROM | met |
| 2 | Disk II synthetic boot (no Apple ROM/DOS) | **CURRENT** |
| 3 | Real Apple II/II+ ROM load/execute | not claimed |
| 4 | BASIC/text interaction | not claimed |
| 5 | Target games | DEFERRED |

## Subsystem gate

| Subsystem | Status |
| --- | --- |
| CPU | PASS |
| RAM | PASS |
| ROM abstraction | PASS |
| keyboard | PASS |
| keyboard strobe | PASS |
| speaker | PASS |
| cassette | PASS (logical only; no physical deck) |
| video switches | PASS |
| paddles | PASS (cycle model) |
| buttons | PASS |
| annunciators | PASS |
| slot dispatch | PASS |
| Slot-6 ROM decode | PASS |
| language card | DEFERRED |
| Disk II controller | PASS (see `disk-ii.md`) |
| Disk II 6-and-2 / DSK boot | PASS |
| Disk II 6502 RWTS in Slot ROM | DEFERRED |
| Disk II WOZ | DEFERRED |
| floating bus | APPROXIMATE |

Labels are **HOST_VERIFIED** for PASS rows — not PHYSICALLY_VERIFIED.
