# ESP][ — BLE HID setup & local control hotkeys

Status: **IMPLEMENTED** (apple2_text + µGulp `device:ble`). Physical verification
of a composite BLE keyboard + integrated pad is **blocked** when the device is
already bonded to Windows as HID host (see Physical tests).

## Purpose

BLE keyboard/gamepad configuration works **on-device** (primary) or from the PC
(development / µGulp):

```text
Touch / hotkey → Settings → Input devices → Add BLE device / Devices
       → NimBLE scan / pair / reconnect → /esp2/config/bluetooth.json

PC → USB CDC → µGulp device:ble → same NimBLE host + bluetooth.json
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
| `#ESP2BLE HIDMON [1\|0]` | Rate-limited `#BLE HID …` report monitor |
| `#ESP2BLE PADMODE joystick\|paddle\|off` | Integrated pointer → Apple II mapping |
| `#ESP2BLE SENS <1..32>` | Relative-pad accumulation sensitivity |
| `#ESP2BLE RECENTER` | Recenter virtual absolute X/Y |
| `#ESP2BLE PADSTAT` | Normalized pad snapshot |

BLE setup refuses when USB Storage / MSC is active (`#NAK BLE usb_storage_active`).

Physical audit helper: `node dev/tools/ble-hid-physical-audit.mjs COM5`.

## Composite HID (keyboard + integrated pad)

One BLE link may expose **multiple HID report shapes** (boot keyboard + relative
mouse/touchpad and/or gamepad). ESP][ classifies **each report**:

| Shape | Heuristic | Path |
| --- | --- | --- |
| Boot keyboard | len 8–10, reserved byte 0 | → `LocalControlEngine` → Apple II latch |
| Relative pointer | short report, buttons + dx/dy | → virtual axes → `GamepadMapper` → GameIo |
| Gamepad | remaining when caps include gamepad/joystick | → `GamepadMapper` → GameIo |

Relative pads do **not** pretend to be absolute joystick coordinates until
accumulated into a virtual axis (`RECENTER`, `SENS`). Absolute/gamepad axes use
the existing mapper + deadzone when those reports are present.

Do **not** put vendor/model checks in `AppleIIMachine` — quirks stay in BLE HID /
profile layers if ever required.

## Simultaneous connections

V1 budget: **two** active BLE HID host connections (`maxConn=2`), e.g. keyboard +
gamepad at once. Auto-reconnect fills free slots from `autoReconnect` known
devices. A composite keyboard+pad still uses **one** connection with multi-report
routing.

## Local actions

| Action | Meaning |
| --- | --- |
| `RESET_APPLE_II` | Existing Apple II reset (disks remain) |
| `OPEN_SETTINGS` | Opens on-device **Settings → Input devices** (BLE scan/pair/list) |

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
BLE keyboard / gamepad / composite pad  ─┐
Windows CDC bridge                      ─┼→ LocalControlEngine → Apple II latch / GameIo
Future 5-way                            ─┘
```

## On-device menu path

```text
Settings (Ctrl+Alt+S or Start+Select hold)
  → Input devices
      → Add BLE device   (bounded async scan, tap to pair)
      → Devices          (known list, connect / disconnect / forget / info)
```

User-facing strings use **i18x** (`settings.*`, `ble.*`). BLE callbacks update
state only; **esp2_settings_ui** owns drawing (display task).

## Physical tests (required before README claim)

- [ ] On-device Add BLE device → pair → Devices list
- [ ] USB + µGulp scan / pair BLE keyboard → Apple II keys
- [ ] Power-cycle standalone reconnect keyboard
- [ ] Integrated pad HID type measured (mouse rel / abs / gamepad / other)
- [ ] Pad usable as joystick and/or paddle with physical Apple II verification
- [ ] Power-cycle standalone reconnect composite device
- [ ] Keyboard / gamepad reset & settings hotkeys consumed (not forwarded)
- [ ] Optional: same device with `(BlueShift)` name uses identical path

**2026-09-29 physical session notes:** ESP][ `apple2_text` BLE host ready on COM5.
Nearby advertisements were only smart-home LE devices (Govee, …). Windows PnP
lists **MX Dialpad** (`DB:EE:9E:2E:60:FC`, BTHLE) as a candidate keyboard/pad,
but it did **not** appear in ESP][ scan and direct `#ESP2BLE PAIR` returned
`#NAK BLE PAIR` while Windows still held the BLE HID association. Release the
Windows bond / enter pairing mode, then re-run `ble-hid-physical-audit.mjs`.
