# Apple II paddle mechanical specification

Status vocabulary: `FIXED` `OFFICIAL` `PHYSICALLY_MEASURED` `DERIVED` `PROVISIONAL` `UNKNOWN` `PENDING_COMPONENT_SELECTION` `AUTHORITATIVE_PHYSICAL_REFERENCE_AVAILABLE`

## Intent — FIXED

Produce **1:1 original-size** Apple II–style paddles as separate accessories for ESP][.

They are **not** miniature accessories scaled to the ESP][ enclosure.

## Physical original — AUTHORITATIVE_PHYSICAL_REFERENCE_AVAILABLE

The project author owns **original Apple II paddles**.

| Use | Policy |
| --- | --- |
| Shell, knob, seam, screws, cable exit, internal layout | Physical paddles = **final dimensional validation** before printable release |
| Button geometry | **Do NOT** use the buttons on those physical paddles as original Apple button geometry — they were **historically modified** with different/better buttons |

Original button appearance must come from reliable photographs / documentation (see `../SOURCES.md` § paddles).

Until physical measurement: exterior reconstruction may proceed with **DERIVED / PROVISIONAL** internet references. Printable release still requires comparison to the author’s hardware.

## Product variants — FIXED architecture

| ID | Role |
| --- | --- |
| `ORIGINAL_REFERENCE` | Stable exterior reference matching original language |
| `ESP2_PADDLE_WIRED` | Future 2.5 mm TRRS; no battery |
| `ESP2_PADDLE_WIRELESS` | Mini ESP-family BLE + LiPo + charge + sensors — electronics `TO_BE_SELECTED` |

**Common exterior** (upper shell, lower shell, rotary knob, button, cable/reference opening). Prefer interchangeable internal carriers:

- `wired_internal_carrier`
- `wireless_internal_carrier`

Do **not** create two unrelated exterior models.

## Geometry

| Item | Value | Status |
| --- | --- | --- |
| Scale | 1:1 original | FIXED |
| Housing overall L×W×H | — | UNKNOWN (pending physical measure) |
| Knob diameter | ≈ 1.25 in (≈ 31.8 mm) | DERIVED / PROVISIONAL (contemporary review; Atari magazine era) |
| Original FIRE button diameter | ≈ 3/16 in (≈ 4.8 mm) | DERIVED / PROVISIONAL (documentation — **not** author’s modified buttons) |
| Potentiometer | historically ~150 kΩ linear | DOCUMENTED (Apple game-port era) / electronics PENDING |
| Pot shaft | — | PENDING_COMPONENT_SELECTION |
| Cable exit / seam / screws | — | UNKNOWN → PHYSICALLY_MEASURED later |

Machine-readable stub: `dimensions/paddle.json`.

## Photo research (curated — URL metadata only)

See `../SOURCES.md` §5. Do not dump hundreds of redundant images into Git. Prefer CC-licensed photo sets (e.g. popcorn.cx CC-BY-SA) when local mirrors are desired later.

## Electrical (high level)

| Net | Wired | Wireless | Status |
| --- | --- | --- | --- |
| Position | Analog pot or digital report | Sensor → BLE | UNASSIGNED |
| Button | Contact | Contact → MCU | UNASSIGNED |
| Power / GND | From host via TRRS (concept) | LiPo | PROPOSED / TO_BE_SELECTED |

Do **not** invent ADC/GPIO numbers — `../WIRING.md`.

## BOM hooks

Structured sections in `../BOM.md`: **PADDLE — COMMON / WIRED / WIRELESS**.
