# Apple II / II+ I/O page (`$C000–$C0FF`)

Canonical ESP][ decode reference for the **II / II+** profile
(`machine-profile.md`).

Status: **HOST_VERIFIED** (documented + host-tested) unless noted.

## Sources (technical facts; not copied manuals)

Primary / established:

- Apple II Reference Manual (I/O map, soft switches)
- Apple II Plus documentation (same built-in I/O family)
- Jim Sather, *Understanding the Apple II* (decode / floating bus notes)
- Beneath Apple DOS (Disk II slot context)

Secondary cross-checks (not sole authority):

- apple2js soft-switch handling
- EWM / other MIT Apple II emulators

Disagreements are noted below rather than silently copying one emulator.

## Classification legend

| Tag | Meaning |
| --- | --- |
| IMPLEMENTED | Host model with tests |
| RESERVED | Decoded / ignored safely |
| FLOATING_BUS | Should track video data; see approximation |
| SLOT_IO | Routed to `SlotDevice` |
| DEVICE_SPECIFIC | Built-in peripheral state |
| NOT_APPLICABLE_TO_II_PLUS | IIe/GS-only |
| FUTURE | Architecture reserved |

## Compact map

| Range | Function | Read | Write | Mirrors / decode | Status | Tested | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `$C000–$C00F` | Keyboard data latch | data (bit7=strobe) | no side effect | A3–A0 ignored | IMPLEMENTED | yes | |
| `$C010–$C01F` | Clear keyboard strobe | clear + return data | clear | A3–A0 ignored | IMPLEMENTED | yes | |
| `$C020–$C02F` | Cassette output toggle | toggle | toggle | A3–A0 ignored | IMPLEMENTED | yes | Logical only; no physical cassette |
| `$C030–$C03F` | Speaker toggle | toggle | toggle | A3–A0 ignored | IMPLEMENTED | yes | One toggle per bus access |
| `$C040–$C04F` | Utility strobe | pulse flag | pulse flag | A3–A0 ignored | IMPLEMENTED | yes | No external pin yet |
| `$C050` | Graphics (TEXT off) | set | set | exact | IMPLEMENTED | yes | Any access |
| `$C051` | Text | set | set | exact | IMPLEMENTED | yes | |
| `$C052` | Full screen (MIXED off) | set | set | exact | IMPLEMENTED | yes | |
| `$C053` | Mixed | set | set | exact | IMPLEMENTED | yes | |
| `$C054` | Page1 | set | set | exact | IMPLEMENTED | yes | Does not erase VRAM |
| `$C055` | Page2 | set | set | exact | IMPLEMENTED | yes | |
| `$C056` | LoRes (HIRES off) | set | set | exact | IMPLEMENTED | yes | |
| `$C057` | HiRes | set | set | exact | IMPLEMENTED | yes | |
| `$C058` | AN0 off | set | set | exact | IMPLEMENTED | yes | |
| `$C059` | AN0 on | set | set | exact | IMPLEMENTED | yes | |
| `$C05A` | AN1 off | set | set | exact | IMPLEMENTED | yes | |
| `$C05B` | AN1 on | set | set | exact | IMPLEMENTED | yes | |
| `$C05C` | AN2 off | set | set | exact | IMPLEMENTED | yes | |
| `$C05D` | AN2 on | set | set | exact | IMPLEMENTED | yes | |
| `$C05E` | AN3 off | set | set | exact | IMPLEMENTED | yes | |
| `$C05F` | AN3 on | set | set | exact | IMPLEMENTED | yes | |
| `$C060/$C068` | Cassette in / PB3 | bit7 sense | — | low 3 bits + A3 | IMPLEMENTED | yes | II: cassette in on `$C060` |
| `$C061/$C069` | PB0 | bit7 pressed | — | | IMPLEMENTED | yes | |
| `$C062/$C06A` | PB1 | bit7 pressed | — | | IMPLEMENTED | yes | |
| `$C063/$C06B` | PB2 | bit7 pressed | — | | IMPLEMENTED | yes | |
| `$C064/$C06C` | PDL0 timer | bit7 active | — | | IMPLEMENTED | yes | Cycle model |
| `$C065/$C06D` | PDL1 | bit7 active | — | | IMPLEMENTED | yes | |
| `$C066/$C06E` | PDL2 | bit7 active | — | | IMPLEMENTED | yes | |
| `$C067/$C06F` | PDL3 | bit7 active | — | | IMPLEMENTED | yes | |
| `$C070–$C07F` | Paddle trigger | start timers | start timers | A3–A0 ignored | IMPLEMENTED | yes | |
| `$C080–$C08F` | Slot 0 (Language Card) | slot | slot | SLOT_IO | FUTURE/DEFERRED | route | Banking not modeled |
| `$C090–$C09F` | Slot 1 | slot | slot | SLOT_IO | IMPLEMENTED | route | Empty default |
| `$C0A0–$C0AF` | Slot 2 | slot | slot | SLOT_IO | IMPLEMENTED | route | |
| `$C0B0–$C0BF` | Slot 3 | slot | slot | SLOT_IO | IMPLEMENTED | route | |
| `$C0C0–$C0CF` | Slot 4 | slot | slot | SLOT_IO | IMPLEMENTED | route | |
| `$C0D0–$C0DF` | Slot 5 | slot | slot | SLOT_IO | IMPLEMENTED | route | |
| `$C0E0–$C0EF` | Slot 6 (Disk II) | slot | slot | SLOT_IO | IMPLEMENTED | route | Controller DEFERRED |
| `$C0F0–$C0FF` | Slot 7 | slot | slot | SLOT_IO | IMPLEMENTED | route | |

### Beyond `$C0FF` (related)

| Range | Role | Status |
| --- | --- | --- |
| `$C100–$C7FF` | Slot ROM `$Cn00–$CnFF` | Boundary + synthetic fixture |
| `$C800–$CFFF` | Expansion ROM window | Boundary / selection latch stub |
| Unimplemented built-in reads | Floating bus | **APPROXIMATE** (`floatingBusApprox_`) |

## Floating bus

Real II video circuitry can present display data on undriven reads
(*Understanding the Apple II*). ESP][ currently returns a host-settable
approximation byte (default `0xFF`) tagged **APPROXIMATE** — not full
scanline-derived floating bus. TODO: video-derived floating bus later.

## IIe differences (do not implement in II+ profile)

- Soft-switch **status reads** in `$C01x` (e.g. reading TEXT/MIXED state)
- 80-column / aux-memory switches in `$C000–$C00F` write side
- Double-hires / AN3 interaction for DHR

## Peek vs read

| API | Side effects |
| --- | --- |
| `read` / `write` | Full hardware semantics |
| `peek` | **No** speaker toggle, strobe clear, paddle trigger, cassette toggle |

Diagnostics / memory viewers must use `peek`.

## Cross-check notes

| Topic | Docs | apple2js / EWM | ESP][ |
| --- | --- | --- | --- |
| Kbd/spkr 16-byte mirrors | yes | typically yes | matches |
| Video `$C050–$C057` any access | yes | yes | matches |
| Paddle timing | RC discharge | implementations vary | cycle threshold model |
| Language Card | complex | varies | DEFERRED — see `language-card.md` |
| Floating bus | video-derived | often approximated | APPROXIMATE |
| `$C800` expansion select | Cx ROM access | yes | latch stub HOST_VERIFIED |

## Related docs

- `machine-profile.md` — II/II+ target
- `language-card.md` — Slot 0 banking (DEFERRED)
- `disk2-boundary.md` — Slot 6 Disk II (DEFERRED)
- `boot-readiness.md` / `boot-readiness.json` — gate levels
