# ESP32 port readiness gate

Status date: PART E physical (Disk II + microSD Level-4 boot).

## Gate result

| Gate | Result |
| --- | --- |
| LEVEL_5_TEST_HARNESS | **READY** (not PASS) |
| ESP32_PORT | **READY** |
| PART A text core | **PASS** (`8a1650e`) |
| PART B display decoupling | **PASS** (`08f2ef7`) |
| PART C dirty video Sharp | **PASS** (`ccbe73c`) |
| PART D HGR Artifact Color | **PASS** (`4a67d8f`) |
| PART E Disk II Level-4 | **PASS** (this milestone) |

## Level-4

| Side | Status |
| --- | --- |
| Host clean-room boot | **HOST_VERIFIED** |
| ESP32 microSD → DiskIIController → CO5300 | **ESP32_PHYSICALLY_VERIFIED** |

Do **not** promote Level 5.

## Timing ownership

Emulated **6502 cycles** own Apple II + Disk II rotation. SD latency is
platform-only (measured `sd_read_us` / `track_build_us`); it must not redefine
nibble timing.

## FreeRTOS layout

| Task | Core | Role |
| --- | --- | --- |
| `a2emu` | 1 | `runCycles` + clean-room denibble + 1× throttle |
| `a2disp` | 0 | dirty video + power/touch |

## Still out of scope

Apple system/Disk II ROM, Level-5 games, WOZ, disk writes, BLE, audio out,
BlueShift, CRT/Monitor effects.
