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

## PART B → PART C display (ESP32 PHYSICAL MEASURED)

| Path | Time / notes |
| --- | --- |
| PART B text 240×192 full | ~58.9 ms |
| PART C Sharp full 280×192 TEXT | ~70 ms (render~7.5 + xfer~62.5) |
| PART C LORES full | ~69 ms |
| PART C HGR Sharp full | ~72 ms |
| TEXT partial (1 row / 8 lines) | ~9.9 ms / 4480 B |
| LORES partial (1 text row) | ~9.2 ms / 4480 B |
| HGR partial (1 scanline) | ~10.8 ms / 560 B |
| Dirty overhead | cps_off≈cps_on ≈ **1.32e6** (negligible) |

QSPI remains **40 MHz** quad, DMA chunk ≤1024 px.

## Scheduling

- `a2emu` core1 — cycle owner, quantum 2000, ~1.023e6 cps throttled
- `a2disp` core0 — consumes dirty bitset; never blocks emu on SPI
- Soft-switch changes mark all scanlines dirty
- Idle (no dirty): zero viewport transfer

## Host measurements

See `HOST MEASUREMENT` lines from `npm run test:apple2`.
