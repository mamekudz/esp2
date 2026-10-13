# ESP][ Touch Control Center

Generic on-device menu for **Waveshare** and **diymore 2.8"** (one UI; panel
size from `ESP2_PANEL_WIDTH` / `ESP2_PANEL_HEIGHT`).

## Immediate entry

While Apple II video is shown and the Control Center is closed:

**touch-down anywhere on the active panel → open Control Center.**

No swipe, double-tap, long-press, or corner gesture. Default
`esp_bracket::SystemTouchMode::Menu`.

## Trigger consumption

`CcPointer` gate:

1. `Closed` + press → `WaitRelease` + `OpenMenu` (pause emu).
2. Same contact ignored for activation.
3. Release → `Interactive`.
4. Later press+release on the **same** hit activates.

Unused space does not close the menu.

## Pause / resume / throttle

| Event | Mechanism |
| --- | --- |
| Open | `g_emuHold = true`, neutralize paddles/buttons, `keyboard().reset()` |
| Close / Resume | `g_emuHold = false`, `resyncEmuWallClock()` |
| Not done | 6502 reset, disk reload, ESP restart, throttle catch-up |

BLE `esp2_ble::service()` keeps running while held so pairing in the menu works.

## Coordinate path

Physical sample (`esp2_touch::poll`) → `mapTouchToPanel` (identity for Control
Center; CC is drawn in panel-native pixels) → **visible UI viewport** (offset
into local coordinates) → `hitTest` / `ccHomeHit`.

Touches outside the configured system-UI viewport do not activate controls
while the menu is interactive. Opening the Control Center still accepts any
panel contact.

Screens must not reinterpret raw FocalTech coordinates independently of drawing.

## Physical panel vs system UI viewport

These rectangles stay distinct:

| Rectangle | Role |
| --- | --- |
| Physical display | Waveshare 280×456 or diymore 240×320 panel |
| Emulator viewport | Classic letterbox / landscape blit (`kView*` / `g_land*`) |
| System UI visible viewport | Where Control Center chrome is laid out and hit-tested |

The enclosure monitor opening will hide the **lower** AMOLED (drives / bezel)
and the upper rounded active corners. System UI therefore **must not** assume
the full LCD is usable.

`esp_bracket::ccComputeSystemUiViewport` resolves:

- `CcUiSurface::FullPanel` — development / no enclosure (`systemUiSurface`: `fullPanel`)
- `CcUiSurface::EnclosureVisible` — Waveshare 280×456 provisional upper window
  `(0,32,280,280)` (`PROVISIONAL_PHYSICAL_VERIFY`; not CAD-final)
- optional JSON override `uiViewportX/Y/W/H`

Default when JSON omits the key: enclosure-visible on 280×456, full panel on
diymore. Presentation mode (Classic / Landscape) is part of the layout context
and is refreshed when Display settings change; the enclosure window is physical
and does not rotate with Apple II landscape blit.

## Menu structure

Home tiles: Display, Disks, Input, Bluetooth, Storage, Settings, Reset / System.
Footer: Resume Apple II. Header Back on sub-screens.

| Section | Service |
| --- | --- |
| Display | `PresentMonitor` / `PresentEffect` / `PresentOrientation` via `applyPresentChange` |
| Disks | Drive 1 mount/eject (`mountDiskPath`, `ejectDrive`); Drive 2 disabled until firmware supports it |
| Input | First row is **Joystick test** (fits in the 4 visible list rows). Then keyboard/gamepad/pointer status. |
| Bluetooth | Same `esp2_ble` scan/pair/connect/disconnect/forget as µGulp (`ESP2_BLE_HID`). Hub **always** shows Add BLE device + Devices (disabled + `cc.err.ble_off` if the backend is unavailable). |
| Storage | `SdOwnership` + `enterUsbStorageMode` / `leaveUsbStorageMode` (no local MSC) |
| Settings | UI locale (`esp2_i18x`) |
| Reset | Apple II reset (keep disks); cold start (RAM clear + reset); ESP restart (confirm) |

Share-with-computer: confirmation, then the canonical DEVICE → TO_HOST → HOST
path. If MSC is unsupported (HW CDC), the action is disabled with
`cc.err.msc_unsupported`. While HOST owns the SD, disk browsing is disabled.

## Future emulated touch

`SystemTouchMode::FutureEmu` is reserved. Default remains Menu. Any future
Apple II pointer/joystick-from-touch mode must keep a system escape into this UI.

## Tests

Host: `host/test_cpp/test_control_center.cpp` (layout, gate, `mapTouchToPanel`).
Firmware integration is the display-task pointer path in `src/apple2_text/main.cpp`.
