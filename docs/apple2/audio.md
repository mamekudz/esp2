# Apple II audio (ESP][)

Status labels: **HOST_VERIFIED** / **SUBJECTIVE_REFERENCE** / **PLANNED** /
**NOT_IMPLEMENTED**

## Summary

The Apple II speaker is a **1-bit soft-switch** (`$C030–$C03F`). Sound is
made only by precisely timed toggles. Digitized audio and music engines use
pulse / PWM timing — not a note generator.

ESP][ keeps:

```
6502 cycle-accurate edges {cycle, level}
        →
interval integration (SpeakerPcmRenderer)
        →
PCM (HostAudioBackend / future ESP32 / optional BlueShift reconstruct)
```

until the final render or transport stage. The core must **not** reduce the
stream to beeps, notes, frequencies, or frame-rate events.

## Soft-switch

| Range | Behavior |
| --- | --- |
| `$C030–$C03F` | Any read/write toggles; one edge per access |
| Peek | Must not toggle |

Verified through `AppleIIBus` (HOST_VERIFIED).

## PCM integration

Each PCM sample covers a rational cycle window:

```
cycleStart(i) = (i * cpuHz) / sampleRate
```

The renderer averages the 1-bit high time over `[cycleStart, cycleEnd)`.
Pulses shorter than one sample still change the sample value.

Tested at **44100 Hz** and **48000 Hz**. Emulation timing is independent of
the audio sample rate.

## Overflow

`Speaker` uses a bounded ring (`kRingCapacity = 4096`). On overflow the
**oldest** edge is dropped and `dropped` / diagnostics `bufferOverruns`
increment. Edges are not discarded merely for being denser than PCM.

## Host path

```
Speaker edges → SpeakerPcmRenderer → HostAudioBackend (PCM)
```

Host/browser assumptions stay in the backend, not in the speaker model.

## Star Blazer — SUBJECTIVE_REFERENCE

The author’s browser-based Apple II simulation of **Star Blazer** already
sounded subjectively close to original Apple II digitized speaker audio.

| Label | Value |
| --- | --- |
| Status | **SUBJECTIVE_REFERENCE** |
| Automated golden | **No** (do not add copyrighted media) |
| Purpose | Real-software regression reminder for PWM/digitized fidelity |

Do **not** commit Star Blazer disk images or other copyrighted media.

## Future ESP32 output (PLANNED)

Same edge stream → local piezo / PCM / I2S. Do **not** invent a separate
low-fidelity physical model.

## Future BlueShift™ extension (NOT_IMPLEMENTED here)

Optional ESP][ → BlueShift vendor path (document only in this repo):

```
ESP][  →  cycle-delta speaker edges (BLE vendor extension)
       →  BlueShift™ reconstruct / PCM
       →  optional Classic Bluetooth A2DP on the BlueShift side
```

Prefer **deltaCycles + level** over shipping full PCM on BLE.

The same optional extension channel could later carry other vendor
functions (e.g. haptics). **Not implemented now.**

## Diagnostics (language-neutral)

| Field | Source |
| --- | --- |
| `edgeCount` | Speaker |
| `minDeltaCycles` / `maxDeltaCycles` | Speaker |
| `dropped` (buffer overruns) | Speaker |
| `pcmSamples` / `sampleRate` / `underruns` | SpeakerPcmRenderer |

## Performance note

Host edge→PCM integration is O(edges + samples) with a simple box filter.
ESP32 should keep the same algorithm; optimize buffering/SIMD later without
dropping cycle accuracy. See host measurement `speaker_pcm_100ms_ms`.

## Related docs

- `docs/apple2/speaker-model.md`
- `docs/architecture/audio-architecture.md`
- CLAUDE.md §17
