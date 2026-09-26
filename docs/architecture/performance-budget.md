# Performance budget

Labels: **HOST MEASURED**, **ESP32 ESTIMATE**, **ESP32 PHYSICAL MEASURED**, **UNKNOWN**.

Target SoC: ESP32-S3, 16 MB flash, 8 MB PSRAM, 240 MHz.

## Memory

| Bucket | Budget (bytes) | Label | Notes |
| --- | --- | --- | --- |
| Internal SRAM (text/video port) | ~130 KiB static / ~210 KiB free heap | ESP32 PHYSICAL MEASURED | Apple II 48K in BSS |
| PSRAM | 8 MiB | ESP32 PHYSICAL MEASURED | Viewport RGB565 + HGR decode staging |
| Dirty metadata | **24 bytes** (192-bit scanline set) | ESP32 PHYSICAL MEASURED | `VideoDirtyTracker` |
| Viewport RGB565 280×192 | 107 520 B | ESP32 PHYSICAL MEASURED | PSRAM |

## PART B → D display (ESP32 PHYSICAL MEASURED)

| Path | Time / notes |
| --- | --- |
| PART B text 240×192 full | ~58.9 ms |
| PART C/D Sharp TEXT full | render ~7.4 ms + xfer ~62.5 ms ≈ **70 ms** |
| PART D HGR Sharp full | render **7.5 ms** + xfer **62.5 ms** |
| PART D HGR Artifact full | render **27.0 ms** + xfer **62.4 ms** |
| PART D HGR Sharp 1 line | render **3.0 ms** + xfer **0.47 ms** (560 B) |
| PART D HGR Artifact 1 line | render **3.1 ms** + xfer **0.40 ms** (560 B) |
| PART D HGR Artifact 8 lines | render **4.1 ms** + xfer **2.7 ms** |
| Artifact stress update rate | max ≈ **12 Hz** (coalesced dirty) |
| Emulator under artifact stress | throttled ≈ **1.023e6** cps (1× preserved) |
| Dirty overhead | cps_off≈cps_on ≈ **1.32e6** |
| HGR clear exact (6502) | **~90 515** host / **92 024** ESP32 cycles; PART C ~8e6 was timeout idle |

QSPI remains **40 MHz** quad, DMA chunk ≤1024 px.

## Scheduling

- `a2emu` core1 — cycle owner, quantum 2000, ~1.023e6 cps throttled
- `a2disp` core0 — consumes dirty bitset; never blocks emu on SPI
- Soft-switch changes mark all scanlines dirty
- Idle (no dirty): zero viewport transfer
- Artifact stress: display may fall behind; emulator keeps 1× Apple II cycles

## Host measurements

See `HOST MEASUREMENT` lines from `npm run test:apple2`.
