<!-- note
Pflegequelle der deutschen ESP][-README. Nur diese Datei (und en-US.src.md) bearbeiten.
Danach: npx gulp docs  → erzeugt README.de-DE.md und aktualisiert die Baseline de-DE.md
website-Blöcke können später ergänzt werden; unmarked Text erscheint in der Git-README.
-->

# ESP][

[English](README.md) | **Deutsch**

<p align="center">
  <a href="https://microgulp.dev/de/ready/">
    <img src="docs/assets/microgulp-ready.png" alt="µGulp Ready" width="110">
  </a>
</p>

> **🚧 In Entwicklung**
>
> Dieses Projekt befindet sich in aktiver Entwicklung.
> Hardware, Firmware, APIs, Dokumentation und Kompatibilität können
> sich noch ändern.

**ESP][** ist ein experimentelles Hobby-/Open-Source-Projekt: ein miniaturisierter, in sich geschlossener **Apple-II- / Apple-II+-kompatibler Rechner** auf dem Board **Waveshare ESP32-S3-Touch-AMOLED-1.64** — Rechner, Display und virtuelles Disk II im Taschenformat.

Paket-/Repo-Identifier (ASCII): `esp2` — sichtbarer Projektname bleibt **ESP][**.

### Wichtiger Status-Hinweis

- ESP][ ist **nicht fertig**.
- **HOST_VERIFIED** (Desktop-Tests) und **physisch verifiziert** (ESP32-S3 / CO5300) klar trennen.
- **Apple-System-ROMs** und **kommerzielle Diskettenabbilder** (z. B. Galaxian) liegen **nicht** im Repository — sie bleiben **user-supplied lokale Runtime-Assets**.
- Ein sichtbares Playfield bedeutet **nicht** automatisch „vollständig spielbar“ (Gamepad/Audio können noch ungetestet sein).

---

## Aktueller Meilenstein: Galaxian auf physischem ESP][

**`GALAXIAN_ESP32 = PASS`**

Echte Apple-II-Software läuft auf dem Board über den normalen Maschinenpfad — kein native Port, kein fertiges Screenshot-Overlay:

```
Apple-II+-System-ROM (user-supplied)
    → Disk II + Galaxian.dsk (user-supplied)
    → NMOS 6502 (fake6502)
    → Apple-II-Video-RAM (HGR)
    → HGR-Renderer (Sharp / Artifact Color)
    → Presentation (Classic / optional Landscape)
    → CO5300 AMOLED
```

Physisch verifiziert (Kurzfassung):

| Check | Ergebnis |
| --- | --- |
| Apple II+ Reset / interaktiver ROM-Start | PASS (`$FA62`, INTERACTIVE) |
| Galaxian-DSK-Upload (Device-SHA-256) | PASS |
| Disk-Mount / Autostart-Boot | PASS |
| Cracktro → Windows-CDC-Tastatur **A** → zweiter Disk-Load | PASS |
| Erkennbares Galaxian-HGR-Playfield | PASS |
| Emulierte Geschwindigkeit | ≈ **1,023 MHz** |
| Stabilität | >60 s ohne SPI-Panic / Watchdog |
| Windows-Tastatur-Bridge | PASS |
| 8BitDo-Gameplay | **NOT_TESTED** (kein XInput während des Laufs) |
| Physischer Lautsprecher / Bluetooth-Audio | **NOT_TESTED** |

Zwei generische Disk-II-Genauigkeitsfixes haben den Weg freigemacht (keine Galaxian-Hacks): physische Address-Field-Sektor-IDs und Half-Track-Stepper — Details in `docs/apple2/boot-forensics.md` / `docs/apple2/disk-ii.md`.

---

## Damals und heute

Während des Wehrdienstes durfte ich meinen Apple II mitnehmen. Er reiste im Koffer — Rechner, Monitor, Laufwerke und Zubehör. Diese Konfiguration zu transportieren bedeutete eine Menge Hardware zu schleppen.

Heute zielt ESP][ auf dieselbe Idee eines in sich geschlossenen Apple-II-Erlebnisses — auf einem ESP32-S3 mit winzigem AMOLED, microSD und virtuellem Disk II. Das Speicherproblem ist weitgehend gelöst; **das menschliche Auge ist jetzt die limitierende Peripherie**. Eine Lupe könnte das nützlichste optionale Zubehör sein.

### Wie viele Disketten passen auf 2 TB?

Ein standardmäßiges DOS-3.3-Apple-II-5,25″-Diskettenabbild enthält:

```
35 Tracks × 16 Sektoren × 256 Bytes = 143 360 Bytes ≈ 140 KiB
```

Nominally **2 TB** (= 2 000 000 000 000 Bytes) Rohkapazität fassen etwa:

```
2 000 000 000 000 / 143 360 ≈ 13,95 Millionen
```

rohe **140-KiB-DSK-Äquivalente** — rund **13,95 Millionen** Apple-II-Disketten.

Das ist theoretisch: Dateisystem-Overhead, andere ESP][-Dateien und größere Formate (WOZ/NIB) sind nicht eingerechnet. Es ist **keine** WOZ-Zählung.

Bei illustrativen ~**1,5 mm** pro physischer Diskette wäre der Stapel etwa:

```
13,95e6 × 1,5 mm ≈ 20,9 km ≈ 21 km
```

Disketten hoch. Die Dicke ist eine Spaß-Näherung — keine Labormessung.

---

## Aktueller Status

| Stufe | Bedeutung |
| --- | --- |
| **VERIFIED** | Auf dem physischen Board nachvollzogen |
| **HOST_VERIFIED** | Auf dem Entwicklungsrechner getestet, nicht auf dem ESP32 |
| **IN DEVELOPMENT** | Teilweise vorhanden, Verifikation unvollständig |
| **PLANNED** | Architektur/Ziel, noch nicht gebaut |

### VERIFIED (physisches Board)

**DISPLAY / Bring-up**

- CO5300 AMOLED, **280 × 456**
- Display-Power: ACTIVE → SCREENSAVER → OFF
- Touch (FT3168), microSD (SPI), IMU (QMI8658) wie in Phase 1

**Apple II auf ESP32 (Firmware `apple2_text`)**

- User-supplied Apple-II+-ROM + interaktiver Start
- Applesoft-/Keyboard-Latch-Pfad
- Level-4 Clean-Room-Disk-II-Spotcheck
- User Disk-II-PROM + DSK-Mount/Boot (Autostart)
- TEXT / LORES / HGR auf dem CO5300
- HGR **Sharp** (Mono) — **Classic + Sharp** bleibt Default
- HGR **Artifact Color** (`ArtifactRenderer`) — unter Galaxian auf dem CO5300 physisch aktiviert (`#ESP2PRESENT COLOR ARTIFACT`)
- Optionale **Landscape**-Presentation (90° CW + Nearest-Neighbor); Classic bleibt Default; Gehäuse-CAD unverändert
- Windows-11→USB-CDC-Input-Bridge (Tastatur mit Galaxian verifiziert)

### HOST_VERIFIED

- Host-Apple-II-Maschine: **fake6502**, Bus, Soft-Switches, Text/LoRes/HGR, Artifact-Farbe, Disk II (Level 4), Compat-Harness, User-ROM-Loader
- Media-Import / Katalog / Provenance
- Keine Apple-ROMs / kommerziellen Disks in Git

### PLANNED / NOT_TESTED (Beispiele)

- Control Screen, Bluetooth-Tastatur/-Gamepad/-Audio als Primärpfad
- Lokaler Piezo, verdrahtete Paddles, natives USB-HID-Host
- WOZ / voller Write-Pfad
- CRT/Monitor-Effekte (getrennt von Artifact Color)
- Finale Gehäuse-CAD-Varianten

---

## Presentation (Classic vs. Landscape)

Das sind **Display-Presentation**-Wahlmöglichkeiten — keine Apple-II-Softswitches.

| Modus | Rolle |
| --- | --- |
| **Classic** (Default) | 280×192-Viewport — verifizierte Baseline / Gehäuse-Orientierung |
| **Landscape** (optional) | 90° CW + Nearest-Neighbor-Skalierung auf 280×456, Seitenverhältnis erhalten |

| HGR-Farbe | Rolle |
| --- | --- |
| **Sharp** | Klares Mono |
| **Artifact Color** | Composite-Paar / High-Bit-Phase → RGB565 |

Physische Galaxian-Presentation-Matrix (Emulation bleibt ≈1,023 MHz):

| Modus | Viewport / Out | Notiz |
| --- | --- | --- |
| Classic + Sharp | 280×192 @ (0,48) | Default-Baseline |
| Classic + Artifact | 280×192 @ (0,48) | Höhere Render-Kosten; Farbpfad aktiv |
| Landscape + Sharp | 280×408 @ (0,24) | Voller 280×192-Frame, Seitenverhältnis erhalten |
| Landscape + Artifact | 280×408 @ (0,24) | Kandidat für lesbareres Bild auf dem winzigen AMOLED |

Details: `docs/architecture/landscape-presentation.md`, `docs/apple2/video.md`.

**Landscape-Gehäuse:** nur `LANDSCAPE_ENCLOSURE_VARIANT = CANDIDATE` — gleiches 1,64″-Board, mögliche spätere Monitor-Geometrie. **Kein CAD in diesem Meilenstein.** Waveshare 1,64″ bleibt **VERIFIED_BASELINE**.

---

## Hardware

| | |
| --- | --- |
| Board | Waveshare ESP32-S3-Touch-AMOLED-1.64 |
| MCU | ESP32-S3 |
| Flash / PSRAM | 16 MB Flash, 8 MB PSRAM |
| Display | 1,64″ AMOLED, **280 × 456**, Controller **CO5300** |
| Touch | **FT3168** |
| Storage | microSD |
| Radio | Wi-Fi, Bluetooth LE |
| IMU | **QMI8658** (falls bestückt) |
| Build | **PlatformIO** |

---

## Kompatibilität (Auszug)

| Titel | Host | ESP32 |
| --- | --- | --- |
| Galaxian | PASS / PLAYFIELD | **PASS / PLAYFIELD** (Input/Audio partiell) |
| ESP][ Boot Test | COMPLETED_TEST_PATH | Level-4 Spot PASS |
| Little Brick Out | NOT_TESTED | NOT_TESTED (später LORES/Paddle) |

Vollmatrix: `docs/compatibility/titles.json`. Nur user-supplied Medien.

---

## Entwicklung

Inkrementell, PlatformIO. Agenten lesen zuerst `CLAUDE.md`.

### µGulp / Gulp

```bash
npm install
npx gulp help
npx gulp docs
npx gulp backup:git
npx gulp backup:nas
npx gulp backup:all
```

Quellen: `dev/docs/readme/en-US.src.md` → `README.md`, `de-DE.src.md` → `README.de-DE.md`.

NAS-/Git-Backup: siehe englische README / `docs/tooling/backup.md`.

---

## Lizenz / Drittanbieter

Firmware und Tooling sind projekteigen, sofern nicht anders vermerkt. Drittanbieter-Code behält seine Lizenzen (z. B. **fake6502** CC0). Proprietäre Apple-ROMs und kommerzielle Diskettenabbilder nicht committen.
