# ESP][ mechanical skeleton / reference assembly

Status: **NON-STYLED layout study** — not final enclosure CAD.

## Purpose

Collision and layout validation only:

- Waveshare module envelope (official outline dims)
- `DISPLAY_GLASS_PHYSICAL` / `DISPLAY_ACTIVE_AREA` / `DISPLAY_VISIBLE_WINDOW`
- Board insertion envelope (+Z from below)
- USB path keep-out (**PENDING_COMPONENT_SELECTION**)
- Approximate computer / monitor / drive / keyboard / rear-I/O volumes
- Bottom-plane reference

## Files

| Path | Role |
| --- | --- |
| `cad/source/esp2_mechanical_skeleton.scad` | Editable B-Rep-style primitives (OpenSCAD) |
| `cad/source/esp2_keyboard_reference.scad` | Keyboard carrier + legends + POWER + legend coupon + badge carrier |
| `dimensions/keyboard-layout.json` | Machine-readable key layout (relative + provisional mm) |
| `cad/reference-bodies/PROVENANCE.md` | How to obtain official STEP |
| `cad/reference-bodies/*.stp` | Local-only vendor STEP copy (gitignored) |
| `dimensions/coordinate-system.json` | Axis convention |
| `dimensions/display-geometry.json` | Bezel / visible-window parameters |
| `dimensions/step-inspection.json` | Programmatic STEP inspection report |

## Sync vendor STEP (local)

```bash
python 3dprint/tools/sync_reference_bodies.py
python 3dprint/tools/inspect_waveshare_step.py
```

Import the STEP into Plasticity / FreeCAD alongside the SCAD skeleton for spatial checks.
Do **not** edit the vendor STEP to match drawing numbers — document deltas instead.

## Keyboard reference assembly

Open `cad/source/esp2_keyboard_reference.scad` for a Plasticity-friendly preview of:

- beige interface plane (not full enclosure)
- brown one-piece keyboard (keys + hidden webs + carrier)
- white legend placeholder inserts
- separate recessed POWER indicator
- legend test coupon (0.2 mm nozzle stroke candidates)
- ESP][ badge carrier envelope (no Apple artwork)

Local mesh/STEP markers (optional):

```bash
python 3dprint/tools/export_keyboard_step.py
```

Writes `cad/step/esp2_keyboard_reference.stl` (+ AABB STEP markers). Prefer editing the SCAD/JSON; refine aesthetics in Plasticity.

**Coordinates:** mm; +X left→right, +Y front→rear (space toward front), +Z up; keyboard inserts from below (+Z into beige).

## Explicitly out of scope

Final fillets, full enclosure shell, vents, drive-front art, final badge artwork, printable production STLs as primary source.
