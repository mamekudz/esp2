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

## Explicitly out of scope

Final fillets, Apple II surface styling, vents, keyboard details, logos, drive-front art, printable STLs.
