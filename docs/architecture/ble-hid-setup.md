# ESP][ — BLE HID setup & local control hotkeys

Status: **IMPLEMENTED** (apple2_text + µGulp `device:ble`). Physical verification
pending on real BLE keyboard / gamepad hardware.

## Purpose

Initial BLE keyboard/gamepad configuration is performed from the PC:

```text
PC → USB CDC → µGulp device:ble → ESP][ NimBLE host
       → scan / pair / manage → /esp2/config/bluetooth.json
```

After pairing, ESP][ can reconnect known devices **standalone** (USB power only,
no PC / µGulp).

## BlueShift transparency

ESP][ has **zero** BlueShift-specific input behavior. A device advertised as
`8BitDo … (BlueShift)` is an ordinary BLE HID peripheral. The `(BlueShift)`
suffix is display text only.

## Config path

| Path | Role |
| --- | --- |
| `/esp2/config/bluetooth.json` | Known devices + local control bindings |

Sibling of `system.json` / `macros.json`. Invalid/missing → safe defaults
(default hotkeys still usable; no known devices).

Tracked example: `config/device/bluetooth.example.json`.

## CDC protocol (`#ESP2BLE …`)

| Command | Effect |
| --- | --- |
| `#ESP2BLE INIT` | Init NimBLE host (bonding enabled) |
| `#ESP2BLE SCAN [sec]` | Bounded scan; emits `#BLE DEV …` lines |
| `#ESP2BLE STATUS` | Ready / connected / known counts |
| `#ESP2BLE LIST KNOWN` | `#BLE KNOWN …` lines |
| `#ESP2BLE PAIR <addr>` | Connect + bond + persist known metadata |
| `#ESP2BLE CONNECT <addr>` | Reconnect known/discovered |
| `#ESP2BLE DISCONNECT` | Drop active HID link |
| `#ESP2BLE FORGET <addr>` | Remove one known entry (+ disconnect if active) |
| `#ESP2BLE BINDINGS` | Human-readable hotkey summary |

BLE setup refuses when USB Storage / MSC is active (`#NAK BLE usb_storage_active`).

## Simultaneous connections

V1 budget: **one** active BLE HID host connection (`maxConn=1`). Multiple devices
may be *known*; auto-reconnect tries the first `autoReconnect` entry. Additional
simultaneous links are not claimed until measured on hardware.

## Local actions

| Action | Meaning |
| --- | --- |
| `RESET_APPLE_II` | Existing Apple II reset (disks remain) |
| `OPEN_SETTINGS` | V1: brief status hint + `#ACK SETTINGS OPEN` — **no** full OLED menu |

Future physical 5-way control must trigger the same `OPEN_SETTINGS` action.
`FUTURE_PHYSICAL_SETTINGS_CONTROL = CANDIDATE` — not implemented; no 3dprint/BOM change.

## Default bindings

| Action | Keyboard | Gamepad |
| --- | --- | --- |
| Reset | Ctrl+Alt+R | Start+B hold 1.5 s |
| Settings | Ctrl+Alt+S | Start+Select hold 1.5 s |

Bindings are evaluated **before** Apple II forwarding. Matching input is consumed
(partial chords / short presses do not fire). Malformed config falls back to these defaults.

Binding capture (“press to record”) is **future work** — µGulp edits human-readable text.

## µGulp

One Device-group task:

| ID | Display |
| --- | --- |
| `device:ble` | Set up BLE devices V\<version/\> |

Scan / pair / connect / disconnect / forget / status / hotkey labels live in **one**
native form — not separate `ble:scan` tasks.

## Normalized input

```text
BLE keyboard / gamepad  ─┐
Windows CDC bridge      ─┼→ LocalControlEngine → Apple II latch / GameIo
Future 5-way            ─┘
```

## Physical tests (required before README claim)

- [ ] USB + µGulp scan / pair BLE keyboard → Apple II keys
- [ ] Power-cycle standalone reconnect keyboard
- [ ] USB + µGulp scan / pair BLE gamepad → paddles/buttons
- [ ] Power-cycle standalone reconnect gamepad
- [ ] Keyboard / gamepad reset & settings hotkeys consumed (not forwarded)
- [ ] Optional: same device with `(BlueShift)` name uses identical path
