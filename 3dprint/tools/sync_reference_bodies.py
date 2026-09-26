#!/usr/bin/env python3
"""Copy official Waveshare STEP into cad/reference-bodies/ (local only)."""
from __future__ import annotations

import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC_ROOT = ROOT / "reference" / "waveshare"
DST = ROOT / "cad" / "reference-bodies" / "waveshare_esp32_s3_touch_amoled_1_64_OFFICIAL.stp"


def main() -> None:
    steps = list(SRC_ROOT.rglob("*.stp")) + list(SRC_ROOT.rglob("*.STP"))
    if not steps:
        raise SystemExit(
            "No vendor STEP under reference/waveshare — extract DAD.zip first "
            "(see SOURCES.md)."
        )
    src = steps[0]
    DST.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, DST)
    print(f"OK {src} -> {DST} ({DST.stat().st_size} bytes)")
    print("NOTE: *.stp under cad/reference-bodies/ is gitignored; keep archival original under reference/waveshare/.")


if __name__ == "__main__":
    main()
