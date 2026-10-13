# Physical device resolver

Canonical module: `dev/tools/esp2-device-resolver.mjs`

COM numbers (`COM5`, `COM10`, …) are **runtime USB addresses**, not ESP][ identity.
Windows may change them after replug, reboot, USB mode change, or a different board.

## Inputs

1. **Project Control Target** (`platform`) — Waveshare vs diymore
2. **Current USB enumeration** — VID/PID (Espressif `303A:1001`), serial number, product/PNP strings
3. **Firmware handshake** — `#ESP2IDENT` → `#ACK IDENT product=ESP][ platform=… build=… msc=… usb=…`
4. **Optional hint** — form field or `ESP2_PORT`, **only if it still matches a live ESP][**

Do not persist COM as project/device identity.

## Selection

| Matches | Action |
| --- | --- |
| 0 ESP][ | Fail: `No compatible ESP][ device detected.` |
| 1 ESP][ | Auto-select |
| 2+ ESP][ | Do not guess — picker with identity labels |

BlueShift (or any board that does not answer `product=ESP][`) is **not** an ESP][ match, even on the same VID/PID.

## After USB transitions

Flash, reset, HOST↔DEVICE MSC, and CDC↔MSC re-enumeration **re-run** discovery (`reResolveEsp2DeviceAfterTransition`). The previous COM number is not assumed to return.

## µGulp tasks

Flash, upload, monitor, SD info/computer/backup/restore, BLE setup, input bridge, macros, device config apply, media sync, and diymore identify/display probe all call this resolver.
