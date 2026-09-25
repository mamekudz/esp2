# Bounded queues / thread safety

Preparation for asynchronous boundaries (`CLAUDE.md` + FreeRTOS).

## Principles

1. All cross-task messages are **bounded** (fixed capacity).
2. Dropping policy is explicit: `DropOldest`, `DropNewest`, or `BlockProducer`
   (BlockProducer only where realtime allows).
3. Payloads are POD / trivial structs — no heap in hot paths.
4. Language-neutral IDs in logs (`queue=disk_cmd overflow=1`).

## Planned queues

| Queue | Producer | Consumer | Capacity (EST) | Payload |
| --- | --- | --- | --- | --- |
| `input_events` | BLE/HID/touch | emulator hotkey + Apple II | 64 | `InputEvent` |
| `disk_commands` | UI / library | media worker | 8 | mount/eject/wp |
| `disk_results` | media worker | UI / Disk II waiters | 8 | status |
| `audio_pcm` | audio engine | local/BT sink | 4–8 frames | PCM block |
| `diag_events` | tests | logger / SD writer | 32 | `DiagEvent` |

## Failure behavior

On overflow: increment counter, log once per second, never crash the
emulation loop.

Exact FreeRTOS `QueueHandle_t` wiring is deferred until on-device integration.
Host tests can simulate queues with ring buffers (see `host/`).
