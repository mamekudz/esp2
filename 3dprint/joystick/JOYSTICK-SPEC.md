# Apple II joystick mechanical specification

Status vocabulary: `FIXED` `OFFICIAL` `PHYSICALLY_MEASURED` `DERIVED` `PROVISIONAL` `UNKNOWN` `PENDING_COMPONENT_SELECTION` `AUTHORITATIVE_PHYSICAL_REFERENCE_AVAILABLE`

## Intent — FIXED

**1:1 original-size** Apple II–style joystick accessory for ESP][. Exterior remains substantially original.

## Physical original — AUTHORITATIVE_PHYSICAL_REFERENCE_AVAILABLE

The project author owns an **original Apple II joystick**. It will provide final dimensional validation before printable release. Internet photos/docs may establish provisional exterior research now.

## Product variants — FIXED architecture

| ID | Role |
| --- | --- |
| `ORIGINAL_REFERENCE` | Exterior / feel reference |
| `ESP2_JOYSTICK_WIRED` | Wired interface (architecture not frozen) |
| `ESP2_JOYSTICK_WIRELESS` | BLE + LiPo — electronics `TO_BE_SELECTED` |

## Geometry (provisional community measurements)

Community report for Apple Joystick //e **A2M2002** (Applefritter, 2022) — **DERIVED / PROVISIONAL** until author measures:

| Item | Value | Status |
| --- | --- | --- |
| Footprint (widest) | 83.5 × 83.5 mm | DERIVED / PROVISIONAL |
| Body height (button deck) | 43.4 mm | DERIVED / PROVISIONAL |
| Body height (raised stick square) | 54.4 mm | DERIVED / PROVISIONAL |
| Raised square base | 62.1 × 62.1 mm | DERIVED / PROVISIONAL |
| Raised square top | 58.6 × 58.6 mm | DERIVED / PROVISIONAL |
| Orange square buttons | 12.6 × 12.6 mm | DERIVED / PROVISIONAL |

Machine-readable: `dimensions/joystick.json`.

## Position sensing — FIXED DESIGN INTENT

Do **NOT** use the original centering mechanism as the position sensor.

Use **electronic / contactless X/Y position sensing** (investigate Hall / magnetic). Goals: continuous, absolute, low-friction, low-wear.

## Centering — FIXED DESIGN INTENT

Mechanically effective springs provide centering **force only**. Engagement is **electronically switchable**:

| Mode | X spring | Y spring |
| --- | --- | --- |
| 1 | ON | ON |
| 2 | OFF | ON |
| 3 | ON | OFF |
| 4 | OFF | OFF |

Springs do **not** determine position.

### Original centering as inspiration

Apple Joystick //e manuals document **mechanical** per-axis centering enable/disable via underside key marks / cams, with coil springs on half-circle actuators (see Applefritter teardown notes). ESP][ deliberately keeps that **feel language** but moves engagement to an **electronic actuator**.

### Actuation candidates (research — not selected)

Prefer energy mainly while **switching**, not continuous hold (especially wireless):

| Candidate | Notes | Status |
| --- | --- | --- |
| Bistable magnetic latch | Pulse to flip | CANDIDATE |
| Miniature servo + cam | Familiar; idle current if holding | CANDIDATE |
| Mechanical bistable latch | Manual or motor-armed | CANDIDATE |
| Miniature solenoid | Often needs hold current unless latching | CANDIDATE |

**Do not select final mechanism yet.**

## Wired interface — open architecture

Because wired mode may need X, Y, button(s), centering control, power, ground:

| Option | Idea | Status |
| --- | --- | --- |
| A | Passive analog pots + switches on multi-pole connector | LEGACY-COMPAT research |
| B | Passive analog TRRS — likely **insufficient** for centering control + power | PROBABLY INSUFFICIENT |
| C | Small intelligent controller in stick; **power + bidirectional digital data over TRRS** (or other slim connector) | PREFERRED CONCEPT (not frozen) |

Do **not** freeze pinout / protocol yet — document in `../WIRING.md`.

## Electrical summary

| Net | Role | Status |
| --- | --- | --- |
| X / Y position | Contactless sensor → MCU | PENDING_COMPONENT_SELECTION |
| Buttons | Digital | UNASSIGNED |
| Centering actuators | Switchable spring engage | PENDING_COMPONENT_SELECTION |
| Host link (wired) | TBD digital or hybrid | UNASSIGNED |
| Host link (wireless) | BLE HID / custom | TO_BE_SELECTED |

No GPIO invention on the main ESP][ board for these until accessory architecture freezes.

## CAD workflow

Specification → parameterized B-Rep → STEP/Parasolid → Plasticity → `printable/`.  
**This milestone:** research + dimensions only — **no final joystick CAD**.
