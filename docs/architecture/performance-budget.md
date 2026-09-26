# Performance budget

Labels: **HOST MEASURED**, **ESP32 ESTIMATE**, **ESP32 PHYSICAL MEASURED**, **UNKNOWN**.

Target SoC: ESP32-S3, 16 MB flash, 8 MB PSRAM, 240 MHz.

## Memory

| Bucket | Budget (bytes) | Label | Notes |
| --- | --- | --- | --- |
| Internal SRAM (text/video/disk port) | ~133 KiB static | ESP32 PHYSICAL MEASURED | Apple II 48K + DiskIIController BSS |
| PSRAM | 8 MiB | ESP32 PHYSICAL MEASURED | Viewport + HGR + Dos33 image (~157 KiB) + optional NIB |
| Dirty metadata | **24 bytes** | ESP32 PHYSICAL MEASURED | |
| Disk track cache | 2 × ≤6656 nibbles | ESP32 PHYSICAL MEASURED | LRU inside Dos33NibbleImage |

## PART D video (ESP32 PHYSICAL MEASURED)

| Path | Time |
| --- | --- |
| HGR Sharp full | render ~7.5 ms + xfer ~62.5 ms |
| HGR Artifact full | render ~27 ms + xfer ~62.4 ms |
| Artifact stress | max ~12 Hz; emu ~1.023e6 cps |

## PART E Disk II (ESP32 PHYSICAL MEASURED)

| Path | Notes |
| --- | --- |
| SD read Esp2BootTest.dsk (143360 B) | ~337 ms (`sd_read_us`) once into PSRAM |
| Track build (cache miss) | ~1.0–1.2 ms |
| Level-4 boot (rot 0) | ~14.5k cycles / ~18 ms wall; cps during boot ~0.8e6 |
| Level-4 boot (rot 128/777) | ~190–210k cycles (longer sector search) |
| Live throttled after boot | ~1.023e6 cps |
| Storage stall | bounded to track-build time; rotation remains cycle-based |

QSPI remains **40 MHz** quad.

## Scheduling

- `a2emu` core1 — cycles + denibble service + 1× throttle
- `a2disp` core0 — dirty → render → CO5300; power/touch
- Disk activity does **not** count as display user activity
