# Input providers (ESP][)

Transport-independent normalized Apple II input:

```text
Apple II normalized input
        ▲
        │
┌───────┼───────────┬──────────────┐
│       │           │              │
Windows   USB HID    BlueShift       TRRS
bridge    native     BLE bridge      controllers
(dev)     future     future          future
```

The Apple II core (`Apple2Bus` keyboard latch / `GameIo` paddles & PB0–PB2)
must not care which provider supplied the state.

## Current development provider

| Provider | Status | Transport |
| --- | --- | --- |
| Windows CDC input bridge | **DEV** | USB CDC text lines `#ESP2INPUT …` |
| Serial media upload | **DEV** | USB CDC binary ESPU (exclusive MEDIA session) |
| Logitech Dial (relative) | **DEV** | Windows Raw Input mouse-wheel channel → virtual absolute PDL0 |

Host tool: `dev/tools/esp2-input-bridge.mjs` (+ `esp2-paddle-accumulator.mjs`).
µGulp: `device:input-bridge` (reads `input.hostBridge` from the chosen profile
or `local/device/config/system.json`).

Multi-device composition stays on the **host** bridge (e.g. PDL0←Dial,
PDL1←8BitDo, PB0←gamepad A). The Apple II core only sees
normalized paddles/buttons.

**Profile gamepad selection:** `system.json` → `input.hostBridge.gamepad`
(`none` / `auto` / XInput slot `0`…`3`) plus optional `gamepadId` regex.
Edit via `device:config`. Firmware ignores the `input` object.

Dial: relative deltas accumulate into 0…255 (default center 128, sensitivity 2,
clamp at ends — no wraparound).

**Logitech MX Dial has no physical press switch** (Logitech hardware design).
Do **not** map PB0 to a Dial “click”. Use gamepad A / keyboard / a real paddle
button for PB0. Rotation-only for PDL0.

**Logitech Options+:** If Dial rotation only changes Windows volume, Options+
is intercepting the HID path. Disable Dial→volume / Smart Actions (or map the
crown to mouse wheel) so Raw Input can see wheel/HWHEEL deltas.

**Keyboard:** CDC keyboard injection requires the bridge terminal to have
focus (Node raw stdin). Gamepad (XInput) and Dial (Raw Input) do not.

CDC roles on one serial stream (mutually exclusive sessions):

| Role | Framing |
| --- | --- |
| `MEDIA_TRANSFER` | `#ESP2UPLOAD` → ESPU frames → `#ESP2UPLOAD DONE` |
| `LIVE_INPUT` | `#ESP2INPUT LIVE` … KEY/PAD … `#ESP2INPUT IDLE` |
| `IDLE` | no host→device input traffic |
| Diagnostics | device→host `[TAG] …` — host must ignore for ACKs |

Live gamepad/keyboard traffic must not run during an active MEDIA session.

## Normalized model

- Keyboard → `$C000` latch / `$C010` strobe (`Keyboard::keyDown`)
- Paddles → `GameIo::setPaddle(0/1)` (timing sense via `$C070`)
- Pushbuttons → `GameIo::setButton(0..2)` (PB0–PB2)

No title-specific mappings in the emulator core.

## Future: `Esp32UsbHidInputProvider`

ESP32-S3 can act as USB **host** (OTG) for:

- USB keyboard
- USB gamepad
- USB hub

**Not implemented in this milestone.**

```text
NATIVE_USB_HID_HOST = FUTURE / REQUIRES_HARDWARE_VALIDATION
```

### USB-C role conflict (design note)

The same physical ESP][ USB-C path currently serves **device** roles for
development:

- flashing
- CDC (upload + input bridge)
- optional MSC

Native keyboard/gamepad attachment requires ESP][ as USB **host**, which
implies unresolved hardware questions:

- Device vs Host role
- VBUS sourcing
- USB-C CC / role switching
- Waveshare board OTG capability
- rear panel extension / adapter CC wiring
- hub possibility

Do **not** assume the rear USB-C concept automatically supports both PC
connection and USB HID peripherals without validation.

A charge-only rear extension is unacceptable (see `3dprint/BOM.md` USB-01…03).
