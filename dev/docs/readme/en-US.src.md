<!-- note
Maintained English source for the ESP][ README. Edit this file (and de-DE.src.md).
Then: npx gulp docs  → generates README.md and updates the en-US.md baseline.
website blocks may be added later; unmarked text appears in the Git README.
-->

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

**ESP][** is an experimental hobby / open-source project: a miniature, self-contained **Apple II / Apple II+–compatible system** on the **Waveshare ESP32-S3-Touch-AMOLED-1.64** board — computer, display, and virtual Disk II in a pocket-sized package.

Package / repo identifier (ASCII): `esp2` — the visible project name remains **ESP][**.

### Important status note

- ESP][ is **not finished**.
- Distinguish **HOST_VERIFIED** (desktop tests) from **physically verified** on the ESP32-S3 / CO5300.
- **Apple system ROMs** and **commercial disk images** (e.g. Galaxian) are **not** distributed in this repository — they remain **user-supplied local runtime assets**.
- A visible playfield does **not** mean a title is fully playable (gamepad / audio may still be untested).

---

## Current milestone: Galaxian on physical ESP][

**`GALAXIAN_ESP32 = PASS`**

Real Apple II software executes on the board through the normal machine path — not a native port or a canned screenshot:

```
Apple II+ system ROM (user-supplied)
    → Disk II + Galaxian.dsk (user-supplied)
    → NMOS 6502 (fake6502)
    → Apple II video RAM (HGR)
    → HGR renderer (Sharp / Artifact Color)
    → presentation (Classic / optional Landscape)
    → CO5300 AMOLED
```

Physically verified so far (summary):

| Check | Result |
| --- | --- |
| Apple II+ reset / interactive ROM startup | PASS (`$FA62`, INTERACTIVE) |
| Galaxian DSK upload (device SHA-256) | PASS |
| Disk mount / Autostart boot | PASS |
| Cracktro → Windows CDC keyboard **A** → second disk load | PASS |
| Recognizable Galaxian HGR playfield | PASS |
| Emulated speed | ≈ **1.023 MHz** |
| Stability | >60 s without SPI panic / watchdog |
| Windows keyboard bridge | PASS |
| 8BitDo gameplay | **NOT_TESTED** (no XInput device connected during the run) |
| Physical speaker / Bluetooth audio | **NOT_TESTED** |

Two generic Disk II accuracy fixes unlocked this path (not Galaxian-specific hacks): physical Address Field sector IDs and half-track stepper distance — details in `docs/apple2/boot-forensics.md` / `docs/apple2/disk-ii.md`.

---

## Then vs. now

During compulsory military service, I was allowed to bring my Apple II with me. It travelled in a suitcase — computer, monitor, disk drives and accessories. Moving that setup meant carrying a lot of hardware.

Today, ESP][ aims at the same idea of a self-contained Apple II experience on an ESP32-S3 with a tiny AMOLED, microSD, and a virtual Disk II. The storage problem is largely solved; **human eyesight is now the limiting peripheral**. A magnifying glass may be the most useful optional accessory.

### How many floppies fit on 2 TB?

A standard DOS 3.3 Apple II 5.25″ disk image is:

```
35 tracks × 16 sectors × 256 bytes = 143,360 bytes ≈ 140 KiB
```

A nominal **2 TB** (= 2,000,000,000,000 bytes) of raw capacity holds about:

```
2,000,000,000,000 / 143,360 ≈ 13.95 million
```

raw **140 KiB DSK-equivalent** images — roughly **13.95 million** Apple II floppies.

That is theoretical: it ignores filesystem overhead, other ESP][ files, and larger formats such as WOZ/NIB. It is **not** a WOZ count.

At an illustrative ~**1.5 mm** per physical floppy, that stack would be about:

```
13.95e6 × 1.5 mm ≈ 20.9 km ≈ 21 km
```

of disks. Thickness is an approximation for fun — not a lab measurement.

---

## Current status

| Level | Meaning |
| --- | --- |
| **VERIFIED** | Confirmed on the physical board |
| **HOST_VERIFIED** | Tested on the development host, not on the ESP32 |
| **IN DEVELOPMENT** | Partially present; verification incomplete |
| **PLANNED** | Architecture / goal; not built yet |

### VERIFIED (physical board)

**DISPLAY / bring-up**

- CO5300 AMOLED, **280 × 456**
- Display power: ACTIVE → SCREENSAVER → OFF
- Touch (FT3168), microSD (SPI), IMU (QMI8658) as in Phase-1 bring-up

**Apple II on ESP32 (`apple2_text` firmware)**

- User-supplied Apple II+ ROM load + interactive startup
- Applesoft / keyboard latch path (basic PRINT checks when ROM supports them)
- Level-4 clean-room Disk II spot check
- User Disk II PROM + DSK mount/boot (Autostart)
- TEXT / LORES / HGR rendering to the CO5300
- HGR **Sharp** (mono luminance) presentation — **Classic + Sharp** remains the default
- HGR **Artifact Color** via shared `ArtifactRenderer` — physically exercised under Galaxian on the CO5300 (`#ESP2PRESENT COLOR ARTIFACT`)
- Optional **Landscape** presentation (CW 90° + nearest-neighbor fit) — Classic remains default; enclosure CAD unchanged
- Windows 11 → USB CDC input bridge (keyboard verified with Galaxian)

### HOST_VERIFIED

- Host Apple II machine: **fake6502**, bus, soft switches, text / LoRes / HGR, artifact color, Disk II (Level-4), compatibility harness, user-ROM loader (`npm run test:apple2`)
- Media import / catalog / provenance gates
- No Apple ROMs or commercial disks in Git

### PLANNED / NOT_TESTED (examples)

- Control Screen UI, Bluetooth keyboard/gamepad/audio as primary input/output
- Local piezo audio path, wired paddles, native USB HID host
- WOZ / full disk write path
- CRT/Monitor display effects (separate from Artifact Color)
- Final miniature enclosure CAD variants

---

## Presentation (Classic vs Landscape)

These are **display presentation** choices. They do not change Apple II soft-switches or VRAM.

| Mode | Role |
| --- | --- |
| **Classic** (default) | 280×192 viewport on the portrait panel — verified baseline / enclosure-oriented |
| **Landscape** (optional) | CW 90° + nearest-neighbor scale to fit the 280×456 panel, aspect ratio preserved |

| HGR color | Role |
| --- | --- |
| **Sharp** | Crisp mono on/off pixels |
| **Artifact Color** | Composite pair / high-bit phase → RGB565 (`ArtifactRenderer`) |

Physical Galaxian presentation matrix (emulation stays ≈1.023 MHz in all cases):

| Mode | Viewport / out | Notes |
| --- | --- | --- |
| Classic + Sharp | 280×192 @ (0,48) | Default baseline |
| Classic + Artifact | 280×192 @ (0,48) | Higher render cost; color path active |
| Landscape + Sharp | 280×408 @ (0,24) | Full 280×192 frame, aspect preserved |
| Landscape + Artifact | 280×408 @ (0,24) | Candidate “playable” view on the tiny AMOLED |

Development CDC:

- `#ESP2PRESENT COLOR SHARP|ARTIFACT`
- `#ESP2PRESENT ORIENT CLASSIC|LANDSCAPE`
- `#ESP2PRESENT STATUS`

Details: `docs/architecture/landscape-presentation.md`, `docs/apple2/video.md`.

**Landscape enclosure:** `LANDSCAPE_ENCLOSURE_VARIANT = CANDIDATE` only — same current 1.64″ board, possible future rotated monitor geometry. **No CAD change in this milestone.** Larger alternate display boards remain speculative; the Waveshare 1.64″ board is the **VERIFIED_BASELINE**.

---

## Hardware

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

## Compatibility (sample)

| Title | Host | ESP32 |
| --- | --- | --- |
| Galaxian | PASS / PLAYFIELD (HOST) | **PASS / PLAYFIELD** (physical; input/audio partial) |
| ESP][ Boot Test | COMPLETED_TEST_PATH | Level-4 spot PASS |
| Little Brick Out | NOT_TESTED | NOT_TESTED (prepared for later LORES/paddle) |
| Choplifter / Night Mission / … | see matrix | NOT_TESTED |

Full matrix: `docs/compatibility/titles.json`. `NOT_TESTED` ≠ incompatible. User-supplied media only.

---

## Emulator architecture (short)

Separated concerns: `apple2/` (CPU, bus, video, Disk II) vs display presentation vs CDC media/input. Disk formats in use: **DSK/DO**, **PO**, **NIB** fixtures on host; **WOZ** / writes **PLANNED**. Details: `docs/apple2/`, `CLAUDE.md`.

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

NAS backup includes valuable **local/downloaded** trees (`local/apple2`, `local/roms`, `_refs`, `3dprint`) while they remain **gitignored**. Git eligibility ≠ NAS eligibility — see `docs/tooling/backup.md`. Disposable caches (`node_modules`, `.pio`, …) stay excluded.

Dry-run: `ESP2_NAS_DRY_RUN=1`.

### Git backup

Explicit checkpoint (`backup: ESP][ YYYY-MM-DD HH:mm`), including **CLAUDE.md**, sources, docs, PlatformIO config. No force-push, no `reset --hard`. Without a remote: local commit only or a clear notice. Preview: `ESP2_BACKUP_GIT_DRY_RUN=1`.

---

## µGulp-ready

ESP][ is wired for the µGulp workflow (task catalog, docs generation, backup). See [microgulp.dev](https://microgulp.dev/).

---

## License / third party

Firmware and tooling are project-owned unless noted. Third-party code retains its licenses (e.g. **fake6502** CC0). Do not commit proprietary Apple ROMs or commercial disk images.
