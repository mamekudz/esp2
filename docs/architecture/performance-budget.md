# Performance budget (host baseline + ESP32 targets)

Host numbers are **HOST MEASUREMENT** only. Do not equate host milliseconds to ESP32.

## Apple II timing model

- Nominal CPU class: Apple II / II+ (~1.023 MHz).
- Emulated cycles are the common timeline.
- Real-time target: approximately **1×** original speed via periodic wall-clock sync.

## Host benchmarks

Produced by `test_perf` / existing suites (`npm run test:apple2`):

| Metric | Role |
| --- | --- |
| `cpu_emu_cycles_per_s` | Bare CPU+bus host throughput |
| `machine_2e6_cycles_ms` | CPU+DiskII+softswitches |
| `text_decode_200_ms` | Text path |
| `hgr_decode_50_ms` | HGR logical |
| `artifact_50_ms` | Artifact RGB |
| `dsk_nibble_35tracks_ms` | Track build/cache |
| `encode_decode_1000_ms` | 6-and-2 |
| `speaker_pcm_100ms_ms` | Historical ~0.06 ms class on host |

## Display refresh (provisional)

Evaluate 30 FPS and 60 FPS CO5300 update later on hardware. Dirty flags only if
cheaper than full scanline render.

## First ESP32 visual priority

1. Sharp
2. Correct video mode/page
3. Artifact color (when budget allows)
4. Monitor/CrtTv cosmetics (deferrable)
