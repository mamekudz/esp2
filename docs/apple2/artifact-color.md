# Artifact color research notes

## Requirement

`CompositeColor` must model Apple II HGR artifact behavior (phase / adjacent
bits), not arbitrary RGB bit mapping (`CLAUDE.md` §9–10).

Monochrome modes must come from underlying video data (on/off luminance),
**not** desaturated composite RGB.

## Reference directions (evaluate before copying code)

| Source | Why useful | License caution |
| --- | --- | --- |
| AppleWin NTSC renderer discussions / docs | Practical artifact + fringe behavior | GPL-2 — reference only unless license accepted |
| MAME apple2 video | Hardware-oriented | GPL |
| Blargg / NTSC filter literature (general) | Separable NTSC concepts | Check each artifact |
| Classic descriptions of HGR odd/even columns → purple/green/blue/orange | Teaching model | Public knowledge |

## Host test patterns (implemented in `host/`)

Synthetic 280×192 bit patterns:

- alternating 1010… / 0101…
- isolated single pixels
- adjacent pairs spanning phase
- white runs / black runs
- left/right edge cases

Output: logical “signal groups” + optional PPM for visual inspection.

## ESP32 translation notes

Prefer:

- precomputed palette LUTs (phase × neighbor bits → RGB565)
- integer math
- per-line processing

Avoid GPU-style shaders.
