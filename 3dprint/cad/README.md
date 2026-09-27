# CAD outputs

| Subfolder | Purpose |
| --- | --- |
| `source/` | Parameterized editable sources (OpenSCAD → Plasticity refine) |
| `source/esp2_mechanical_skeleton.scad` | Enclosure layout / collision study |
| `source/esp2_keyboard_reference.scad` | Keyboard + POWER + legend coupon + badge carrier |
| `step/` | Local exports (gitignored `*.stp`; regenerate via tools) |
| `parasolid/` | Parasolid when tooling permits |
| `reference-bodies/` | Pointers / copies of vendor bodies used as references |

Keyboard/badge milestone produces **reference** geometry, not the final enclosure shell.

Official Waveshare STEP (reference body):

`../reference/waveshare/ESP32-S3-Touch-AMOLED-1.64-DAD/.../esp32-s3-touch-amoled-1_64.stp`

Outline dimensions for fit still come from `../dimensions/board.json` (OFFICIAL size drawing), not from raw STEP AABB.
