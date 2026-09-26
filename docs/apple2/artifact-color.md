# Artifact color research notes

## Requirement

`CompositeColor` must model Apple II HGR artifact behavior (phase / adjacent
bits), not arbitrary RGB bit mapping (`CLAUDE.md` §9–10).

Monochrome modes must come from underlying video data (on/off luminance),
**not** desaturated composite RGB.

## Implemented model (host + ESP32)

`ArtifactRenderer` — **digital HGR phase/pair classification**, shared by host
tests and the ESP32 physical path (`renderScanlineRgb565`).

| Pair bits | Phase (high bit) | Color |
| --- | --- | --- |
| 00 | either | black |
| 11 | either | white |
| 10 | clear | purple |
| 10 | set | blue |
| 01 | clear | green |
| 01 | set | orange |

Phase comes from the high bit of the HGR byte that contains the pair’s **first**
pixel (`pair / 7`). Pairs may span byte boundaries (e.g. pixels 6|7).

**Not claimed:** full NTSC electrical simulation.

## Reference directions (evaluate before copying further code)

| Source | Why useful | License caution |
| --- | --- | --- |
| AppleWin NTSC renderer discussions / docs | Practical artifact + fringe behavior | GPL-2 — reference only unless license accepted |
| MAME apple2 video | Hardware-oriented | GPL |
| Blargg / NTSC filter literature (general) | Separable NTSC concepts | Check each artifact |
| Classic descriptions of HGR odd/even columns → purple/green/blue/orange | Teaching model | Public knowledge |

## Host / ESP32 test patterns (`HgrDecoder::writePattern`)

- `alt1010` / `alt0101` / `isolated` / `hline` / `vline` / `checker`
- `white_run` / `highbit` / `byte_boundary`
- `artifact_ref` — combined phase, pairs, runs, byte-boundary, high-bit bands

Golden: RGB888 full-frame vs per-scanline RGB565 after `toRgb565` must match.

## ESP32 path

```
dirty HGR scanline → decode bits/high → ArtifactRenderer::renderScanlineRgb565
  → RGB565 line in PSRAM viewport → CO5300
```

Prefer small exact LUTs derived from `compositePair` (no approximate palettes).
