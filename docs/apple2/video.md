# Apple II video (ESP][)

## Logical resolution

Apple II **HGR remains 280 × 192** permanently.

PART B’s physical text path used **240 × 192** because project-owned text
glyphs are rendered in **6×8 cells** (40 × 6 = 240). That was a **TEXT layout
choice**, not an HGR resolution change and not an enclosure crop requirement.

PART C physical Sharp pipeline uses a **280 × 192** viewport so LoRes/HGR
keep every logical pixel. Text is drawn with **7×8 cells** (5px glyph + 2px
gap) so 40 columns span 280 pixels without discarding columns.

## Dirty tracking

`VideoDirtyTracker` records which logical scanlines (0..191) may need a
physical update after authentic `Apple2Bus` RAM writes or video soft-switch
accesses.

Rules:

- Every emulated VRAM store still happens exactly once.
- Dirty bits only optimize CO5300 work (merge runs, skip idle frames).
- Soft-switch mode/page changes mark the **entire** viewport dirty.
- Display may drop/coalesce frames; Apple II cycles are never dropped for FPS.

Metadata size: 192-bit bitset (**24 bytes**).

## Modes (Sharp on device)

| Mode | Source | Notes |
| --- | --- | --- |
| TEXT | text page 1/2 | Sharp glyphs |
| LORES | same memory as text | 40×48 nibbles → 280×192 |
| LORES MIXED | graphics + bottom 4 text rows | |
| HGR Sharp | HGR page 1/2 | logical bits, no artifact yet |
| HGR MIXED | HGR + bottom text | |

LoRes is required for early Apple II titles (e.g. Brick Out–class games).
No title-specific code; no copyrighted media in-repo.

## Artifact / CRT

Host artifact-color tests remain. Physical artifact / Monitor / CRT effects
are **out of scope** until a later milestone.
