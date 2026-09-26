# Boot readiness (host)

Machine-readable companion: `boot-readiness.json`

## Levels

| Level | Meaning | Current |
| --- | --- | --- |
| 0 | Synthetic CPU tests | met |
| 1 | Synthetic ESP][ test ROM | **CURRENT** |
| 2 | Real Apple II/II+ ROM load/execute | not claimed |
| 3 | BASIC/text interaction | not claimed |
| 4 | Disk II boot | DEFERRED |
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
| language card | DEFERRED |
| Disk II | DEFERRED |
| floating bus | APPROXIMATE |

Labels are **HOST_VERIFIED** for PASS rows — not PHYSICALLY_VERIFIED.
