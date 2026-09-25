# THIRD_PARTY_LICENSES (ESP][)

This file tracks **evaluated** vs **incorporated** third-party software.

Phase-1 hardware references remain in `docs/THIRD_PARTY.md`.

## Incorporated (in tree)

| Component | Upstream | License | Where |
| --- | --- | --- | --- |
| Arduino_GFX (via PlatformIO lib_deps) | moononournation/Arduino_GFX | MIT | PlatformIO package (not vendored) |
| NimBLE-Arduino | h2zero/NimBLE-Arduino | Apache-2.0 | PlatformIO package |

## Evaluated only (not incorporated)

| Project | URL | License | Relevant part | Decision |
| --- | --- | --- | --- | --- |
| AppleWin | https://github.com/AppleWin/AppleWin | GPL-2.0 | NTSC/video/Disk reference | Reference only |
| audetto/AppleWin | https://github.com/audetto/AppleWin | GPL-2.0 | Cross-platform AppleWin | Reference only |
| MAME | https://github.com/mamedev/mame | GPL-2.0+ | apple2 driver accuracy | Reference only |
| POM2 | https://github.com/habib256/pom2 | GPL-3.0 | Cycle-accurate notes | Reference only |
| microM8 | paleotronic.com | Proprietary | — | Not reusable |
| fake6502-class cores | various | PD/CC0 (verify fork) | 6502 CPU | Leading candidate for host harness |
| Bluepad32 docs | https://bluepad32.readthedocs.io/ | — | Gamepad compatibility lists | Documentation reference only (no Bluepad32 code yet) |

When source is actually copied into this repository, move the row to
**Incorporated** and add license text under `third_party/<name>/`.
