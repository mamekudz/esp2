# ESP32 port readiness gate

Status date: PART B physical (after `8a1650e` text core + display throughput).

## Gate result

| Gate | Result |
| --- | --- |
| LEVEL_5_TEST_HARNESS | **READY** |
| ESP32_PORT | **READY** (text-only physical + display scheduling) |
| PART A text core on S3 | **PASS** (`8a1650e`) |
| PART B display throughput / scheduling | **PASS** (this milestone) |

## Module classification

| Module | Class | Notes |
| --- | --- | --- |
| Cpu6502 / fake6502 | READY | Physical text-port |
| Apple2Bus / RAM / ROM | READY | 48 KiB RAM in **internal SRAM BSS** |
| Soft switches / keyboard | READY | |
| Paddles / GameIo | READY | |
| GamepadMapper | READY | Normalized pad → joystick/paddles |
| Speaker / PCM / EdgeDelta | READY | Cycle timeline authoritative |
| Text / LoRes / HGR decode | READY | Text Sharp path on device |
| Artifact color | NEEDS_OPTIMIZATION | LUT/scanline OK; measure on S3 before enabling |
| Display effects CRT | NEEDS_OPTIMIZATION | First port: **Sharp** only |
| DiskIIController | READY | No SD coupling |
| Media / track cache | NEEDS_ADAPTER | Future `Esp32SdStorageBackend` wraps verified SD |
| HostAppleIIMachine / PPM / scripts | HOST_ONLY | |
| BLE input | NEEDS_ADAPTER | Feed GamepadState/KeyboardEvent only |
| BlueShift | HOST_ONLY (extension) | Edge encoder stays in ESP][; no BlueShift firmware here |
| Physical CO5300/FT3168/SD/IMU | **VERIFIED** (do not rewrite) | Regression PASS under text-port |
| Display task / QSPI path | READY | Viewport 240×192; decoupled from emu |

## Timing ownership

Emulated **6502 cycles** own Apple II time for speaker, paddles, Disk II, and
video timing where relevant. FreeRTOS ticks are wall-clock only.

Target: ~1× Apple II/II+ (~1.023 MHz class). Throttle by syncing emulated
cycles to wall clock in batches — not per instruction.

**ESP32 PHYSICAL MEASURED:** with display on core 0 and emu on core 1,
throttled live rate stays ~1.023e6 cps while panel updates; blocking the emu
task on SPI drops cps catastrophically (~3.3e4) — do not couple them.

If late: keep machine correctness > input > audio > display > cosmetics.
Never skip Disk II / CPU cycles to chase FPS.

## Memory sketch (physical text-port)

| Item | Prefer | Approx | Label |
| --- | --- | --- | --- |
| CPU + soft state | internal SRAM | <4 KiB | |
| Apple II 48K RAM | **internal BSS** | 48 KiB | ESP32 PHYSICAL MEASURED |
| Viewport RGB565 240×192 | **PSRAM** staging | 92 KiB | ESP32 PHYSICAL MEASURED |
| Strip 240×8 DMA | internal DMA | ~4 KiB | |
| Track cache (later) | PSRAM | ~13 KiB | |
| ROM 12K synthetic | static / BSS | 12 KiB | |

## FreeRTOS layout (PART B)

| Task | Core | Prio | Role |
| --- | --- | --- | --- |
| `a2emu` | 1 | 1 | `runCycles` quantum 2000 + 1× throttle |
| `a2disp` | 0 | 2 | text snapshot → CO5300; power/touch housekeeping |
| Arduino `loop` | 1 | 1 | idle delay only |

Publish path: double-buffered text chars + generation counter; newest wins;
identical frames not retransferred. Display paint ≠ user activity.

## Display transfer strategy

- SPI: Arduino_ESP32QSPI @ **40 MHz**, quad, SPI2, DMA (≤1024 px/chunk), polling.
- Do **not** full-panel `fillScreen` each Apple II frame.
- Transfer **Apple II viewport only** (240×192 at offset), one bitmap window.
- See `performance-budget.md` for before/after ESP32 PHYSICAL MEASURED tables.

## Compile smoke

```bash
pio run -e core_smoke
pio run -e apple2_text
```

| Target | Flash (app) | Static RAM |
| --- | --- | --- |
| `core_smoke` | ~317 KiB | ~116 KiB / 320 KiB |
| `apple2_text` (PART B) | ~486 KiB | ~133 KiB / 320 KiB |

## Still out of scope

Disk II on device, real Apple ROM, BLE, audio out, HGR artifact, CRT, library UI.
