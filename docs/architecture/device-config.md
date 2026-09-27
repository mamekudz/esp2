# ESP][ — persistent device configuration & macros

Status: **IMPLEMENTED** (apple2_text firmware).

## Purpose

Allow a configured ESP][ to boot standalone (USB power only — no PC):

- load system ROM path
- mount configured disk image(s)
- optional Autostart disk boot
- presentation (classic/landscape, sharp/artifact)
- screensaver idle timeout
- optional startup input macro

Galaxian (or any title) is a **user profile on the SD card**, not compiled-in firmware behavior.

## Paths (device SD)

| Path | Role |
| --- | --- |
| `/esp2/config/system.json` | Canonical device configuration |
| `/esp2/config/macros.json` | Named input macros |

Tracked examples (host repo, not auto-copied):

- `config/device/system.example.json`
- `config/device/macros.example.json`
- `config/device/profiles/galaxian-demo/` — demonstration profile

## Schema (`system.json`)

`schemaVersion` must be `1`. Nested objects; unknown keys ignored by the parser.

| Field | Meaning |
| --- | --- |
| `machine.rom` | Absolute `/esp2/...` ROM path |
| `media.drive1` | Disk image path or `null` |
| `media.drive2` | Reserved (path validated; V1 mounts drive1 only) |
| `startup.bootFromDisk` | If true, Autostart Slot-6 boot after mount |
| `startup.macro` | Macro id from `macros.json`, or `""` |
| `presentation.orientation` | `classic` \| `landscape` |
| `presentation.color` | `sharp` \| `artifact` |
| `display.screensaverSeconds` | `0` = off; else idle seconds before AMOLED screensaver |

Invalid / missing config → concise `[CONFIG][FAIL]` log → **safe defaults** (historical interactive ROM bring-up, no auto disk). No reboot loop.

## Macros

V1 actions:

| `op` | Fields | Behavior |
| --- | --- | --- |
| `wait` | `ms` | Wall-clock wait while 6502 continues |
| `waitHires` | `ms` (budget) | Wait until graphics+HGR soft-switches (generic) |
| `key` | `code` | Apple II latch via same path as live input (`A`, `RETURN`, hex `41`, …) |

Bounds: max 32 actions/macro, max 180s single wait, max 300s total wait. Failure stops the macro; emulation continues.

Startup: after config mount/boot, the named startup macro is armed.

Manual: serial `#ESP2MACRO RUN <id>` (same engine).

## Screensaver activity

`notifyActivity` is for **user** interaction only:

- keyboard / pad / buttons
- touch

Not reset by: CPU, VRAM, disk spin, presentation changes, title heuristics.

While the panel sleeps, Apple II emulation continues.

## µGulp

| Task | Role |
| --- | --- |
| `device:config` | Form → write profile → upload `system.json` + `macros.json` |
| `device:macro:run` | Run a macro on a live device |

## Serial diagnostics

```
#ESP2CONFIG STATUS
#ESP2MACRO RUN galaxian-start
#ESP2MACRO STATUS
#ESP2MACRO STOP
```

## Pre-publication / demo notes

Do not commit ROM/disk bytes. Configuration stores paths only.

Future input-provider selection (Windows bridge / USB HID / BlueShift / paddles) may extend `system.json` — not implemented here.
