# ESP32 port readiness gate

Status date: PART C physical (dirty-tracked Sharp TEXT/LORES/HGR).

## Gate result

| Gate | Result |
| --- | --- |
| LEVEL_5_TEST_HARNESS | **READY** |
| ESP32_PORT | **READY** |
| PART A text core | **PASS** (`8a1650e`) |
| PART B display decoupling | **PASS** (`08f2ef7`) |
| PART C dirty video Sharp | **PASS** (this milestone) |

## Timing ownership

Emulated **6502 cycles** own Apple II time. Display dirty coalescing must not
skip VRAM stores or alter instruction timing.

**ESP32 PHYSICAL MEASURED:** throttled live ~1.023e6 cps with TEXT dirty
pulses; raw ~1.32e6 with dirty tracking on or off.

## Video / dirty

- Logical HGR **280×192** (unchanged).
- Physical Sharp viewport **280×192** (PART B 240 was TEXT 40×6 glyph layout).
- `VideoDirtyTracker` hooked from `Apple2Bus::write` + video soft-switches.
- Modes exercised on device: TEXT, LORES, LORES MIXED, HGR Sharp, HGR MIXED,
  PAGE1/PAGE2.

## FreeRTOS layout

| Task | Core | Role |
| --- | --- | --- |
| `a2emu` | 1 | `runCycles` + 1× throttle |
| `a2disp` | 0 | dirty → Sharp render → CO5300; power/touch |

## Still out of scope

Artifact color, CRT/Monitor effects, Disk II on device, real Apple ROM, BLE,
audio out, BlueShift, commercial media, WOZ.
