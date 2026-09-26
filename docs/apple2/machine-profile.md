# ESP][ machine profile

Status labels: **HOST_VERIFIED** / **APPROXIMATE** / **DEFERRED** / **NOT_IMPLEMENTED**

## Initial target

**Apple II / Apple II+ compatible** baseline (48 KiB RAM + 12 KiB ROM map
`$D000–$FFFF`).

This is the V1 soft-switch and I/O decode target for ESP][.

## Explicitly out of scope for this profile

| Feature | Machine | ESP][ status |
| --- | --- | --- |
| 80-column / SoftSwitch `$C00x` video status reads | IIe | NOT_IMPLEMENTED |
| Auxiliary memory / ALTZP / 80STORE / PAGE2 aux | IIe | NOT_IMPLEMENTED |
| Double Hi-Res | IIe | NOT_IMPLEMENTED |
| Built-in mouse / SmartPort | //c / GS | NOT_IMPLEMENTED |
| Language Card banking (`$C080–$C08F` full model) | II+ option | DEFERRED (not required for Level-3 Applesoft-in-ROM) |
| Disk II controller | Slot 6 card | HOST_VERIFIED (synthetic); WOZ/RWTS DEFERRED |

Where II/II+ and IIe differ, ESP][ implements **II/II+** behavior unless a
future `MachineProfile::AppleIIe` is selected explicitly.

## Memory map (II+)

| Range | Role |
| --- | --- |
| `$0000–$BFFF` | 48 KiB RAM |
| `$C000–$C0FF` | Built-in + slot soft switches (see `io-page.md`) |
| `$C100–$C7FF` | Slot ROM windows (`$Cn00–$CnFF`) |
| `$C800–$CFFF` | Expansion ROM (shared; selection latch) — boundary only |
| `$D000–$FFFF` | Motherboard ROM (12 KiB) |

## Reset semantics

| Action | Effect |
| --- | --- |
| CPU reset | 6502 reset vector only |
| Apple II reset | Soft switches / keyboard / speaker / game I/O to documented reset; **disks stay mounted**; ROM stays loaded |
| ESP32 restart | Full firmware reboot (not this host milestone) |

## Boot-readiness levels

| Level | Meaning |
| --- | --- |
| 0 | Synthetic CPU tests only |
| 1 | Synthetic ESP][ test ROM |
| 2 | Disk II synthetic boot |
| 3 | User-supplied II/II+ ROM + interactive text |
| 4 | Broader disk authenticity |
| 5 | Target games boot/run |

Current level: see `docs/apple2/boot-readiness.md`.
