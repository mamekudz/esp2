# Apple II video (ESP][)

## Logical resolution

Apple II **HGR remains 280 × 192** permanently.

PART B’s physical text path used **240 × 192** because project-owned text
glyphs are rendered in **6×8 cells** (40 × 6 = 240). That was a **TEXT layout
choice**, not an HGR resolution change and not an enclosure crop requirement.

PART C/D physical pipelines use a **280 × 192** viewport so LoRes/HGR keep
every logical pixel. Text is drawn with **7×8 cells** (5px glyph + 2px gap)
so 40 columns span 280 pixels without discarding columns.

## Dirty tracking

`VideoDirtyTracker` records which logical scanlines (0..191) may need a
physical update after authentic `Apple2Bus` RAM writes or video soft-switch
accesses.

Rules:

- Every emulated VRAM store still happens exactly once.
- Dirty bits only optimize CO5300 work (merge runs, skip idle frames).
- Soft-switch mode/page changes mark the **entire** viewport dirty.
- Display may drop/coalesce frames; Apple II cycles are never dropped for FPS.
- Artifact color regenerates each **complete dirty scanline** (280 px). No
  per-pixel dirty metadata; byte-boundary neighbor pairs stay correct without
  horizontal dirty expansion.

Metadata size: 192-bit bitset (**24 bytes**).

## Presentation composition (not Apple II state)

Apple II soft-switches know **HGR vs text/LoRes**. ESP][ presentation is three
independent dimensions (no hard-coded combo modes):

| Dimension | Values | Role |
| --- | --- | --- |
| **Orientation** | Classic, Landscape | Panel layout / scale |
| **Monitor** | White, Green, Amber, Artifact Color | Appearance / HGR color model |
| **Effect** | Clean, CRT/TV | Optional post-monitor pass |

| Monitor | Behavior |
| --- | --- |
| `White` (former Sharp) | True mono from bits / luminance → white phosphor |
| `Green` / `Amber` | Same mono path → phosphor RGB565 (`phosphor.hpp` peaks) |
| `Artifact` | Host `ArtifactRenderer` composite pair + high-bit phase |

CRT/TV (V1, cheap): subtle horizontal softness + scanline dim; optional chroma
bleed only with Artifact. No barrel distortion / bloom / persistence.

Switching monitor/effect/orient invalidates the viewport and re-renders; it
does **not** change Apple II RAM or soft-switches.

Serial (development CDC):

- `#ESP2PRESENT MONITOR WHITE|GREEN|AMBER|ARTIFACT`
- `#ESP2PRESENT EFFECT CLEAN|CRT`
- `#ESP2PRESENT COLOR SHARP|ARTIFACT` (legacy aliases → WHITE|ARTIFACT)
- `#ESP2PRESENT ORIENT CLASSIC|LANDSCAPE`
- `#ESP2PRESENT STATUS`

## Presentation orientation (not Apple II state)

| PresentOrientation | Behavior |
| --- | --- |
| `Classic` (default) | 280×192 viewport at panel `(0,48)` — enclosure / portrait bring-up |
| `Landscape` | CW 90° + nearest-neighbor scale to fit 280×456, aspect preserved |

`AppleIIMachine` does not know about panel orientation. Landscape is an
**optional** presentation experiment on the current 1.64″ board; Classic remains
the verified default.

## Modes (physical)

| Mode | Source | Notes |
| --- | --- | --- |
| TEXT | text page 1/2 | Sharp glyphs |
| LORES | same memory as text | 40×48 nibbles → 280×192 |
| LORES MIXED | graphics + bottom 4 text rows | |
| HGR Sharp | HGR page 1/2 | logical bits |
| HGR Artifact Color | same HGR RAM | pair/phase → RGB565 |
| HGR MIXED Artifact | HGR + bottom text | artifact stops at scanline 160 |

## What “Artifact Color” means

ESP][ Artifact Color is the **same educational digital model** as the host
`ArtifactRenderer`:

- adjacent even/odd bit pairs
- high bit of the HGR byte containing the pair’s first pixel → phase
- maps to black / white / purple / green / blue / orange

It is **not** a full analog NTSC electrical signal chain (no chroma subcarrier
demodulation, no TV bandpass filter, no CRT phosphor simulation). CRT/Monitor
display effects remain a separate later milestone.

Host and ESP32 share `ArtifactRenderer::renderScanlineRgb565` so logical
RGB565 output matches host RGB888 after deterministic `toRgb565` quantization.

## HGR-clear cycle diagnosis

PART C logged ~**8e6** cycles for the synthetic HGR-clear path. That figure was
the firmware **timeout** while parked in the post-clear `JMP *`, not the cost of
clearing 8 KiB.

Exact clear cost (host + fixed ROM): **~90 515 cycles** for one 8 KiB page
(every `STA ($00),Y` executed). A separate ROM bug (`LDA $01`/`CMP` left A
holding the page byte so only the first 256 bytes were truly zeroed) is fixed;
measurement wording now uses the exact done PC `$E819`.

## Artifact / CRT

Physical Artifact Color: **PART D**. Monitor / CRT_TV effects: still later.
