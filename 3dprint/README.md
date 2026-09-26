# ESP][ 3D print / mechanical workspace

Authoritative area for enclosure design, mechanical references, CAD preparation,
printable part groups, BOM, wiring, and assembly planning.

**This milestone is research + specification foundation only.**

- Do **not** generate final enclosure CAD yet.
- Do **not** invent dimensions obtainable from official sources.
- Do **not** convert assumptions into facts without status labels.

## Intended later workflow

```
specification + official dimensions + reference images
    →
programmatically generated editable B-Rep foundation (~80–90%)
    →
STEP / Parasolid where available
    →
Plasticity
    →
manual aesthetic refinement
    →
printable production parts
```

## Status vocabulary

| Kind | Values |
| --- | --- |
| Dimensions / design facts | `FIXED` `OFFICIAL` `PHYSICALLY_MEASURED` `DERIVED` `PROVISIONAL` `UNKNOWN` `PENDING_COMPONENT_SELECTION` |
| BOM components | `SELECTED` `ORDERED` `RECEIVED` `PHYSICALLY_VERIFIED` `CANDIDATE` `TO_BE_SELECTED` |
| Wiring | `VERIFIED` `DOCUMENTED` `PROPOSED` `UNASSIGNED` `UNKNOWN` |

## Key documents

| File | Role |
| --- | --- |
| [ENCLOSURE-SPEC.md](ENCLOSURE-SPEC.md) | **Authoritative** mechanical architecture (FIXED design intent) |
| [BOM.md](BOM.md) | Bill of materials (MAIN + paddles + joystick) |
| [WIRING.md](WIRING.md) | Physical connection plan |
| [SOURCES.md](SOURCES.md) | Provenance of all references |
| [dimensions/board.json](dimensions/board.json) | Machine-readable Waveshare board dims |
| [dimensions/display-geometry.json](dimensions/display-geometry.json) | Glass / active / visible window + bezel params |
| [dimensions/coordinate-system.json](dimensions/coordinate-system.json) | Enclosure XYZ convention |
| [dimensions/enclosure.json](dimensions/enclosure.json) | Enclosure / print parameters |
| [dimensions/wiring.json](dimensions/wiring.json) | GPIO inventory JSON |
| [dimensions/step-inspection.json](dimensions/step-inspection.json) | Official STEP inspection report |
| [assembly/README.md](assembly/README.md) | Mechanical skeleton / reference assembly |
| [paddles/](paddles/) | 1:1 paddle foundation |
| [joystick/](joystick/) | 1:1 joystick foundation |

## Directory map

```
3dprint/
  reference/waveshare/   official Waveshare mechanical + schematics (local binaries gitignored)
  reference/apple2/      Apple II visual / dimensional references (links + notes)
  cad/source/            OpenSCAD mechanical skeleton (non-styled)
  cad/reference-bodies/  vendor STEP convenience copy (gitignored binaries)
  printable/             color-grouped printable outputs (future)
  assembly/              assembly / skeleton docs
  prototypes/            experimental prints (future)
  tools/                 STEP inspect / sync helpers
```

## Authority vs CLAUDE.md

`CLAUDE.md` §§28–30 hold **high-level permanent mechanical rules** only and
link here. **`ENCLOSURE-SPEC.md` is authoritative** for detailed mechanical
design. Older sketches (magnetic removable monitor/drives, direct Waveshare
USB-C as user port) are superseded.