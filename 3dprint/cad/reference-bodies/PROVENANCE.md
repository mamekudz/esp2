# Reference-body provenance

| File | Role |
| --- | --- |
| `waveshare_esp32_s3_touch_amoled_1_64_OFFICIAL.stp` | Unmodified copy of vendor STEP for CAD assembly convenience |

## Source

- Vendor: Waveshare
- Package: `ESP32-S3-Touch-AMOLED-1.64-DAD.zip`
- Original path under `reference/waveshare/.../esp32-s3-touch-amoled-1_64.stp`
- Retrieval: 2026-09-26 (see `3dprint/SOURCES.md`)
- Status: **OFFICIAL** vendor documentation asset

## Policy

- Keep `reference/waveshare/` as the archival original.
- This copy is for `cad/reference-bodies/` assembly wiring only.
- Do **not** edit either STEP to “fix” drawing dims.
- Outline millimetres remain authoritative in `dimensions/board.json` (size drawing).
- Raw STEP AABB ≠ module outline (see `dimensions/step-inspection.json`).
