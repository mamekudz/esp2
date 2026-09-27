#!/usr/bin/env python3
"""Export provisional ESP][ keyboard reference boxes for Plasticity import.

Source of truth: dimensions/keyboard-layout.json (+ power / badge / envelope).
Outputs (local; cad/step/*.stp gitignored):
  cad/step/esp2_keyboard_reference.stl  — solid boxes (preferred import)
  cad/step/esp2_keyboard_reference.stp  — AABB corner markers + names

Prefer editing JSON + cad/source/esp2_keyboard_reference.scad.
Does not copy Apple artwork. POWER is a separate solid.
"""
from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT_STP = ROOT / "cad" / "step" / "esp2_keyboard_reference.stp"
OUT_STL = ROOT / "cad" / "step" / "esp2_keyboard_reference.stl"


def load(name: str):
    return json.loads((ROOT / "dimensions" / name).read_text(encoding="utf-8"))


def box(xmin, ymin, zmin, xmax, ymax, zmax, name: str):
    return {
        "name": name,
        "xmin": xmin,
        "ymin": ymin,
        "zmin": zmin,
        "xmax": xmax,
        "ymax": ymax,
        "zmax": zmax,
    }


def collect_boxes():
    lay = load("keyboard-layout.json")
    env = load("keyboard-envelope.json")
    pwr = load("power-indicator.json")
    badge = load("badge-carrier.json")
    u = lay["parameters"]["unitMm"]["value"]
    kt = lay["parameters"]["keyTopHeightMm"]["value"]
    ct = lay["parameters"]["carrierPlateThicknessMm"]["value"]
    boxes = []

    ew = env["insertEnvelope"]["widthMm"]["value"]
    ed = env["insertEnvelope"]["depthMm"]["value"]
    boxes.append(box(-4, -4, 0, ew - 4, ed - 4, ct, "carrier_plate"))

    for k in lay["keys"]:
        x0 = k["xU"] * u
        y0 = k["yU"] * u
        x1 = x0 + k["wU"] * u
        y1 = y0 + k["hU"] * u
        boxes.append(box(x0, y0, ct, x1, y1, ct + kt, f"key_{k['id']}"))

    fw = pwr["geometry"]["faceWidthMm"]["value"]
    fd = pwr["geometry"]["faceDepthMm"]["value"]
    bh = pwr["geometry"]["bodyHeightMm"]["value"]
    space = next(k for k in lay["keys"] if k["id"] == "k_space")
    px = space["xU"] * u - fw - 2.5
    py = space["yU"] * u
    pz = ct + kt - pwr["geometry"]["recessBelowBeigeMm"]["value"]
    boxes.append(box(px, py, pz, px + fw, py + fd, pz + bh, "power_indicator"))

    boxes.append(box(0, -40, 0, 40, -18, 1.0, "legend_test_coupon"))

    bw = badge["geometry"]["envelopeWidthMm"]["value"]
    bhgt = badge["geometry"]["envelopeHeightMm"]["value"]
    bt = badge["geometry"]["carrierThicknessMm"]["value"]
    boxes.append(box(30, -40, 0, 30 + bw, -40 + bhgt, bt, "esp2_badge_carrier"))

    boxes.append(box(-7, -7, -1.5, ew - 1, ed - 1, -0.3, "beige_interface_plane"))
    return boxes


def write_stl(boxes, path: Path) -> None:
    def tri(f, n, a, b, c):
        f.write(
            "  facet normal %.6f %.6f %.6f\n" % n
            + "    outer loop\n"
            + "      vertex %.6f %.6f %.6f\n" % a
            + "      vertex %.6f %.6f %.6f\n" % b
            + "      vertex %.6f %.6f %.6f\n" % c
            + "    endloop\n"
            + "  endfacet\n"
        )

    def box_faces(f, b):
        x0, y0, z0 = b["xmin"], b["ymin"], b["zmin"]
        x1, y1, z1 = b["xmax"], b["ymax"], b["zmax"]
        faces = [
            ((0, 0, -1), (x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0)),
            ((0, 0, 1), (x0, y0, z1), (x0, y1, z1), (x1, y1, z1), (x1, y0, z1)),
            ((0, -1, 0), (x0, y0, z0), (x0, y0, z1), (x1, y0, z1), (x1, y0, z0)),
            ((0, 1, 0), (x0, y1, z0), (x1, y1, z0), (x1, y1, z1), (x0, y1, z1)),
            ((-1, 0, 0), (x0, y0, z0), (x0, y1, z0), (x0, y1, z1), (x0, y0, z1)),
            ((1, 0, 0), (x1, y0, z0), (x1, y0, z1), (x1, y1, z1), (x1, y1, z0)),
        ]
        for n, a, b2, c, d in faces:
            tri(f, n, a, b2, c)
            tri(f, n, a, c, d)

    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        f.write("solid esp2_keyboard_reference\n")
        for b in boxes:
            box_faces(f, b)
        f.write("endsolid esp2_keyboard_reference\n")


def write_step_markers(boxes, path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        f.write("ISO-10303-21;\nHEADER;\n")
        f.write("FILE_DESCRIPTION(('ESP][ keyboard reference AABB markers'),'2;1');\n")
        f.write(
            "FILE_NAME('esp2_keyboard_reference.stp','2026-09-27T00:00:00',"
            "('ESP]['),'','export_keyboard_step.py','esp2','');\n"
        )
        f.write("FILE_SCHEMA(('CONFIG_CONTROL_DESIGN'));\nENDSEC;\nDATA;\n")
        eid = 1
        for b in boxes:
            f.write(
                f"#{eid}=CARTESIAN_POINT('{b['name']}_min',"
                f"({b['xmin']:.6f},{b['ymin']:.6f},{b['zmin']:.6f}));\n"
            )
            eid += 1
            f.write(
                f"#{eid}=CARTESIAN_POINT('{b['name']}_max',"
                f"({b['xmax']:.6f},{b['ymax']:.6f},{b['zmax']:.6f}));\n"
            )
            eid += 1
        f.write("ENDSEC;\nEND-ISO-10303-21;\n")


def main() -> int:
    boxes = collect_boxes()
    write_stl(boxes, OUT_STL)
    write_step_markers(boxes, OUT_STP)
    print(f"Wrote {OUT_STL} ({len(boxes)} solids)")
    print(f"Wrote {OUT_STP} (AABB markers)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
