# Sources — ESP][ mechanical / CAD research

Retrieval date (this foundation): **2026-09-26**.

Redistribution: Waveshare documentation/files are vendor-published for customers;
treat as **reference assets** for this open-source project. Do not assume blanket
relicensing of STEP/DWG beyond normal vendor documentation use. Third-party CAD
models are **not** copied into Git unless redistribution is clearly permitted.

---

## 1. Waveshare ESP32-S3-Touch-AMOLED-1.64 (OFFICIAL)

| File / resource | Local path | URL | Notes |
| --- | --- | --- | --- |
| Product wiki | — | https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.64 | Features, dimensions section, demos |
| Resources index | — | https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.64/Resources-And-Documents | Schematic / DAD / datasheets |
| Mechanical package (DAD) | `reference/waveshare/ESP32-S3-Touch-AMOLED-1.64-DAD.zip` | https://files.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.64/ESP32-S3-Touch-AMOLED-1.64-DAD.zip | Contains PDF + DWG + STEP |
| Extracted STEP | `reference/waveshare/ESP32-S3-Touch-AMOLED-1.64-DAD/.../esp32-s3-touch-amoled-1_64.stp` | (inside DAD.zip) | Reference body; AABB ≠ outline dims |
| Extracted PDF drawing | same DAD folder `*-20241221.pdf` | (inside DAD.zip) | Vector drawing (little extractable text) |
| Extracted DWG | same DAD folder `*-20241221.dwg` | (inside DAD.zip) | Preferred for precise CAD import |
| Size detail image | `reference/waveshare/ESP32-S3-Touch-AMOLED-1.64-details-size.jpg` | https://www.waveshare.com/img/devkit/ESP32-S3-Touch-AMOLED-1.64/ESP32-S3-Touch-AMOLED-1.64-details-size.jpg | **Primary OFFICIAL mm source** for `board.json` |
| V1 schematic | `reference/waveshare/ESP32-S3-Touch-AMOLED-1.64-schematic.pdf` | https://files.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.64/ESP32-S3-Touch-AMOLED-1.64-schematic.pdf | |
| V2 / Rev1.1 schematic | `reference/waveshare/ESP32-S3-Touch-AMOLED-1.64-Rev1.1.pdf` | https://files.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.64/ESP32-S3-Touch-AMOLED-1.64-Rev1.1.pdf | |
| Pin header soldering guide | `reference/waveshare/ESP32-S3-Touch-AMOLED-1.64-JW_Explanation.pdf` | https://files.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.64/ESP32-S3-Touch-AMOLED-1.64-JW_Explanation.pdf | |
| Arduino pin variant | `reference/waveshare/pins_arduino.h.txt` | https://raw.githubusercontent.com/espressif/arduino-esp32/master/variants/waveshare_esp32_s3_touch_amoled_164/pins_arduino.h | Mirror for wiring research |
| CO5300 datasheet | (not mirrored yet) | https://files.waveshare.com/wiki/common/Co5300_Datasheet.pdf | Display controller |
| FT3168 datasheet | (not mirrored yet) | https://files.waveshare.com/wiki/common/DATA_SHEET_FT3168.pdf | Touch |
| QMI8658 datasheet | (not mirrored yet) | https://files.waveshare.com/wiki/common/QMI8658C_datasheet_rev_0.9.pdf | IMU |

ESP][ firmware pin evidence: `include/board_pins.h` (project-owned).

---

## 2. Apple II / II Plus dimensional references

| Source | URL | Claimed dims | Status | Usefulness |
| --- | --- | --- | --- | --- |
| Apple TIL00105 Hardware Specs | http://absurdengineering.org/library/MASTER%20Tech%20Info%20Library/Apple%20II%20Hardware/Hardware%20Specifications/TIL00105%20-%20Apple%20II%20and%20Apple%20II%20Plus%20-%20Hardware%20Specs%20(Discontinued%206-83).pdf | H 4.5" × W 15.1" × L 18.0" | OFFICIAL (Apple TIL reprint) | Full-size envelope reference |
| RigPix Apple II page | https://rigpix.com/vcomp/apple_ii.htm | 387 × 108 × 451 mm | DOCUMENTED (secondary) | Cross-check |
| Applefritter thread (IIe measurements) | https://www.applefritter.com/node/22888 | Community measurements; Disk II ~6.125×3.8125×8.625" | PROVISIONAL | Drive proportions |
| Auction listing of enclosure blueprints | https://www.lot-art.com/auction-lots/Apple-II-Computer-6-Original-Engineering-Blueprints-From-the/4046-apple_ii-20.8.26-rr | Drawing 00500 etc. | UNKNOWN (images not free) | Awareness only |

**Not yet downloaded into `reference/apple2/`:** TIL PDF and photographs — add when redistribution comfort is confirmed; until then link-only.

---

## 3. Existing Apple II CAD / printable models (research only)

| Model | Author / site | Format | License (claimed) | In Git? | Usefulness |
| --- | --- | --- | --- | --- | --- |
| Apple II Plus Case | Dosfish — Printables 658773 | STL | Check Printables page | **No** | Mesh proportions only; author notes slightly longer than original |
| Apple II or II Plus | Charles Mangin — Printables 1713037 / Thingiverse | STL | Check page | **No** | Visual mesh reference |
| Apple II+ IO mounts | alnwlsn.com | STL (partial) | Site terms | **No** | Rear slot accessories, not full case |
| Sleeper conversion parts | CrexisEnnex — Printables | STL | Check page | **No** | Not authentic proportions |

No redistributable **Parasolid / STEP / FreeCAD B-Rep** full Apple II case was located in this research pass. Prefer mesh as **shape reference** only; ESP][ CAD should be parametric B-Rep from specification.

---

## 4. Project-internal design authority

| Document | Role |
| --- | --- |
| `CLAUDE.md` §§28–30 | High-level permanent mechanical rules + link |
| `3dprint/ENCLOSURE-SPEC.md` | **Authoritative** detailed mechanical FIXED intent |
| `3dprint/dimensions/*.json` | Machine-readable dims / wiring / display / coordinates |
| `3dprint/BOM.md` | MAIN + paddle + joystick BOM sections |
| `3dprint/WIRING.md` | Connection plan (no invented GPIOs) |

---

## 5. Paddles — photo / documentation research (curated)

Retrieval date: **2026-09-26**. Images **not** mirrored into Git (rights / redundancy). Store URLs only unless a clearly redistributable set is later approved.

| Source | URL | License / status | Covers | Usefulness |
| --- | --- | --- | --- | --- |
| Stephen Edmonds — Hand Controllers //e //c | https://computers.popcorn.cx/apple/peripherals/input/hand-controllers/ | CC-BY-SA 2.5 (site claim) | Box, top, underside, leaflet | Strong exterior photo set |
| Apple II History Museum — Hand Controllers II | https://www.duxburysystems.org/downloads/library/texas/apple/history/museum/peripherals_apple/handcontrollersii.html | Site terms | Original tiny-button vs redesigned | Button history (use for **original** button language) |
| PCM Museum — Apple II Paddles | http://www.thepcmuseum.com/apple/A2Paddles/default.htm | Site terms | IIe/IIc paddles | Visual |
| Rebuilding Paddles & Joysticks (PDF) | https://mirrors.apple2.org.za/ftp.apple.asimov.net/documentation/hardware/misc/Rebuilding%20Paddles%20%26%20Joysticks.pdf | Unknown / archival mirror | Repair, pot ~150 kΩ, button advice | Technical; not exterior CAD authority |
| Atari magazine era review (knob/button sizes) | https://www.atarimagazines.com/cva/v1n1/joysticks.php | Publication archive | Knob ≈1.25", original FIRE ≈3/16" | DERIVED sizes only |

**Physical validation:** project-author original paddles = `AUTHORITATIVE_PHYSICAL_REFERENCE_AVAILABLE` for shell/knob/seam/cable; **modified buttons on those units are not original-button authority**.

---

## 6. Joystick — photo / documentation research (curated)

| Source | URL | License / status | Covers | Usefulness |
| --- | --- | --- | --- | --- |
| Stephen Edmonds — Joystick //e //c | https://computers.popcorn.cx/apple/peripherals/input/joystick/ | CC-BY-SA 2.5 (site claim) | Top, underside trim dials | Exterior photos |
| Applefritter — A2M2002 dimensions | https://www.applefritter.com/content/apple-joystick-dimensions-a2m2002-or-a2m2012 | Forum terms | Community-measured mm | DERIVED / PROVISIONAL dims |
| Applefritter — cleaning / centering teardown | https://www.applefritter.com/content/cleaning-joystick-pots | Forum terms | Springs, actuators, pots | Centering **inspiration** |
| Apple Joystick //e Operations Manual (PDF mirrors) | e.g. https://mirrors.apple2.org.za/Apple%20II%20Documentation%20Project/Peripherals/Digitizers/Apple%20Joystick%20IIe/Manuals/Apple%20Joystick%20IIe%20Manual.pdf | Apple doc archival | Self-centering key-mark procedure | Historical centering UX |
| Apple Joystick IIe/IIc User instructions | Apple II Documentation Project mirrors | Apple doc archival | Trim / centering | Reference |

**Physical validation:** project-author original joystick = `AUTHORITATIVE_PHYSICAL_REFERENCE_AVAILABLE`.

**ESP][ FIXED deltas vs original:** contactless X/Y sensing; electronically switchable centering springs (force only).

---

## 7. Keyboard / badge working references (LOCAL)

Retrieval / presence: files already under `dev/drafts/` (do **not** re-download duplicates).

| Local filename | Source | Purpose | Asset class | NAS | Redistribution / license |
| --- | --- | --- | --- | --- | --- |
| `dev/drafts/keylayout_example.png` | scullinsteel.com / Apple ][js (visual reference) | Orthographic Apple II keyboard layout, legends, relative positions | `LOCAL_WORKING_REFERENCE` | **YES** | **UNKNOWN** — do not publish/copy elsewhere merely because useful |
| `dev/drafts/logo example.png` | scullinsteel.com / Apple ][js (visual reference) | Badge layering / relief / proportion language only | `LOCAL_WORKING_REFERENCE` | **YES** | **UNKNOWN** — **do not** copy Apple ][js wording or Apple logo into ESP][ CAD |

Git tracking: follow current `dev/drafts` repository policy (do not change solely for this milestone).

ESP][ machine-readable derivatives (project-authored): `dimensions/keyboard-layout.json`, `keyboard-envelope.json`, `power-indicator.json`, `badge-carrier.json`. CAD: `cad/source/esp2_keyboard_reference.scad`.

---

## 8. Redistribution policy

| Asset class | In public Git? |
| --- | --- |
| Project-authored specs / JSON / OpenSCAD skeleton | Yes |
| Waveshare STEP / DWG / PDF / size JPG | Local under `reference/waveshare/` — **gitignored**; URL in this file |
| Third-party paddle/joystick photos | URL metadata only unless license clearly permits |
| Copyrighted Apple logo / Apple ][js badge artwork | Never as redistributable CAD art; reference images stay local |
| Project-authored keyboard/badge reference SCAD + JSON | Yes |

If redistribution is unclear: keep URL + metadata only.
