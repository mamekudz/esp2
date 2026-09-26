#!/usr/bin/env python3
"""Inspect Waveshare official STEP — report only, never rewrite vendor geometry."""
from __future__ import annotations

import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "dimensions" / "step-inspection.json"


def main() -> None:
    steps = list((ROOT / "reference" / "waveshare").rglob("*.stp"))
    if not steps:
        raise SystemExit("no .stp under reference/waveshare")
    p = steps[0]
    text = p.read_text(encoding="utf-8", errors="ignore")

    counts = {}
    for name in [
        "CARTESIAN_POINT",
        "ADVANCED_FACE",
        "CLOSED_SHELL",
        "MANIFOLD_SOLID_BREP",
        "CIRCLE",
        "CYLINDRICAL_SURFACE",
        "TOROIDAL_SURFACE",
        "PRODUCT",
        "NEXT_ASSEMBLY_USAGE_OCCURRENCE",
        "AXIS2_PLACEMENT_3D",
        "EDGE_CURVE",
    ]:
        counts[name] = len(re.findall(rf"\b{name}\s*\(", text))

    prods = re.findall(r"PRODUCT\s*\(\s*'([^']*)'", text)
    pts = re.findall(
        r"CARTESIAN_POINT\s*\([^,]*,\s*\(\s*([-+eE0-9.]+)\s*,\s*([-+eE0-9.]+)\s*,\s*([-+eE0-9.]+)",
        text,
    )
    xs = [float(a) for a, b, c in pts]
    ys = [float(b) for a, b, c in pts]
    zs = [float(c) for a, b, c in pts]
    circ = re.findall(r"CIRCLE\s*\([^,]*,[^,]*,\s*([-+eE0-9.]+)\s*\)", text)
    radii = sorted({round(float(r), 4) for r in circ})

    # Drawing-official comparison targets (mm)
    official = {
        "module": (29.12, 44.0, 9.6),
        "pcb": (28.6, 43.5),
        "glass": (26.74, 41.62),
        "active": (22.34, 36.06),
        "mount_spacing": (22.86, 38.5),
    }

    aabb = {
        "min": [min(xs), min(ys), min(zs)],
        "max": [max(xs), max(ys), max(zs)],
        "span": [max(xs) - min(xs), max(ys) - min(ys), max(zs) - min(zs)],
    }

    # Mount hole radii ~ M2 clearance ≈ 1.0–1.2 mm radius often; look near 1.0
    m2_like = [r for r in radii if 0.9 <= r <= 1.3]

    report = {
        "schemaVersion": 1,
        "status": "DERIVED",
        "sourceStep": str(p.relative_to(ROOT)).replace("\\", "/"),
        "fileBytes": p.stat().st_size,
        "entityCounts": counts,
        "productNames": prods[:80],
        "productNameCount": len(prods),
        "cartesianPointCount": len(pts),
        "aabbMm": aabb,
        "circleRadiiMm": radii,
        "m2LikeCircleRadiiMm": m2_like,
        "officialDrawingTargetsMm": official,
        "comparisonNotes": [
            "Raw STEP AABB spans include pin headers / helper geometry and are NOT equal to module outline.",
            "Do not silently 'fix' vendor STEP to match drawing — keep both and document deltas.",
            f"AABB span vs module {official['module']}: "
            f"dx={aabb['span'][0]-official['module'][0]:.3f} "
            f"dy={aabb['span'][1]-official['module'][1]:.3f} "
            f"dz={aabb['span'][2]-official['module'][2]:.3f}",
            "Outline dimensions in board.json remain OFFICIAL from size drawing; STEP is spatial reference body.",
        ],
        "manifoldSolidBrepCount": counts.get("MANIFOLD_SOLID_BREP", 0),
        "closedShellCount": counts.get("CLOSED_SHELL", 0),
    }

    OUT.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
