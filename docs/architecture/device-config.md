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
| `presentation.monitor` | `white` \| `green` \| `amber` \| `artifact` |
| `presentation.effect` | `clean` \| `crt` |
| `presentation.color` | Legacy alias (`sharp`/`artifact`/…) — prefer `monitor` |
| `display.screensaverSeconds` | `0` = off; else idle seconds before AMOLED screensaver |
| `usb.storageMode` | `normal` (default) \| `auto` — auto enters USB MSC only on a real USB **data** host when TinyUSB MSC is available; never on power-only |
| `input.hostBridge.*` | **Host-only** Windows CDC bridge (firmware ignores) |

### `input.hostBridge` (host tooling)

Stored in the same profile `system.json` so `device:config` / `device:input-bridge`
share one selection. ESP32 firmware does not read these fields.

| Field | Meaning |
| --- | --- |
| `gamepad` | `none` \| `auto` \| `0`…`3` (XInput user index) |
| `gamepadId` | Optional name/id regex (overrides slot when set) |
| `pdl0` | `auto` \| `gamepadX` \| `dial` \| `none` |
| `pdl1` | `auto` \| `gamepadY` \| `none` |
| `pb0` | `auto` \| `gamepadA` \| `none` (legacy `dialPress`/`or` → `gamepadA`; MX Dial has no press) |
| `dial` | Enable Logitech Dial Raw Input path |
| `keyboard` | Forward host TTY keys (default true) |
| `deadzone` | Stick deadzone 0…0.5 (default 0.08) |

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
| `device:input-bridge` | Windows CDC bridge using profile `input.hostBridge` |
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

`input.hostBridge` selects the Windows development gamepad/Dial mapping per
profile. Native USB HID / BlueShift / TRRS providers remain future work
(see `docs/architecture/input-providers.md`).
