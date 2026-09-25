# Task / core strategy (conceptual)

ESP32-S3 has two cores. FreeRTOS is available; **do not** spawn a task per
subsystem.

## Proposed ownership

| Concern | Suggested home | Notes |
| --- | --- | --- |
| Apple II runCycles batches | Core 1 realtime task | Bounded time slice |
| Video decode + color | Same task or tightly coupled | Avoid lock thrash with CPU |
| Display QSPI transfer | Core 0 or DMA-friendly path | Measure contention with BLE |
| BLE / NimBLE | Existing stack tasks | Keep callbacks short |
| SD / media | Worker task + queue | Never block CPU loop on FAT |
| UI / Control Screen | Low priority | May dirty-flag only |
| Audio | Timer ISR or small task | Speaker toggle → buffer |
| Diagnostics | On demand | Idle / UI driven |

## Synchronization

- Single producer / single consumer queues between media ↔ Disk II.
- Atomically published `VideoFrameState` (double buffer or generation counter).
- Input state: snapshot copy once per frame.

## Anti-patterns

- Blocking `SD.open` inside `runCycles`.
- Full-frame memcpy across cores every line.
- Many mutexes around the 6502 core.
