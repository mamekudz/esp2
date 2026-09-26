# ESP][ enclosure mechanical specification

Status: **RESEARCH / FOUNDATION** — no final CAD in this milestone.

## 1. Product geometry language

ESP][ should read as **one miniature Apple II system**:

- computer
- monitor
- Disk II drive body/bodies

not as separate modern boxes bolted together.

## 2. Upper enclosure (BEIGE) — FIXED

The beige upper enclosure is **one coherent visible structure** containing:

- computer upper housing
- **integrated** drive bodies
- monitor housing

Monitor and drive bodies are **not** separate visible bolt-on boxes.

### Board mounting — FIXED

| Step | Intent |
| --- | --- |
| 1 | Insert Waveshare board **from below** |
| 2 | Guide into monitor/drive structure |
| 3 | Positive stop aligns AMOLED |
| 4 | Screws retain board to **upper** enclosure |

- Board is **not** primarily mounted to the bottom plate.
- Guides/stops establish alignment; screws only retain.
- Do **not** rely on screw-hole play to align the AMOLED.
- Remains serviceable.

## 3. Bottom plate (GRAY) — FIXED

- Separate printed part, screwed to beige upper.
- Provides service access.
- Must **not** be permanently glued.
- Removing the bottom must **not** disturb display/board alignment.

## 4. Three display geometries — CRITICAL

| Geometry | Meaning | Status |
| --- | --- | --- |
| AMOLED physical outline | Glass/cover outer shape (R3.7) | OFFICIAL — `board.json` |
| AMOLED active area | Lit region 22.34 × 36.06 mm, upper R2.50 | OFFICIAL — `board.json` |
| ESP][ visible monitor window | Front rectangular aperture | PROVISIONAL — CAD TBD |

**Rules:**

- They are **not** the same.
- OLED upper roundings **must not** be visible from the finished front.
- Bezel hides roundings while exposing the maximum practical **rectangular** active area.
- Do **not** simply copy the OLED outline into the monitor opening.
- Lower active area may be masked by drive/monitor structure (CLAUDE.md §7).

## 5. Print precision — FIXED capability

Fine FDM with **0.2 mm nozzle** is available.

Use appropriate detail for:

- keyboard legends (WHITE printable geometry)
- small inserts
- visual details

Fit/tolerance parameters remain **explicit** (`enclosure.json` → currently UNKNOWN/PROVISIONAL). Do not apply crude generic FDM clearances everywhere.

## 6. Keyboard — FIXED intent

| Item | Spec |
| --- | --- |
| Color | BROWN structural part |
| Structure | **All normal visible keycaps = one printed part** linked by hidden bridges/webs below the surface |
| Insertion | From **below** through beige key openings |
| Fixation | Screws and/or defined adhesive surfaces from below |
| Legends | WHITE printable geometry (0.2 mm nozzle); track min stroke / depth / alignment |

Do **not** model normal keys as dozens of individually assembled pieces.

## 7. POWER indicator — FIXED intent

- **Not** a key or switch; no movement.
- Separate **WHITE / TRANSLUCENT** insert with **BLACK** “POWER” lettering.
- Inserted from **above**, intentionally **recessed** below beige surface.
- Illuminated from below by white mini LED (`LED-PWR`).
- CAD must later provide: locating shoulder, insertion depth, hidden retention, light cavity, isolation.
- Real ON/OFF is the **rear** power switch (`PWR-01`).

## 8. Drives — FIXED intent

- Drive bodies integrated into beige upper.
- Visible **BLACK** drive fronts = separate printed inserts after print.
- Beige shell should provide shallow locating recesses.
- Drive 1 / Drive 2 **red** mini LEDs in black fronts (`LED-D1`, `LED-D2`) — software later maps emulated activity.

## 9. Ventilation — FIXED intent

Preserve Apple II ventilation-slot design language as **real openings** for:

- visual authenticity
- ventilation
- acoustic outlet for internal piezo/speaker

Do **not** add an obvious modern speaker grille unless later evidence requires it.

## 10. Rear I/O — FIXED intent

Approximately original Apple II rear power-connection area hosts:

| Port | Role |
| --- | --- |
| Physical ON/OFF switch | Real power control |
| 2.5 mm TRRS (4-pole) | Audio / controller — socket `PENDING_COMPONENT_SELECTION` |
| USB-C panel female | Chassis data + power + flash + CDC (+ future MSC) |

## 11. USB-C path — FIXED intent

```
Waveshare USB-C (internal)
  → 90° USB-C adapter (USB-01)
  → short USB-C data extension (USB-02)
  → chassis/panel USB-C female (USB-03)
  → rear wall
```

Requirements: USB 2.0 data, power, firmware flashing, CDC/serial, future MSC where possible.

Chassis socket absorbs plug force — **do not** mechanically load the Waveshare PCB connector.

## 12. microSD — FIXED intent

- May remain permanently inside.
- **No** external SD opening required by current design.
- Runtime media via serial upload; later USB MSC possible.

## 13. Internal lower cavity — FIXED intent

Contains:

- LiPo battery (retention, no sharp pressure, service access, cable clearance)
- internal piezo / small speaker
- wiring
- rear I/O components

Do **not** trap/compress the LiPo with screw bosses.

## 14. Apple badge / logo inserts — FIXED intent

- Separate **WHITE** printed carriers.
- Finish path: white print → laser-print film / logo print → transparent resin coating → glossy surface.
- Shallow locating recesses.
- **Do not** ship copyrighted logo artwork in generated CAD until redistribution is resolved — placeholder geometry only.

## 15. Print color groups

See `dimensions/enclosure.json` / README table (BEIGE / GRAY / BROWN / BLACK / WHITE / WHITE_TRANSLUCENT).

Main beige shell must **not** require complex multi-color printing.

## 16. CAD foundation rules

- Prefer parameterized B-Rep → STEP (+ Parasolid when tooling permits) → Plasticity.
- Generated CAD ≈ **80–90%** foundation; author refines aesthetics in Plasticity.
- Do **not** optimize around editing STL meshes as the primary workflow.
- Official Waveshare STEP lives under `reference/waveshare/.../esp32-s3-touch-amoled-1_64.stp` (reference body; outline dims from size drawing remain authoritative).
- Convenience copy / sync: `cad/reference-bodies/` + `tools/sync_reference_bodies.py` (binaries gitignored).

## 17. Mechanical coordinate system — FIXED directions

See `dimensions/coordinate-system.json`.

| Axis | Meaning |
| --- | --- |
| **+X** | Left → right when facing keyboard/monitor |
| **+Y** | Front → rear (toward rear I/O wall) |
| **+Z** | Bottom → top |

- **Display normal:** toward the user (−Y after board is mounted in the monitor).
- **Board insertion:** from below along **+Z** into the upper enclosure.
- Origin numeric lock remains **PROVISIONAL** until miniature scale is frozen; axis directions are FIXED.

## 18. Display / bezel parameters — no magic numbers

Machine-readable: `dimensions/display-geometry.json`.

Named parameters (mm):

| Parameter | Status |
| --- | --- |
| `oled_physical_width` / `oled_physical_height` | OFFICIAL |
| `oled_active_width` / `oled_active_height` | OFFICIAL |
| `oled_active_offset_x` / `oled_active_offset_y` | OFFICIAL |
| `oled_upper_radius` (R2.50) | OFFICIAL |
| `monitor_visible_width` / `monitor_visible_height` | **PROVISIONAL_CAD_CANDIDATE** |
| `bezel_overlap_{top,bottom,left,right}` | **PROVISIONAL_CAD_CANDIDATE** |

### Provisional maximum rectangular visible window

Method: inscribe a rectangle in the **active** area with top inset = `oled_upper_radius` (2.50 mm) so upper R2.50 corners are fully masked; full active width.

| Candidate | Value | Status |
| --- | --- | --- |
| Visible width | **22.34 mm** | PROVISIONAL_CAD_CANDIDATE |
| Visible height | **33.56 mm** (= 36.06 − 2.50) | PROVISIONAL_CAD_CANDIDATE |
| Offset from active (X) | 0 | PROVISIONAL_CAD_CANDIDATE |
| Offset from active (Y from top) | 2.50 mm | PROVISIONAL_CAD_CANDIDATE |
| Bezel overlap top (vs active) | 2.50 mm | PROVISIONAL_CAD_CANDIDATE |
| Bezel overlap L/R/B (vs active) | 0 | PROVISIONAL_CAD_CANDIDATE |

**Do not freeze** until project-author review. Additional bottom masking for drive structure is a separate choice (CLAUDE.md §7).

## 19. Board insertion study — FIXED intent (no final bosses yet)

Using official outline + STEP reference body:

| Item | Finding | Status |
| --- | --- | --- |
| Insertion envelope | Module 29.12 × 44.00 × 9.60 mm plus clearance; direction +Z from below | FIXED intent / envelope PROVISIONAL pad in skeleton |
| Alignment | Positive stop at display plane; M2 screws retain only | FIXED |
| Collision zones | Pin headers, USB-C, microSD, battery header, rear component height ~4 mm | OFFICIAL stackup + STEP AABB note |
| Screw access | Four M2 holes, 22.86 × 38.50 mm pattern | OFFICIAL |
| USB path keep-out | Placeholder only — adapter/socket P/N **PENDING_COMPONENT_SELECTION** | PROVISIONAL |

Skeleton: `cad/source/esp2_mechanical_skeleton.scad` + `assembly/README.md`.

## 20. STEP vs drawing — do not silent-fix

Programmatic inspection: `dimensions/step-inspection.json` / `tools/inspect_waveshare_step.py`.

- Vendor STEP is a **multi-body** reference (dozens of solids/shells).
- Raw Cartesian AABB **exceeds** the official module outline (headers / helpers).
- **Outline millimetres in `board.json` remain OFFICIAL** from the size drawing.
- Never rewrite the vendor STEP to force AABB == drawing.

## 21. Supersession note (CLAUDE.md)

Older CLAUDE.md §§28–30 sketches (magnetic removable monitor/drives, direct external Waveshare USB-C) are **superseded**. High-level permanent rules remain in CLAUDE.md §28–30 and point here.
