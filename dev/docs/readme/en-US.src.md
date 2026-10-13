<!-- note
Maintained English source for the ESP][ README. Edit this file (and de-DE.src.md).
Then: npx gulp docs  → bilingual README.md (EN then DE) + locale baselines.
website blocks may be added later; unmarked text appears in the Git README.
-->

<p align="center">
  <img src="docs/assets/esp2-pear-logo.svg" alt="ESP][ logo" width="220">
</p>

**ESP][ — An Apple in a hand.**

*Better an Apple in the hand than a dove on the roof.*

<p align="center">
  <img src="docs/assets/an-apple-in-a-hand.gif" alt="ESP][ — An Apple in a hand" width="420">
</p>

*English (below) · [Deutsch](#deutsch)*

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

**`MICROS_WRAP_REGRESSION = PASS`** · **`GALAXIAN_2H_SOAK = PASS`**

Real Apple II software executes on the board through the normal emulator stack — **not** a native Galaxian port, **not** a canned screenshot, **not** a fake firmware animation:

```
Apple II+ system ROM (user-supplied)
    → NMOS 6502 emulation
    → Disk II emulation
    → Galaxian disk image (user-supplied)
    → Apple II HGR video memory
    → Artifact Color renderer (generic HGR path)
    → physical CO5300 AMOLED
```

### Galaxian — 2-hour physical soak test passed

Continuous Apple II execution remained stable beyond the former **32-bit microsecond timer-wrap** boundary (~71.6 minutes). Verified on physical ESP][ (`apple2_text`, COM5): cycles kept advancing, cps ≈ 1.023 MHz, HGR active. That closes the known long-run throttle stall; it does **not** claim overnight or all-title stability. Details: `docs/architecture/apple2-throttle.md`, `docs/apple2/galaxian-2h-soak.json`.

Physically verified so far (summary):

| Check | Result |
| --- | --- |
| Apple II+ reset / interactive ROM startup | PASS |
| Galaxian DSK mount / Autostart boot | PASS |
| Cracktro → keyboard **A** → second disk load | PASS |
| Recognizable Galaxian HGR playfield | PASS |
| Presentation matrix (Orient × Monitor × Effect incl. CRT) | PASS (physical) |
| Emulated speed | ≈ **1.023 MHz** (Artifact Color raises render cost; clock not slowed) |
| Former ~71.6 min `micros()` throttle wrap | **PASS** (`MICROS_WRAP_REGRESSION`) |
| Galaxian ≥ 2 h physical soak | **PASS** (`GALAXIAN_2H_SOAK`) |
| Windows keyboard bridge | PASS |
| Startup macro input (`galaxian-start`) | infrastructure **IMPLEMENTED** |
| 8BitDo gameplay | **NOT_TESTED** |
| Logitech Dial → paddle path | **DEV** (host bridge; not a physical PASS claim) |
| Physical speaker / Bluetooth audio | **NOT_TESTED** |

Getting real Galaxian running exposed generic Disk II accuracy work (DOS 3.3 physical Address Field mapping and half-track stepper behavior) — not title-specific emulator hacks. Details: `docs/apple2/boot-forensics.md`, `docs/apple2/disk-ii.md`.

---

## Then vs. now

During compulsory military service, I was allowed to take my Apple II with me. I transported it in a suitcase. Carrying that setup around was a lot of hardware — a lot of hauling.

| Then | Now |
| --- | --- |
| Apple II in a suitcase | **ESP][ in the hand** |

**ESP][ — An Apple in a hand.**

Storage is no longer the limiting factor. **Human eyesight is.** On the tiny AMOLED a magnifying glass may be the most useful optional peripheral.

---

## How many floppies fit on 2 TB?

A standard DOS 3.3 Apple II 5.25″ disk image is:

```
35 tracks × 16 sectors × 256 bytes = 143,360 bytes ≈ 140 KiB
```

A nominal **2 TB** (= 2,000,000,000,000 bytes) of raw capacity holds about:

```
2,000,000,000,000 / 143,360 ≈ 13.95 million
```

raw **140 KiB DSK-equivalent** images — roughly **13.95 million** Apple II floppies.

That is a **fun theoretical comparison**. It ignores filesystem overhead, other ESP][ files, practical filesystem limits, and larger formats such as WOZ/NIB. It is **not** a WOZ count and does **not** claim a 2 TB card stores 13.95 million WOZ files.

At an illustrative ~**1.5 mm** per physical floppy, that stack would be about:

```
13.95e6 × 1.5 mm ≈ 21 km
```

of disks. Thickness is an approximation for fun — not a lab measurement.

---

## Persistent configuration & startup macros

ESP][ can store a **device profile** on the microSD (`/esp2/config/system.json` + `macros.json`):

- system ROM path
- Drive 1 / Drive 2 image paths
- boot from disk (Autostart)
- presentation (orientation + HGR color mode)
- screensaver idle timeout
- optional **startup macro**
- named macros that can also be run manually

This is **generic infrastructure**. Title behavior lives in **user profiles** under `config/device/profiles/` — not hardcoded in the emulator core.

### Tracked demo profiles (paths only)

Disk images are **never** in Git. Activate media locally after clone with µGulp **`setup:profiles`** (or `setup:first-run` for a single demo title).

| Profile id | Drive 1 (device path) | Presentation | Notes |
| --- | --- | --- | --- |
| `galaxian-demo` | `/esp2/disks/Galaxian.dsk` | landscape / artifact | Startup macro `galaxian-start` |
| `little-brick-out` | `/esp2/disks/LittleBrickOut.dsk` | landscape / white | LORES + paddle / Dial; macro `little-brick-out-start` |
| `star-blazer` | `/esp2/disks/StarBlazer.dsk` | landscape / artifact | HGR demo; empty macros |

Example **galaxian-demo** fields:

| Field | Value |
| --- | --- |
| ROM | `/esp2/roms/system.rom` |
| Drive 1 | `/esp2/disks/Galaxian.dsk` |
| Boot from disk | `true` |
| Orientation | `landscape` |
| Color | `artifact` |
| Screensaver | `300` seconds |
| Startup macro | `galaxian-start` |

### Apple ][js catalog profiles (downloadable)

Path-only **device profiles** under `config/device/profiles/<id>/` reference titles from [Apple ][js](https://www.scullinsteel.com/apple2). Each entry has a device disk **path**; media is fetched from the website JSON URL (e.g. `…/json/disks/starblazer.json`) — **not** stored in Git.

Download after clone (gitignored `local/apple2/`, LOCAL_TEST_ONLY):

```bash
npx gulp setup:profiles
npx gulp setup:profiles --profile star-blazer --seed star-blazer
npx gulp setup:profiles --profile category:Game
```

Also written as a standalone file: [`docs/device/generated/profile-catalog.md`](docs/device/generated/profile-catalog.md). Regenerate profiles + this list: `npx gulp device:profiles:generate`.

<!-- include:docs/device/generated/profile-catalog.md -->

Downloads land in gitignored `local/apple2/`; optional seed writes `local/device/config/`. Details: `docs/architecture/device-config.md`.

**Note (µGulp UI):** select dropdowns are capped at ~200 options in the current µGulp host, so `device:config` / `setup:profiles` browse by **group** (curated demos or category) instead of one flat list. Raising that host limit is a µGulp change; grouping remains useful for large catalogs regardless.

**Standalone wording:** the persistent startup path is **implemented** and intended for USB-power-only operation without a PC. Independent confirmation of a plain power-supply **cold boot** to the full Galaxian demo is still being validated — do not treat `STANDALONE_GALAXIAN = PASS` as settled until that check is recorded.

### microSD backup / restore

µGulp **Device** actions back up and restore the ESP][ `/esp2` tree as a
**logical**, SHA-256–verified filesystem copy under gitignored
`local/sd-backups/` (privately NAS-backed). Not a raw card image — source and
destination card capacities may differ when the data fits. Details:
`docs/architecture/sd-backup-restore.md`.

---

## Presentation (Classic vs Landscape)

These are **display presentation** choices. They do not change Apple II soft-switches or VRAM.

**Classic + Sharp** remains the established default baseline.

| Mode | Role |
| --- | --- |
| **Classic** (default) | 280×192 viewport on the portrait panel |
| **Landscape** (optional) | CW 90° + nearest-neighbor scale to use more of the 280×456 panel |

Why Landscape exists: on this tiny AMOLED the Classic Apple II image is extremely small. Landscape rotates and scales the presentation so substantially more of the panel is used.

Measured geometry (current firmware evidence):

| | Viewport |
| --- | --- |
| Classic | 280 × 192 @ (0, 48) |
| Landscape | 280 × 408 @ (0, 24) |

The panel cannot show the full Apple II framebuffer at a perfect integer **2×** scale. Landscape therefore **maximizes usable area while preserving aspect ratio**. It is readable / practical — not pixel-perfect 2× scaling, and not an emulator defect.

Presentation is three independent dimensions (not hard-coded combo modes):

| Dimension | Choices |
| --- | --- |
| **Orientation** | Classic · Landscape |
| **Monitor** | White (mono) · Green · Amber · Artifact Color |
| **Effect** | Clean (default) · CRT/TV (optional, lightweight) |

| Monitor | Role |
| --- | --- |
| **White** | True mono from Apple II bits / luminance (former “Sharp”) |
| **Green / Amber** | Same mono path → phosphor RGB565 (not a desaturate of Artifact) |
| **Artifact Color** | Generic Apple II HGR composite / phase → RGB565 (`ArtifactRenderer`) |

Artifact Color is a **generic HGR path**, exercised under Galaxian on the physical AMOLED — not a Galaxian-specific palette. CRT/TV is a separate post-monitor pass (scanline + slight softness; chroma bleed only with Artifact).

Physical checks (emulation stays ≈1.023 MHz): Classic/Landscape × White/Green/Amber/Artifact × Clean, plus Landscape × CRT variants including **Landscape + Artifact + CRT**.

Development CDC: `#ESP2PRESENT MONITOR|EFFECT|ORIENT|STATUS` (legacy `COLOR SHARP|ARTIFACT` still accepted). Details: `docs/architecture/landscape-presentation.md`, `docs/apple2/video.md`.

**Future Landscape enclosure:** `LANDSCAPE_ENCLOSURE_VARIANT = CANDIDATE` only — same current 1.64″ board, possible later rotated monitor geometry. **No CAD yet.** A future larger ESP][ variant may use another board if enough usable rectangular resolution for clean 2× Apple II scaling becomes available; the Waveshare 1.64″ board remains the **VERIFIED_BASELINE**.

---

## Display power / screensaver

AMOLED power management: ACTIVE → SCREENSAVER → OFF (panel only — not ESP32 deep sleep).

While the panel sleeps or is off, **Apple II emulation continues**. User activity (touch / keyboard / pad) wakes the display. Configurable idle timeout (demo profile: 300 s). See `docs/architecture/display-power.md`, `docs/architecture/device-config.md`.

---

## Current status

| Level | Meaning |
| --- | --- |
| **VERIFIED** | Confirmed on the physical board |
| **HOST_VERIFIED** | Tested on the development host, not on the ESP32 |
| **IN DEVELOPMENT** | Partially present; verification incomplete |
| **PLANNED** | Architecture / goal; not built yet |

### VERIFIED (physical board)

- CO5300 AMOLED **280 × 456**, touch, microSD, IMU (Phase-1 bring-up)
- Display power ACTIVE → SCREENSAVER → OFF
- User-supplied Apple II+ ROM + Disk II + DSK Autostart
- TEXT / LORES / HGR to the CO5300
- HGR Sharp + Artifact Color; Classic + Landscape presentations
- Windows 11 → USB CDC keyboard bridge (Galaxian)
- Windows 11 → USB CDC multi-input bridge (keyboard + Logitech Dial → PDL0) for Little Brick Out
- Little Brick Out physical LORES+MIXED playfield (WHITE/CLEAN)
- Persistent device config + macros (paths on SD)

### HOST_VERIFIED

- Host Apple II machine: **fake6502**, bus, video, Disk II, compatibility harness
- Media import / catalog / provenance gates
- No Apple ROMs or commercial disks in Git

### PLANNED / NOT_TESTED (examples)

- Control Screen UI; Bluetooth keyboard / gamepad / audio as primary I/O
- Local piezo audio; wired paddles; native USB HID host
- WOZ / full disk write path
- Final miniature enclosure CAD variants

---

## Hardware

### Supported targets

Choose the board with the µGulp **Project Control** bar (**Target** radio) under the project identity — not inside individual tasks. Persistence: `.microgulp/project-controls.json` (gitignored).

| Platform ID | UI label | Status | Default PlatformIO env |
| --- | --- | --- | --- |
| `waveshare-amoled-s3` | Waveshare AMOLED | Production ESP][ | `apple2_text` |
| `diymore-es3c28p` | diymore 2.8" | Bring-up / in development | `diymore_apple2` |
| `waveshare-rlcd42` | RLCD 4.2 | Mono Apple II (in development) | `rlcd42_apple2` |

diymore part number (metadata only): **AM03-FBBA0125-AAA**. See `docs/hardware/diymore-es3c28p.md`.

**RLCD 4.2** (Waveshare/Luckfox ESP32-S3-RLCD-4.2): reflective **ST7305** 300×400 (logical 400×300), no touch — Control Center via **KEY / BOOT**. Classic present defaults to sharp **1×** (280×192 + letterbox); optional full-panel fit in the Display menu. Details: `docs/hardware/waveshare-rlcd42.md`.

### Waveshare (production)

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
| Little Brick Out | PASS / LORES+MIXED (HOST) | **PASS / LORES+MIXED** (physical; Dial bridge LIVE) |
| ESP][ Boot Test | COMPLETED_TEST_PATH | Level-4 spot PASS |
| Choplifter / Night Mission / … | see matrix | NOT_TESTED |

Full matrix: `docs/compatibility/titles.json`. `NOT_TESTED` ≠ incompatible. User-supplied media only — Galaxian (HGR) and Little Brick Out (LORES + paddle / Logitech Dial via Windows CDC) are **verified compatibility titles**, not bundled firmware media.

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
npx gulp setup:first-run   # ROMs + one demo title + seed local config
npx gulp setup:profiles    # selectively activate device profiles / media
npx gulp docs          # bilingual README.md (EN + DE) + locale baselines
npx gulp docs:en-US    # same compose (refresh baselines + README.md)
npx gulp docs:de-DE    # same compose (refresh baselines + README.md)
npx gulp backup:git    # Git checkpoint (explicit; no auto-commit)
npx gulp backup:nas    # NAS copy (0–3 targets)
npx gulp backup:all    # docs → Git → NAS
```

Documentation sources (edit these — not the generated root README):

- `dev/docs/readme/en-US.src.md`
- `dev/docs/readme/de-DE.src.md`

`npx gulp docs` builds one GitHub `README.md` with English first, then German
(microCSS style; jump link `#deutsch`). Do not maintain `README.md` by hand.

Detailed project history lives in root **`RELEASES.json`** and the µGulp release-history workflow (`releases:history`) — not duplicated here as a changelog.

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

## Acknowledgments

ESP][’s game-library / title-first UX is inspired by **[Apple ][js](https://www.scullinsteel.com/apple2)** by Will Scullin — thank you for that excellent browser Apple II emulator and for years of keeping the platform approachable online.

---

## License / third party

Firmware and tooling are project-owned unless noted. Third-party code retains its licenses (e.g. **fake6502** CC0). Do not commit proprietary Apple ROMs or commercial disk images.
