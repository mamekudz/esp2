# Performance budget

Labels: **HOST MEASURED**, **ESP32 ESTIMATE**, **ESP32 PHYSICAL MEASURED**, **UNKNOWN**.

Target SoC: ESP32-S3, 16 MB flash, 8 MB PSRAM, 240 MHz.

## Memory

| Bucket | Budget (bytes) | Label | Notes |
| --- | --- | --- | --- |
| Internal SRAM free after bring-up | ~240–280 KiB typical free heap | ESP32 PHYSICAL MEASURED | Text-port live heap ~206 KiB |
| PSRAM | 8 MiB | ESP32 PHYSICAL MEASURED | Present; viewport FB staged in PSRAM |
| 6502 core state | <1 KiB | ESP32 ESTIMATE | fake6502 globals + wrapper |
| Apple II main RAM (48K II+) | 48 KiB | ESP32 PHYSICAL MEASURED | **internal SRAM BSS** (not PSRAM) |
| Aux / language card (later) | +64 KiB | ESP32 ESTIMATE | Not in host baseline |
| ROM (user-provided) | 12 KiB typical ][+ | ESP32 ESTIMATE | Flash or PSRAM; never commit proprietary ROM |
| Video state / soft switches | <1 KiB | ESP32 ESTIMATE | |
| HGR bitplane decode | 280×192 ≈ 52.5 KiB bits + 7.5 KiB highbits | ESP32 ESTIMATE | Prefer line buffers on device |
| Text viewport RGB565 240×192 | 92 160 B | ESP32 PHYSICAL MEASURED | PSRAM staging; DMA chunk copy in QSPI driver |
| Disk buffers | 4–64 KiB | ESP32 ESTIMATE | Stream WOZ later |
| Speaker event ring | ~16–32 KiB host; 2–8 KiB device | HOST / ESTIMATE | Bounded ring in `Speaker` |
| BLE / NimBLE | tens of KiB | UNKNOWN | Measure with keyboard+gamepad |
| UI / Control Screen | 32–128 KiB | ESP32 ESTIMATE | Avoid second full framebuffer |
| Stacks / FreeRTOS | 8–24 KiB total | ESP32 ESTIMATE | emu + display tasks |

## Host measurements

Run `node host/tools/build_and_test_apple2.mjs` and read lines prefixed
`HOST MEASUREMENT`. These are **not** ESP32 performance.

| Metric | Label |
| --- | --- |
| `cpu_1e6_cycles_ms` | HOST MEASURED |
| `cpu_emu_cycles_per_s` | HOST MEASURED (`test_perf`) |
| `machine_2e6_cycles_ms` | HOST MEASURED (`test_perf`) |
| `text_decode_200_ms` | HOST MEASURED (`test_perf`) |
| `hgr_decode_50_ms` | HOST MEASURED |
| `artifact_50_ms` | HOST MEASURED |
| `dsk_nibble_35tracks_ms` | HOST MEASURED (`test_perf`) |
| `encode_decode_1000_ms` | HOST MEASURED (`test_perf`) |
| `speaker_pcm_100ms_ms` | HOST MEASURED (~0.06 ms class historically) |

## ESP32 PHYSICAL MEASURED — text-port display (PART B)

Env: `apple2_text`. QSPI: **40 MHz**, 4 lanes, SPI2, DMA chunk ≤1024 px, blocking poll.
Apple II logical viewport: **240×192** Sharp text (not full 280×456 panel).

### Before (PART A path)

| Stage | Time | Notes |
| --- | --- | --- |
| text decode | ~170 µs | |
| glyph fill line buffers | ~3.2 ms | |
| `fillScreen` full 280×456 | **~129.5 ms** | dominant |
| 192× scanline `draw16bitRGBBitmap(280×1)` | **~79.2 ms** | addr-window per line |
| **total** | **~212 ms** | |

Theoretical payload @ 40 MHz×4: 280×192 RGB565 ≈ 5.4 ms; 240×192 ≈ 4.6 ms; panel ≈ 12.8 ms.
→ Problem was **software/transaction overhead**, not raw QSPI bandwidth.

### After (viewport + decouple)

| Metric | Value |
| --- | --- |
| Full viewport 240×192 (render+xfer) | **~58.9 ms** (~17 Hz max) |
| of which glyph render | ~5.4 ms |
| of which QSPI xfer | ~53.5 ms |
| 24× block8 strips | ~58.6 ms (similar) |
| One dirty text row (240×8) | **~2.35 ms** |
| Raw 6502 cps (display idle) | **~1.32e6** (~1.29×) |
| Same-core blocking display | ~3.3e4 cps (do not use) |
| Decoupled + 1× throttle, display active | **~1.023e6 cps** stable |
| SPI clock | 40 MHz (unchanged) |
| DMA | yes (driver); before+after |

Scheduling: emu task core 1 owns cycles; display task core 0; generation snapshot;
stale frames skipped; render does **not** call `notifyActivity`.

## Level-5 timing / throttle notes

- Emulated 6502 cycles are the Apple II timeline (speaker, paddles, Disk II).
- Wall-clock sync targets ~1× original II/II+ speed in batches — not per instruction.
- If late: correctness > input > audio > display > cosmetics; never skip Disk II cycles for FPS.
- First ESP32 visual priority: **Sharp**; CRT cosmetics deferrable.
- Execution quantum used on device: **2000** cycles/checkpoint.

## CPU / timing (ESP32 targets)

| Concern | Target | Label |
| --- | --- | --- |
| Apple II effective speed | ≥ 1.0× real-time preferred | **ESP32 PHYSICAL MEASURED** (~1.0× throttled) |
| Text viewport refresh | readable; full redraw ~17 Hz class | ESP32 PHYSICAL MEASURED |
| Audio latency local | < 40 ms | UNKNOWN |
| SD disk latency | must not stall CPU mid-instruction batch | ESP32 ESTIMATE |

## Design implications

1. Prefer **scanline / tile** video over dual full RGB framebuffers.
2. Keep Disk II I/O on a worker with bounded queues; never block the 6502
   step loop on FAT.
3. Measure before optimizing (`CLAUDE.md` §32).
4. No repeated heap allocation in CPU/bus/video hot paths.
5. Never let CO5300 transfer define Apple II emulated time.
