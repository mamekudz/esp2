# CAD outputs

| Subfolder | Purpose |
| --- | --- |
| `source/` | Future parameterized B-Rep generators / editable native sources |
| `step/` | Exported STEP for Plasticity / exchange |
| `parasolid/` | Parasolid when tooling permits |
| `reference-bodies/` | Pointers / copies of vendor bodies used as references |

**Do not** start final enclosure CAD in this milestone.

Official Waveshare STEP (reference body):

`../reference/waveshare/ESP32-S3-Touch-AMOLED-1.64-DAD/.../esp32-s3-touch-amoled-1_64.stp`

Outline dimensions for fit still come from `../dimensions/board.json` (OFFICIAL size drawing), not from raw STEP AABB.
