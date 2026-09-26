# ESP32 port readiness gate

Status date: PART F1 (user system ROM path; physical ROM asset pending).

## Gate result

| Gate | Result |
| --- | --- |
| LEVEL_5_TEST_HARNESS | **READY** (not PASS) |
| ESP32_PORT | **READY** |
| PART A text core | **PASS** (`8a1650e`) |
| PART B display decoupling | **PASS** (`08f2ef7`) |
| PART C dirty video Sharp | **PASS** (`ccbe73c`) |
| PART D HGR Artifact Color | **PASS** (`4a67d8f`) |
| PART E Disk II Level-4 | **PASS** (`85e4207`) |
| PART F1 user system ROM | **PATH_READY** / `REAL_SYSTEM_ROM=NOT_VERIFIED` (`SKIPPED_NO_ROM`) |

## Level-4

| Side | Status |
| --- | --- |
| Host clean-room boot | **HOST_VERIFIED** |
| ESP32 microSD → DiskIIController → CO5300 | **ESP32_PHYSICALLY_VERIFIED** |

## Level-5 / REAL_SYSTEM_ROM

| Side | Status |
| --- | --- |
| Host Level-3 ROM path | HOST_VERIFIED (with optional local ROM) |
| ESP32 F1 firmware (SD load, RESET, keyboard) | Implemented |
| ESP32 F1 with user ROM on microSD | **NOT_VERIFIED** — place `/esp2/roms/system.rom` |

Do **not** promote full Level 5 PASS from F1 alone. F1 success (when ROM present)
yields `REAL_SYSTEM_ROM=ESP32_PHYSICALLY_VERIFIED` and
`LEVEL_5=PARTIAL/READY_FOR_REAL_SOFTWARE_TEST` only.

## Timing ownership

Emulated **6502 cycles** own Apple II + Disk II rotation. SD latency is
platform-only (measured `sd_read_us` / `track_build_us`); it must not redefine
nibble timing.

## FreeRTOS layout

| Task | Core | Role |
| --- | --- | --- |
| `a2boot` | 1 | One-shot F1 bring-up (large stack; avoids loopTask canary) |
| `a2emu` | 1 | `runCycles` + optional clean-room denibble + 1× throttle |
| `a2disp` | 0 | dirty video + power/touch |

## Still out of scope

Real commercial software (F2), Disk II ROM testing, WOZ, disk writes, BLE,
physical audio out, BlueShift, CRT/Monitor effects.
