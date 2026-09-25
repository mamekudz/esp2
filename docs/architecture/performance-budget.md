# Performance budget

Labels: **ESTIMATE** (design), **MEASURED** (on device), **UNKNOWN**.

Target SoC: ESP32-S3, 16 MB flash, 8 MB PSRAM, 240 MHz.

## Memory

| Bucket | Budget (bytes) | Label | Notes |
| --- | --- | --- | --- |
| Internal SRAM free after bring-up | ~240–280 KiB typical free heap | MEASURED* | BLE + display reduce free heap; see serial `[RAM]` |
| PSRAM | 8 MiB | MEASURED | Present on target board |
| Apple II main + aux RAM | 128 KiB (+ language card later) | ESTIMATE | Keep in PSRAM if needed |
| Apple ROM (user-provided) | 12–16 KiB typical ][+ | ESTIMATE | Flash or PSRAM; never commit proprietary ROM |
| HGR working buffer | 8 KiB bitplane + decode scratch | ESTIMATE | Prefer line/scan buffers over full RGB |
| RGB560 framebuffer 280×456 | ~255 KiB | ESTIMATE | 280*456*2; optional if line push used |
| Disk track/sector cache | 4–64 KiB | ESTIMATE | Stream WOZ later; DSK can be SD-backed |
| BLE / NimBLE | tens of KiB | UNKNOWN | Measure with keyboard+gamepad |
| Audio ring | 2–8 KiB | ESTIMATE | Local piezo may need only toggle timing |
| UI / Control Screen | 32–128 KiB | ESTIMATE | Avoid second full framebuffer |
| Stacks / FreeRTOS | 8–24 KiB total | ESTIMATE | Few tasks |

\*Bring-up logs previously showed ~242 KiB free heap after BLE init — re-measure after each subsystem.

## CPU / timing

| Concern | Target | Label |
| --- | --- | --- |
| Apple II effective speed | ≥ 1.0× real-time preferred | UNKNOWN |
| Display refresh | ≥ 30 FPS readable; 60 ideal | UNKNOWN |
| Audio latency local | < 40 ms | UNKNOWN |
| SD disk latency | must not stall CPU mid-instruction batch | ESTIMATE |

## Design implications

1. Prefer **scanline / tile** video over dual full RGB framebuffers.
2. Keep Disk II I/O on a worker with bounded queues; never block the 6502
   step loop on FAT.
3. Measure before optimizing (`CLAUDE.md` §32).

## Measurement hooks (planned)

Debug builds should eventually log: emulation speed, FPS, frame time, free
heap/PSRAM, audio underruns, BLE status, disk latency.
