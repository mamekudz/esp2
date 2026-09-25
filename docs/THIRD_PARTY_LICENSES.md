# THIRD_PARTY_LICENSES (ESP][)

This file tracks **evaluated** vs **incorporated** third-party software.

Phase-1 hardware references remain in `docs/THIRD_PARTY.md`.

## Incorporated (in tree)

| Component | Upstream | License | Where |
| --- | --- | --- | --- |
| Arduino_GFX (via PlatformIO lib_deps) | moononournation/Arduino_GFX | MIT | PlatformIO package (not vendored) |
| NimBLE-Arduino | h2zero/NimBLE-Arduino | Apache-2.0 | PlatformIO package |
| fake6502 v1.3 | https://github.com/C-Chads/MyLittle6502 @ `3078e2c` | Public domain / CC0 | `third_party/fake6502/` (host wrapper `Cpu6502`; not in firmware yet) |

## Evaluated only (not incorporated)

| Project | URL | License | Relevant part | Decision |
| --- | --- | --- | --- | --- |
| AppleWin | https://github.com/AppleWin/AppleWin | GPL-2.0 | NTSC/video/Disk reference | Reference only |
| audetto/AppleWin | https://github.com/audetto/AppleWin | GPL-2.0 | Cross-platform AppleWin | Reference only |
| MAME | https://github.com/mamedev/mame | GPL-2.0+ | apple2 driver accuracy | Reference only |
| POM2 | https://github.com/habib256/pom2 | GPL-3.0 | Cycle-accurate notes | Reference only |
| microM8 | paleotronic.com | Proprietary | — | Not reusable |
| Bluepad32 docs | https://bluepad32.readthedocs.io/ | — | Gamepad compatibility lists | Documentation reference only (no Bluepad32 code yet) |
| apple2js | https://github.com/whscullin/apple2js | MIT (emulator only) | Catalog format, UX reference, optional oracle | **EVALUATED / DEVELOPMENT REFERENCE** — no emulator source incorporated; website disk media is **not** covered by MIT |
| a2kit | https://github.com/dfgordon/a2kit | MIT | Disk format CLI/oracle | Optional external tool only |
| DiskM8 | https://github.com/paleotronic/diskm8 | GPL-3.0 | Disk cataloging CLI | External invoke only — do not incorporate |
| EWM | https://github.com/st3fan/ewm | MIT | Disk II / emulator reference | Reference only |
| tcjennings/apple2 | https://github.com/tcjennings/apple2 | MIT (all contents claim) | screen_address WOZ fixture | Fixture under `fixtures/redistributable/` with NOTICE |

When source is actually copied into this repository, move the row to
**Incorporated** and add license text under `third_party/<name>/`.
