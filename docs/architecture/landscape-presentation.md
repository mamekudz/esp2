# Landscape presentation — enclosure note

Status: **`LANDSCAPE_ENCLOSURE_VARIANT = CANDIDATE`**

## Experiment (firmware)

On the **current** Waveshare ESP32-S3-Touch-AMOLED-1.64 (280×456), firmware may
optionally present the Apple II 280×192 frame as:

- **Classic** (default) — portrait viewport used for the verified Galaxian path
- **Landscape** — CW 90° rotation + nearest-neighbor scale, aspect preserved,
  full frame visible (no crop)

This is a **presentation** transform after Sharp / Artifact Color. It does not
rotate Apple II memory or change `AppleIIMachine`.

Physical Galaxian matrix (device; emulation ≈1.023 MHz in all modes):

| Mode | Out | Approx. render_us | Approx. xfer_us |
| --- | --- | --- | --- |
| Classic + Sharp | 280×192 @ (0,48) | ~8 ms | ~15 ms (dirty partial lower) |
| Classic + Artifact | 280×192 @ (0,48) | ~32 ms | ~15 ms |
| Landscape + Sharp | 280×408 @ (0,24) | ~8 ms | ~49 ms |
| Landscape + Artifact | 280×408 @ (0,24) | ~29 ms | ~50 ms |

CDC: `#ESP2PRESENT COLOR|ORIENT|STATUS` (`docs/apple2/video.md`).

## Future enclosure (not started)

If Landscape + Artifact Color proves useful for playability on the tiny AMOLED,
a later mechanical variant may:

| | ESP][ Classic | ESP][ Landscape (candidate) |
| --- | --- | --- |
| Electronics | same 1.64″ board | same board |
| Board / glass | current orientation | rotated ~90° |
| Goal | maximum miniaturization / model character | larger practical Apple II image |
| CAD | existing `3dprint/` track | **not started** |

Do **not** treat this document as an enclosure specification change.
`3dprint/ENCLOSURE-SPEC.md` remains authoritative for Classic until a dedicated
Landscape CAD milestone.

## Other display boards

The 1.64″ Waveshare board remains **VERIFIED_BASELINE**. Larger panels may be
considered later if a better board appears — not in this presentation milestone.
