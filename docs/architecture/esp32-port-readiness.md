# ESP32 port readiness gate

Status date: Level-5 preparation (host). **No physical emulator integration yet.**

## Gate result (this milestone)

| Gate | Result |
| --- | --- |
| LEVEL_5_TEST_HARNESS | **READY** |
| ESP32_PORT | **READY** (first incremental text-only milestone only) |

## Module classification

| Module | Class | Notes |
| --- | --- | --- |
| Cpu6502 / fake6502 | READY | Compile-smoke target |
| Apple2Bus / RAM / ROM | READY | 48 KiB RAM object — prefer static/PSRAM placement later |
| Soft switches / keyboard | READY | |
| Paddles / GameIo | READY | |
| GamepadMapper | READY | Normalized pad → joystick/paddles |
| Speaker / PCM / EdgeDelta | READY | Cycle timeline authoritative |
| Text / LoRes / HGR decode | READY | |
| Artifact color | NEEDS_OPTIMIZATION | LUT/scanline OK; measure on S3 before enabling |
| Display effects CRT | NEEDS_OPTIMIZATION | First port: **Sharp** only |
| DiskIIController | READY | No SD coupling |
| Media / track cache | NEEDS_ADAPTER | Future `Esp32SdStorageBackend` wraps verified SD |
| HostAppleIIMachine / PPM / scripts | HOST_ONLY | |
| BLE input | NEEDS_ADAPTER | Feed GamepadState/KeyboardEvent only |
| BlueShift | HOST_ONLY (extension) | Edge encoder stays in ESP][; no BlueShift firmware here |
| Physical CO5300/FT3168/SD/IMU | BLOCKED (do not rewrite) | Verified bring-up baseline |

## Timing ownership

Emulated **6502 cycles** own Apple II time for speaker, paddles, Disk II, and
video timing where relevant. FreeRTOS ticks are wall-clock only.

Target: ~1× Apple II/II+ (~1.023 MHz class). Throttle by syncing emulated
cycles to wall clock in batches — not per instruction.

If late: keep machine correctness > input > audio > display > cosmetics.
Never skip Disk II / CPU cycles to chase FPS.

## Memory sketch (estimates)

| Item | Prefer | Approx |
| --- | --- | --- |
| CPU + soft state | internal SRAM | <4 KiB |
| Apple II 48K RAM | internal or PSRAM | 48 KiB |
| Track cache (2×~6.5K) | PSRAM | ~13 KiB |
| RGB565 280×192 | PSRAM | ~105 KiB |
| RGB565 280×456 panel | PSRAM | ~250 KiB |
| Speaker ring + PCM | internal | few KiB |
| ROM 12K | flash / XIP | 12 KiB |

Provisional framebuffer: **scanline or RGB565 280×192** into CO5300; full 280×456
panel buffer optional for UI. Indexed/mono HGR buffer for Sharp text/HGR first.

## Disk cache

Provisional: **lazy per-track** nibble build with small bounded cache (current host
model). Whole-disk convert at load is optional later if SD latency dominates.

## FreeRTOS sketch

Minimal: Emulator task (cycle owner) + Display/UI task + input events + optional
audio sink. Storage ops off emulator hot path.

## First ESP32 emulator milestone (NEXT — not this milestone)

Only if gate stays READY:

- Cpu6502 + Apple2Bus + RAM + synthetic ROM + text + existing CO5300 adapter

Explicitly **not** yet: Disk II, real Apple ROM, BLE, audio out, HGR artifact, CRT, library.

## Compile smoke

```bash
pio run -e core_smoke
```

COMPILE/LINK only. Do not flash COM5 for this gate.

Measured on this host (Arduino ESP32-S3 / pioarduino 55.03.32, smoke + portable
core linked; includes Arduino framework baseline):

| | Used | Notes |
| --- | --- | --- |
| Flash (.bin image class) | ~317 KiB | framework + core smoke |
| Static RAM (PlatformIO report) | ~116 KiB / 320 KiB | includes static `Apple2Bus` 48K RAM |

fake6502 / Cpu6502 compile cleanly under the ESP32-S3 toolchain (warnings only
in vendored unused helpers).