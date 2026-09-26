#!/usr/bin/env python3
"""Mechanical foundation consistency checks (no CAD generation)."""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parent
errors: list[str] = []


def load(p: Path):
    return json.loads(p.read_text(encoding="utf-8"))


def main() -> int:
    disp = load(ROOT / "dimensions" / "display-geometry.json")
    coord = load(ROOT / "dimensions" / "coordinate-system.json")
    enc = load(ROOT / "dimensions" / "enclosure.json")
    paddle = load(ROOT / "paddles" / "dimensions" / "paddle.json")
    joy = load(ROOT / "joystick" / "dimensions" / "joystick.json")

    vis = disp["DISPLAY_VISIBLE_WINDOW"]
    if vis["status"] != "PROVISIONAL_CAD_CANDIDATE":
        errors.append("visible window must remain PROVISIONAL_CAD_CANDIDATE")

    vw = vis["monitor_visible_width"]["value"]
    vh = vis["monitor_visible_height"]["value"]
    aw = disp["oled_active_width"]["value"]
    ah = disp["oled_active_height"]["value"]
    r = disp["oled_upper_radius"]["value"]
    if abs(vw - aw) > 1e-6:
        errors.append(f"candidate visible width {vw} != active {aw}")
    if abs(vh - (ah - r)) > 1e-6:
        errors.append(f"candidate visible height {vh} != active-r {ah - r}")
    if abs(disp["bezel_overlap_top"]["value"] - r) > 1e-6:
        errors.append("bezel_overlap_top should equal oled_upper_radius for this candidate")

    if coord.get("status") != "FIXED":
        errors.append("coordinate-system status should be FIXED")
    if enc.get("coordinateSystem", {}).get("Z") != "bottom→top":
        errors.append("enclosure.json coordinateSystem Z mismatch")

    if paddle["scale"]["value"] != "1:1_ORIGINAL":
        errors.append("paddle scale must be 1:1_ORIGINAL")
    if joy["scale"]["value"] != "1:1_ORIGINAL":
        errors.append("joystick scale must be 1:1_ORIGINAL")
    if (
        paddle["physicalReference"]["buttonGeometry"]["status"]
        != "DO_NOT_USE_PHYSICAL_AS_ORIGINAL"
    ):
        errors.append("paddle button warning missing")
    if not joy["positionSensing"]["doNotUseOriginalCenteringAsSensor"]:
        errors.append("joystick must not use original centering as sensor")
    if joy["centering"]["selection"] != "NOT_SELECTED":
        errors.append("centering actuator must remain NOT_SELECTED")
    if not joy["centering"]["electronicallySwitchable"]:
        errors.append("joystick centering must be electronically switchable")

    claude = (REPO / "CLAUDE.md").read_text(encoding="utf-8")
    m = re.search(r"# PHYSICAL DESIGN(.*?)# SOFTWARE ARCHITECTURE", claude, re.S)
    if not m:
        errors.append("CLAUDE.md PHYSICAL DESIGN section missing")
    else:
        phys = m.group(1)
        if re.search(r"hidden magnets|attaches using hidden magnets", phys, re.I):
            errors.append("CLAUDE.md still describes magnetic monitor/drives as current rules")
        if "Do NOT assume an internal USB extension" in phys:
            errors.append("CLAUDE.md still forbids internal USB extension (superseded)")
        if "ENCLOSURE-SPEC.md" not in phys:
            errors.append("CLAUDE.md must link ENCLOSURE-SPEC.md")
        if "Waveshare USB-C stays **internal**" not in phys and "internal" not in phys.lower():
            errors.append("CLAUDE.md should state Waveshare USB-C is internal")

    skel = ROOT / "cad" / "source" / "esp2_mechanical_skeleton.scad"
    if not skel.is_file():
        errors.append("missing mechanical skeleton SCAD")

    step_report = ROOT / "dimensions" / "step-inspection.json"
    if list((ROOT / "reference" / "waveshare").rglob("*.stp")):
        if not step_report.is_file():
            errors.append(
                "STEP present but step-inspection.json missing — run inspect_waveshare_step.py"
            )
        else:
            rep = load(step_report)
            if rep.get("manifoldSolidBrepCount", 0) < 1:
                errors.append("STEP inspection found no MANIFOLD_SOLID_BREP")

    if errors:
        print("FAIL")
        for e in errors:
            print(" -", e)
        return 1
    print("OK mechanical foundation checks")
    return 0


if __name__ == "__main__":
    sys.exit(main())
