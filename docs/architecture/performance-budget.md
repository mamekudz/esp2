# Performance budget

Labels: **HOST MEASURED**, **ESP32 ESTIMATE**, **ESP32 MEASURED**, **UNKNOWN**.

Target SoC: ESP32-S3, 16 MB flash, 8 MB PSRAM, 240 MHz.

## Memory

| Bucket | Budget (bytes) | Label | Notes |
| --- | --- | --- | --- |
| Internal SRAM free after bring-up | ~240–280 KiB typical free heap | ESP32 MEASURED* | BLE + display reduce free heap; see serial `[RAM]` |
| PSRAM | 8 MiB | ESP32 MEASURED | Present on target board |
| 6502 core state | <1 KiB | ESP32 ESTIMATE | fake6502 globals + wrapper |
| Apple II main RAM (48K II+) | 48 KiB | ESP32 ESTIMATE | PSRAM-friendly |
| Aux / language card (later) | +64 KiB | ESP32 ESTIMATE | Not in host baseline |
| ROM (user-provided) | 12 KiB typical ][+ | ESP32 ESTIMATE | Flash or PSRAM; never commit proprietary ROM |
| Video state / soft switches | <1 KiB | ESP32 ESTIMATE | |
| HGR bitplane decode | 280×192 ≈ 52.5 KiB bits + 7.5 KiB highbits | ESP32 ESTIMATE | Prefer line buffers on device |
| Output RGB line/frame | 280×192×2 ≈ 105 KiB RGB565 frame optional | ESP32 ESTIMATE | Prefer scanline push to CO5300 |
| Disk buffers | 4–64 KiB | ESP32 ESTIMATE | Stream WOZ later |
| Speaker event ring | ~16–32 KiB host; 2–8 KiB device | HOST / ESTIMATE | Bounded ring in `Speaker` |
| BLE / NimBLE | tens of KiB | UNKNOWN | Measure with keyboard+gamepad |
| UI / Control Screen | 32–128 KiB | ESP32 ESTIMATE | Avoid second full framebuffer |
| Stacks / FreeRTOS | 8–24 KiB total | ESP32 ESTIMATE | Few tasks |

\*Bring-up logs previously showed ~242 KiB free heap after BLE init — re-measure after each subsystem.

## Host measurements

Run `node host/tools/build_and_test_apple2.mjs` and read lines prefixed
`HOST MEASUREMENT`. These are **not** ESP32 performance.

| Metric | Label |
| --- | --- |
| `cpu_1e6_cycles_ms` | HOST MEASURED |
| `hgr_decode_50_ms` | HOST MEASURED |
| `artifact_50_ms` | HOST MEASURED |

## CPU / timing (ESP32 targets)

| Concern | Target | Label |
| --- | --- | --- |
| Apple II effective speed | ≥ 1.0× real-time preferred | UNKNOWN |
| Display refresh | ≥ 30 FPS readable; 60 ideal | UNKNOWN |
| Audio latency local | < 40 ms | UNKNOWN |
| SD disk latency | must not stall CPU mid-instruction batch | ESP32 ESTIMATE |

## Design implications

1. Prefer **scanline / tile** video over dual full RGB framebuffers.
2. Keep Disk II I/O on a worker with bounded queues; never block the 6502
   step loop on FAT.
3. Measure before optimizing (`CLAUDE.md` §32).
4. No repeated heap allocation in CPU/bus/video hot paths.
