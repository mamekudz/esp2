# USB storage & SD ownership

## One ownership state machine

| State | Who may access |
| --- | --- |
| `ESP2_OWNS_SD` / DEVICE | ESP][ mounts FAT; host has no MSC |
| `TRANSITION_TO_USB` | firmware closing FAT / Disk II |
| `USB_OWNS_SD` / HOST | USB MSC block I/O only |
| `TRANSITION_TO_ESP2` | remount FAT |
| `ERROR` | ownership uncertain |

**Hard invariant:** firmware and host OS must never have writable ownership at once.

Shared host tooling (`esp2-sd-ownership-session.mjs`) is used by:

- **SD card on computer** (`device:sd:computer`)
- **Back up SD card** (`device:sd:backup`)
- **Restore SD card** (`device:sd:restore`)

CLI helper `device:usb-storage` remains for scripts (not a dashboard task).

## USB stack (ESP32-S3)

| Flag | Role |
| --- | --- |
| `ARDUINO_USB_MODE=1` (default) | HW USB-Serial/JTAG — reliable flash/CDC; **MSC unavailable** |
| `ARDUINO_USB_MODE=0` | TinyUSB OTG — composite CDC + MSC intended |

Default firmware stays MODE=1 for development (keyboard bridge, Dial, flash).
MSC-backed Explorer/Finder access and auto-mount require a TinyUSB build.

Power-only USB adapters must **not** trigger HOST ownership. Auto mode uses
TinyUSB `tud_mounted()` (data host), never VBUS alone.

## Config: `usb.storageMode`

In `/esp2/config/system.json`:

```json
"usb": { "storageMode": "normal" }
```

| Value | Behavior |
| --- | --- |
| `normal` (default) | SD stays on ESP][ until user opens **SD card on computer** |
| `auto` | On real USB data host + MSC supported → DEVICE→HOST |

## Manual UX

**SD card on computer:** Make available → ENTER MSC → OS mounts → optional
Explorer/Finder. Return → eject first → LEAVE MSC → remount `/esp2`.

While HOST owns SD: Disk II / runtime SD access is unavailable.

## Backup / restore

Same ownership session. Logical `/esp2` tree only (not raw card images).
See `docs/architecture/sd-backup-restore.md`.

## Host platforms

| | Detection | Open folder | Safe eject |
| --- | --- | --- | --- |
| Windows | volume before/after MSC | Explorer | instruct user (automatic eject unreliable) |
| macOS | `/Volumes` before/after | Finder | `diskutil eject` when possible |
| Linux | best-effort | `xdg-open` | instruct user |
