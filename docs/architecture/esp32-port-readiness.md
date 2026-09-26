# ESP32 port readiness gate

Status date: PART D physical (HGR Artifact Color + dirty color video).

## Gate result

| Gate | Result |
| --- | --- |
| LEVEL_5_TEST_HARNESS | **READY** |
| ESP32_PORT | **READY** |
| PART A text core | **PASS** (`8a1650e`) |
| PART B display decoupling | **PASS** (`08f2ef7`) |
| PART C dirty video Sharp | **PASS** (`ccbe73c`) |
| PART D HGR Artifact Color | **PASS** (this milestone) |

## Timing ownership

Emulated **6502 cycles** own Apple II time. Display dirty coalescing must not
skip VRAM stores or alter instruction timing.

**ESP32 PHYSICAL MEASURED:** throttled live ~1.023e6 cps with TEXT dirty
pulses; raw ~1.32e6 with dirty tracking on or off. Artifact stress must not
pull Apple II below 1× — drop/coalesce physical frames instead.

## Video / dirty / color

- Logical HGR **280×192** (unchanged).
- Physical viewport **280×192** RGB565 in PSRAM.
- `VideoDirtyTracker` (24 B) — regenerate full dirty scanlines for artifact
  (no per-pixel dirty).
- Presentation `Sharp` vs `ArtifactColor` separate from Apple II soft-switches.
- Shared host/ESP32 `ArtifactRenderer` (digital pair/phase — **not** full NTSC).
- Modes: TEXT, LORES, LORES MIXED, HGR Sharp, HGR Artifact, HGR MIXED Artifact,
  PAGE1/PAGE2.

## FreeRTOS layout

| Task | Core | Role |
| --- | --- | --- |
| `a2emu` | 1 | `runCycles` + 1× throttle |
| `a2disp` | 0 | dirty → Sharp/Artifact render → CO5300; power/touch |

## Still out of scope

CRT/Monitor effects, Disk II on device, real Apple ROM, BLE, audio out,
BlueShift, commercial media, WOZ.
