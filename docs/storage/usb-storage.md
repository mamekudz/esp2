# USB storage & serial media transfer

## Why exclusive ownership

The microSD uses a FAT filesystem. **Simultaneous read/write mounts from
Windows and ESP][ corrupt the volume.** ESP][ therefore implements an
explicit ownership state machine:

| State | Who may access |
| --- | --- |
| `ESP2_OWNS_SD` | ESP][ mounts FAT; Windows has no MSC |
| `TRANSITION_TO_USB` | FAT unmounting; no clients |
| `USB_OWNS_SD` | Windows MSC block I/O only; ESP][ must not open files |
| `TRANSITION_TO_ESP2` | MSC tearing down; remounting FAT |
| `ERROR` | Recovery required |

## USB stack audit (ESP32-S3 / pioarduino 55.03.32)

| Item | Finding |
| --- | --- |
| Peripheral | Native USB (USB-OTG device + USB-Serial/JTAG) |
| Arduino flag `ARDUINO_USB_MODE=1` | **HW USB-Serial/JTAG** — reliable `esptool` flash + CDC on COM5 |
| Arduino flag `ARDUINO_USB_MODE=0` | **TinyUSB OTG** — enables `USBMSC` + TinyUSB CDC composite |
| TinyUSB MSC | `CONFIG_TINYUSB_MSC_ENABLED=1` in framework sdkconfig; API `USBMSC` |
| Block I/O | SPI `SD` + `sdcard_unmount` then `sd_read_raw` / `sd_write_raw` |

### Composite CDC + MSC — board constraint

Preferred architecture is TinyUSB composite (CDC + MSC). On this Waveshare
board’s **single USB-C**:

1. Flashing `ARDUINO_USB_MODE=0` replaces the HW USB-Serial/JTAG application
   interface with TinyUSB.
2. Application TinyUSB CDC did **not** produce usable serial traffic in the
   first physical trial (0 bytes on COM6).
3. `esptool` could no longer connect for the next flash until the port
   reappeared as HW CDC (COM5) — recovery may require **BOOT** strap /
   power cycle.

**Default `apple2_text` therefore remains `ARDUINO_USB_MODE=1`.**

| Feature | Default build (`USB_MODE=1`) | Experimental OTG (`USB_MODE=0`) |
| --- | --- | --- |
| CDC diagnostics | **PASS** (HW) | TinyUSB CDC — needs further bring-up |
| `esptool` flash | **PASS** | Difficult / BOOT button |
| MSC mass storage | Code present; `UsbStorageMode::isSupported()==false` | Intended path |
| Serial media upload | **Supported** | Supported once CDC works |

Do **not** claim Windows MSC physically verified until an OTG build mounts
on Windows with clean eject + ESP remount.

## Serial media upload (development — primary path)

Binary framed protocol `ESPU` v1:

- magic `0x55505345`, version 1
- chunk default **2048** bytes (max 4096)
- SHA-256 end-to-end
- staging `<target>.upload` then rename/replace
- destinations only under `/esp2/`

```bash
# Do NOT use RTS reset on ESP32-S3 USB-Serial/JTAG — it enters DOWNLOAD mode.
node dev/tools/esp2-upload.mjs --port COM5 --file local.bin --target /esp2/roms/system.rom
gulp device:upload --port COM5 --file local.bin --target /esp2/roms/system.rom
node dev/tools/esp2-upload.mjs --port COM5 --verify /esp2/roms/system.rom
```

`--port` is **required** (no arbitrary COM auto-pick). Default is mid-run
upload (device already running). Optional `--reset` uses a mild DTR toggle only.

Physical evidence (project-owned test file, not a ROM):

| Field | Value |
| --- | --- |
| Target | `/esp2/diagnostics/upload-test.txt` |
| Size | 25 |
| SHA-256 | `c0db922924147f58f7001910265dadbf8df405aa5992610aa93842c12f356831` |
| Result | `ok: true` (~70 ms) |

## Enter USB Storage Mode (when supported)

```bash
gulp device:usb-storage --port COM5
gulp device:usb-storage --port COM5 --leave true
```

Serial: `#ESP2USBMSC` / `#ESP2USBMSC LEAVE`

On-device UI uses i18x keys `usb_storage.*` (en-US / de-DE).

## Canonical SD layout

```
/esp2/
  roms/
  disks/
  config/
  diagnostics/storage-manifest.json
```

## Physical verification status (this milestone)

| Check | Status |
| --- | --- |
| Serial uploader host tests | PASS |
| Serial uploader on device | requires flash of MODE=1 build after OTG experiment |
| Windows MSC mount | **NOT physically verified** (OTG CDC/flash conflict) |
| Local user ROM | `NO_LOCAL_ROM_FOUND` under `local/roms/` |
| F1 readiness | `F1_BLOCKED_NO_LOCAL_ROM` until `/esp2/roms/system.rom` exists |
