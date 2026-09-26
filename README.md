# ESP][

**English** | [Deutsch](README.de-DE.md)

<p align="center">
  <a href="https://microgulp.dev/ready/">
    <img src="docs/assets/microgulp-ready.png" alt="µGulp Ready" width="110">
  </a>
</p>

> **🚧 Work in Progress**
>
> This project is under active development.
> Hardware, firmware, APIs, documentation and compatibility may change.

**ESP][** is an experimental hobby / open-source project: a miniature, self-contained **Apple II emulator** on the **Waveshare ESP32-S3-Touch-AMOLED-1.64** board.

Package / repo identifier (ASCII): `esp2` — the visible project name remains **ESP][**.

### Important status note

- ESP][ is **not finished**.
- The **Apple II emulator is not complete** and is **not yet** integrated into the ESP32 firmware.
- Verified **Phase-1 hardware** (display, touch, microSD, …) does **not** mean the full emulator runs on the device.
- Host-tested emulator components (`HOST_VERIFIED`) must be distinguished from **ESP32-tested** parts.
- Controller / media compatibility and Disk II remain under development.

---

## Current status

Keep these levels distinct:

| Level | Meaning |
| --- | --- |
| **VERIFIED** | Confirmed on the physical board |
| **HOST_VERIFIED** | Tested on the development host, not on the ESP32 |
| **IN DEVELOPMENT** | Partially present; verification incomplete |
| **PLANNED** | Architecture / goal; not built yet |

### VERIFIED (physical board)

**DISPLAY**

- CO5300 AMOLED initialized
- Resolution **280 × 456**
- Visually verified (test pattern / orientation)
- Display power: ACTIVE → SCREENSAVER → OFF (CO5300 `displayOff` / `displayOn`)

**TOUCH**

- FT3168 on shared I2C
- SDA **GPIO47**, SCL **GPIO48**
- Touch coordinate mapping verified

**microSD**

- SPI: CS **GPIO38**, MOSI **GPIO39**, MISO **GPIO40**, SCLK **GPIO41**
- SDHC / FAT verified
- 32 GB card tested
- Read / write / persistence verified

**IMU (QMI8658)**

- Detected at I2C address **0x6B**
- WHO_AM_I **0x05**, revision **0x7C**
- Live accelerometer / gyro verified
- Physical axis mapping still pending

**Build / diagnostics**

- PlatformIO firmware builds and flashes
- Serial `[TAG]` diagnostics for bring-up

### HOST_VERIFIED (host tooling / host machine only)

- Media import / catalog / provenance gates (`media:import`, apple2js catalog)
- Host-side Apple II machine: **fake6502**, bus, soft switches, text / LoRes / HGR, artifact color, Disk II (Level-4 clean-room boot path), user-ROM loader (`npm run test:apple2`)
- Synthetic test ROM / no Apple ROMs in the repository (optional local user ROM via `local/roms/`)

### PLANNED

Everything under [Planned features](#planned-features) — including ESP32 emulator integration, Disk II, Bluetooth input / audio, and the Control Screen.

---

## Hardware

Target platform:

| | |
| --- | --- |
| Board | Waveshare ESP32-S3-Touch-AMOLED-1.64 |
| MCU | ESP32-S3 |
| Flash / PSRAM | 16 MB Flash, 8 MB PSRAM |
| Display | 1.64″ AMOLED, **280 × 456**, controller **CO5300** |
| Touch | **FT3168** |
| Storage | microSD |
| Radio | Wi-Fi, Bluetooth LE |
| IMU | **QMI8658** (when populated) |
| Build | **PlatformIO** (not an Arduino IDE primary project) |

Planned additional hardware: piezo / local speaker, physical reset / control button, Bluetooth keyboard / gamepad / headphones, later wired analog paddles.

---

## Planned features

### Apple II emulation

Host-side machine + **fake6502** (CC0) under `third_party/fake6502/` — **HOST_VERIFIED** (boot readiness Level 4: realistic Slot-6 Disk II boot path; optional user-supplied II/II+ and Slot-6 ROMs). Firmware integration on the ESP32: **not yet**. No Apple ROMs in the repo (synthetic / clean-room test firmware only). See `docs/apple2/roms.md` and `docs/apple2/boot-readiness.md`.

### Virtual Disk II / media layer

Separated from the core: Disk II controller → virtual disk → Drive 1/2 → image on microSD. Host-side standard Disk II boot path implemented and regression-tested (DSK/DO, PO, NIB fixtures; WOZ / full write path **PLANNED**). No embedded commercial disk images in the repository.

### Touch UI

Primarily Control Screen, library, disk management, settings — **not** as Apple II input. **PLANNED** (UI); touch hardware: see VERIFIED.

### Bluetooth

- Keyboard (HID → hotkeys + Apple keyboard) — **PLANNED**
- Gamepad (reference 8BitDo SN30 Pro) — **PLANNED**
- Audio (headphones; BLE / Classic feasibility still experimental) — **PLANNED**

### Local audio

Piezo / simple transducer from Apple II speaker toggle — **PLANNED** (V1 goal).

### Display modes

- **LANDSCAPE_STANDALONE** — development without enclosure — **PLANNED** (viewport concept)
- **PORTRAIT_APPLE2_CASE** — vertical monitor viewport in the miniature enclosure — **PLANNED**
- **CONTROL_SCREEN** — touch UI; emulation may keep running — **PLANNED**

### Video colors

Independent of CRT effects:

| Mode | Goal |
| --- | --- |
| Composite Color | Authentic artifact colors |
| White monochrome | True mono, not desaturated composite |
| Green phosphor | Green monitor mono |
| Amber phosphor | Amber monitor mono |

Host renderer: **HOST_VERIFIED**; ESP32 path: **PLANNED**.

### Display effects (optional)

Sharp / Monitor / CRT-TV, strength OFF–HIGH — **PLANNED**, independent of color mode.

### Miniature Apple II enclosure

Base + monitor + Disk II, board vertical in the monitor, side USB-C accessible — CAD is **not** the current priority — **PLANNED**.

---

## Development

Incremental, PlatformIO; do not replace known-working board init without a concrete reason. Agents read `CLAUDE.md` first.

### µGulp / Gulp

```bash
npm install
npx gulp help          # task list
npx gulp docs          # README.md + README.de-DE.md
npx gulp docs:en-US    # English README only
npx gulp docs:de-DE    # German README only
npx gulp backup:git    # Git checkpoint (explicit; no auto-commit)
npx gulp backup:nas    # NAS copy (0–3 targets)
npx gulp backup:all    # docs → Git → NAS
```

Documentation sources:

- `dev/docs/readme/en-US.src.md` → `README.md`
- `dev/docs/readme/de-DE.src.md` → `README.de-DE.md`

Do not maintain generated READMEs by hand.

### Configure NAS backup

1. Copy `config/nas.targets.example` to `config/nas.targets.local`
2. Set up to three paths (`NAS_TARGET_1` … `NAS_TARGET_3`)
3. Or set environment variables of the same names

`nas.targets.local` is gitignored. At most three destinations; missing targets are skipped individually.

Dry-run: `ESP2_NAS_DRY_RUN=1`.

### Git backup

Explicit checkpoint (`backup: ESP][ YYYY-MM-DD HH:mm`), including **CLAUDE.md**, sources, docs, PlatformIO config. No force-push, no `reset --hard`. Without a remote: local commit only or a clear notice. Preview: `ESP2_BACKUP_GIT_DRY_RUN=1`.

---

## µGulp-ready

This repository uses the **µGulp** automation workflow for:

- documentation generation (`gulp docs`)
- README localization sources (en-US / de-DE)
- infrastructure validation (`npm run test:infra`)
- Git checkpoints (`gulp backup:git`)
- NAS backup when configured (`gulp backup:nas` / `backup:all`)

Platform / flash tasks use PlatformIO and project Gulp wrappers.

µGulp-ready badge: official artwork at `docs/assets/microgulp-ready.png` ([rules](https://microgulp.dev/ready/)).

---

## Building with PlatformIO

```bash
pio run
pio run -t upload
pio device monitor
```

Environment: `bringup` in `platformio.ini` (pioarduino / ESP32-S3, Arduino-GFX for CO5300). Adjust upload / monitor port locally (e.g. COM5).

---

## Input (target)

| Input | Role |
| --- | --- |
| Bluetooth keyboard | Emulator hotkeys + Apple II keyboard |
| Bluetooth gamepad | Joystick / buttons |
| Touch | Control Screen / library |
| Wired paddles | Later; shared joystick abstraction |

**PLANNED** except the verified touch hardware path.

---

## Storage / virtual disks

microSD library layout (e.g. `/apple2/games/<id>/game.json` + images). Firmware and media stay separable. Users supply legally obtained images. Metadata may exist even when the image is missing (`media not installed`).

Development catalog / reference: [apple2js](https://github.com/whscullin/apple2js) (MIT for the emulator — **not** for third-party media). ESP][ is not affiliated with apple2js. Users import legally obtained images offline (`media:import`); commercial titles remain `USER_SUPPLIED_ONLY`. Details: `docs/media/apple2js.md`, `docs/media/source-audit.md`.

---

## Video modes

See the table under Planned features. Apple II HGR (280 × 192) is not an ordinary RGB bitmap; artifact color is a core requirement. Host path: **HOST_VERIFIED**; device: **PLANNED**.

---

## Audio

Speaker toggle → audio engine → local and/or Bluetooth — **PLANNED**.

---

## Enclosure

Miniature Apple II setup; visible monitor viewport ≠ full AMOLED — **PLANNED**.

---

## License / legal notices

- Firmware and project documentation: open-source hobby project (license file to follow at publication).
- **No** commercial Apple II ROMs or copyrighted disk images in this repository.
- Users may use **legally obtained** images locally on microSD.
- “Apple II” and related marks belong to their respective owners; this project is independent and not endorsed by Apple.
- [apple2js](https://github.com/whscullin/apple2js) is used only as a development reference / catalog source; the emulator MIT license does not cover commercial website disk images.

---

## Status summary

| Area | Level |
| --- | --- |
| PlatformIO build / flash / serial | VERIFIED |
| CO5300 280×456 display + power | VERIFIED |
| FT3168 touch (I2C 47/48, mapping) | VERIFIED |
| microSD SPI 38–41 | VERIFIED |
| QMI8658 live sensing | VERIFIED (axis mapping open) |
| Host Apple II machine / media tooling | HOST_VERIFIED |
| Emulator on ESP32, Disk II, BT, audio, UI, enclosure | PLANNED |
