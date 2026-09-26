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

**ESP][** ist ein experimentelles Hobby-/Open-Source-Projekt: ein miniaturisierter, in sich geschlossener **Apple-II-Emulator** auf dem Board **Waveshare ESP32-S3-Touch-AMOLED-1.64**.

Paket-/Repo-Identifier (ASCII): `esp2` — sichtbarer Projektname bleibt **ESP][**.

### Wichtiger Status-Hinweis

- ESP][ ist **nicht fertig**.
- Die **Apple-II-Emulation ist noch nicht vollständig** und **noch nicht** in die ESP32-Firmware integriert.
- Verifizierte **Phase-1-Hardware** (Display, Touch, microSD, …) bedeutet **nicht**, dass der komplette Emulator auf dem Gerät läuft.
- Host-getestete Emulator-Komponenten (`HOST_VERIFIED`) sind von **ESP32-getesteten** Teilen zu unterscheiden.
- Controller-/Medien-Kompatibilität und Disk-II bleiben in Entwicklung.

---

## Aktueller Status

Klar getrennt:

| Stufe | Bedeutung |
| --- | --- |
| **VERIFIED** | Auf dem physischen Board nachvollzogen |
| **HOST_VERIFIED** | Auf dem Entwicklungsrechner getestet, nicht auf dem ESP32 |
| **IN DEVELOPMENT** | Teilweise vorhanden, Verifikation unvollständig |
| **PLANNED** | Architektur/Ziel, noch nicht gebaut |

### VERIFIED (physisches Board)

**DISPLAY**

- CO5300 AMOLED initialisiert
- Auflösung **280 × 456**
- Visuell verifiziert (Testmuster / Orientierung)
- Display-Power: ACTIVE → SCREENSAVER → OFF (CO5300 `displayOff`/`displayOn`)

**TOUCH**

- FT3168 auf shared I2C
- SDA **GPIO47**, SCL **GPIO48**
- Touch-Koordinaten-Mapping verifiziert

**microSD**

- SPI: CS **GPIO38**, MOSI **GPIO39**, MISO **GPIO40**, SCLK **GPIO41**
- SDHC / FAT verifiziert
- 32-GB-Karte getestet
- Lesen / Schreiben / Persistenz verifiziert

**IMU (QMI8658)**

- Erkannt unter I2C-Adresse **0x6B**
- WHO_AM_I **0x05**, Revision **0x7C**
- Live-Accelerometer/Gyro verifiziert
- Physisches Achsen-Mapping noch ausstehend

**Build / Diagnose**

- PlatformIO-Firmware baut und flash
- Serielle `[TAG]`-Diagnostik für Bring-up

### HOST_VERIFIED (nur Host-Tooling / Host-Maschine)

- Media-Import / Katalog / Provenance-Gates (`media:import`, apple2js-Katalog)
- Host-seitige Apple-II-Maschine: **fake6502**, Bus, Soft-Switches, Text/LoRes/HGR, Artifact-Farbe (`npm run test:apple2`)
- Synthetische Test-ROM / keine Apple-ROMs im Repo

### PLANNED

Alles unter [Geplante Funktionen](#geplante-funktionen) — inkl. ESP32-Integration des Emulators, Disk II, Bluetooth-Eingabe/-Audio, Control Screen.

---

## Hardware

Zielplattform:

| | |
| --- | --- |
| Board | Waveshare ESP32-S3-Touch-AMOLED-1.64 |
| MCU | ESP32-S3 |
| Flash / PSRAM | 16 MB Flash, 8 MB PSRAM |
| Display | 1,64″ AMOLED, **280 × 456**, Controller **CO5300** |
| Touch | **FT3168** |
| Speicher | microSD |
| Funk | Wi-Fi, Bluetooth LE |
| IMU | **QMI8658** (falls bestückt) |
| Build | **PlatformIO** (kein Arduino-IDE-Hauptprojekt) |

Geplante Zusatzhardware: Piezo/Lokal-Lautsprecher, physischer Reset/Control-Taster, Bluetooth-Tastatur/-Gamepad/-Kopfhörer, später verdrahtete Analog-Paddles.

---

## Geplante Funktionen

### Apple-II-Emulation

Host-seitige Maschine + **fake6502** (CC0) unter `third_party/fake6502/` — **HOST_VERIFIED**. Firmware-Integration auf dem ESP32: **noch nicht**. Keine Apple-ROMs im Repo (synthetische Test-ROM).

### Virtuelle Disk II / Media-Schicht

Getrennt vom Core: Disk-II-Controller → Virtual Disk → Drive 1/2 → Image auf microSD. Formate schrittweise (DSK, PO, NIB, WOZ). Host-Import-Tooling vorhanden; Disk-II-Controller **PLANNED.** Keine eingebetteten kommerziellen Disk-Images im Repo.

### Touch-Bedienung

Primär Control Screen, Bibliothek, Disk-Management, Settings — **nicht** als Apple-II-Eingabe. **PLANNED** (UI), Touch-Hardware: siehe VERIFIED.

### Bluetooth

- Tastatur (HID → Hotkeys + Apple-Tastatur) — **PLANNED**
- Gamepad (Referenz 8BitDo SN30 Pro) — **PLANNED**
- Audio (Kopfhörer; BLE/Classic-Machbarkeit experimentell klären) — **PLANNED**

### Lokales Audio

Piezo / einfacher Wandler aus Apple-II-Speaker-Toggle — **PLANNED** (V1-Ziel).

### Anzeigemodi

- **LANDSCAPE_STANDALONE** — Entwicklung ohne Gehäuse — **PLANNED** (Viewport-Konzept)
- **PORTRAIT_APPLE2_CASE** — vertikales Monitor-Viewport im Miniaturgehäuse — **PLANNED**
- **CONTROL_SCREEN** — Touch-UI, Emulation darf weiterlaufen — **PLANNED**

### Videofarben

Unabhängig von CRT-Effekten:

| Modus | Ziel |
| --- | --- |
| Composite Color | Authentische Artifact-Farben |
| White monochrome | Echtes Mono, nicht entsättigtes Composite |
| Green phosphor | Grünes Monitor-Mono |
| Amber phosphor | Bernsteinfarbenes Monitor-Mono |

Host-Renderer: **HOST_VERIFIED**; ESP32-Pfad: **PLANNED**.

### Display-Effekte (optional)

Sharp / Monitor / CRT-TV, Stärke OFF–HIGH — **PLANNED**, unabhängig von der Farbmodus-Wahl.

### Miniatur-Apple-II-Gehäuse

Basis + Monitor + Disk-II, Board senkrecht im Monitor, USB-C seitlich zugänglich — CAD **nicht** aktuelle Priorität — **PLANNED**.

---

## Entwicklung

Inkrementell, PlatformIO, bestehende Board-Init nicht ohne Grund ersetzen. Agenten lesen zuerst `CLAUDE.md`.

### µGulp / Gulp

```bash
npm install
npx gulp help          # Aufgabenliste
npx gulp docs          # README.md + README.de-DE.md
npx gulp docs:en-US    # nur englische README
npx gulp docs:de-DE    # nur deutsche README
npx gulp backup:git    # Git-Checkpoint (explizit, kein Auto-Commit)
npx gulp backup:nas    # NAS-Kopie (0–3 Ziele)
npx gulp backup:all    # docs → Git → NAS
```

Dokumentationsquellen:

- `dev/docs/readme/en-US.src.md` → `README.md`
- `dev/docs/readme/de-DE.src.md` → `README.de-DE.md`

Generierte READMEs nicht manuell pflegen.

### NAS-Backup konfigurieren

1. `config/nas.targets.example` nach `config/nas.targets.local` kopieren
2. Bis zu drei Pfade setzen (`NAS_TARGET_1` … `NAS_TARGET_3`)
3. Oder Umgebungsvariablen gleichen Namens setzen

`nas.targets.local` ist gitignored. Maximal drei Ziele; fehlende Ziele werden einzeln übersprungen.

Dry-Run: `ESP2_NAS_DRY_RUN=1`.

### Git-Backup

Expliziter Checkpoint (`backup: ESP][ YYYY-MM-DD HH:mm`), inkl. **CLAUDE.md**, Quellen, Docs, PlatformIO-Config. Kein Force-Push, kein `reset --hard`. Ohne Remote: nur lokaler Commit bzw. klarer Hinweis. Preview: `ESP2_BACKUP_GIT_DRY_RUN=1`.

---

## µGulp-ready

Dieses Repository nutzt den **µGulp**-Automatisierungsworkflow für:

- Dokumentationsgenerierung (`gulp docs`)
- Lokalisierung der README-Quellen (en-US / de-DE)
- Infrastruktur-Validierung (`npm run test:infra`)
- Git-Checkpoints (`gulp backup:git`)
- NAS-Backup, sofern konfiguriert (`gulp backup:nas` / `backup:all`)

Weitere Plattform-/Flash-Tasks laufen über PlatformIO und projekteigene Gulp-Wrapper.

µGulp-Ready-Badge: offizielles Artwork unter `docs/assets/microgulp-ready.png` ([Regeln](https://microgulp.dev/de/ready/)).

---

## Bauen mit PlatformIO

```bash
pio run
pio run -t upload
pio device monitor
```

Umgebung: `bringup` in `platformio.ini` (pioarduino / ESP32-S3, Arduino-GFX für CO5300). Upload-/Monitor-Port lokal anpassen (z. B. COM5).

---

## Bedienung (Ziel)

| Eingabe | Rolle |
| --- | --- |
| Bluetooth-Tastatur | Emulator-Hotkeys + Apple-II-Tastatur |
| Bluetooth-Gamepad | Joystick / Buttons |
| Touch | Control Screen / Bibliothek |
| Verdrahtete Paddles | Später, gemeinsames Joystick-Abstrakt |

**PLANNED** außer verifiziertem Touch-Hardware-Pfad.

---

## Speicher / virtuelle Disks

microSD mit Bibliotheksstruktur (z. B. `/apple2/games/<id>/game.json` + Images). Firmware und Medien getrennt. Nutzer stellen legal erworbene Images selbst bereit. Metadaten dürfen existieren, auch wenn das Image fehlt (`media not installed`).

Entwicklungskatalog/Referenz: [apple2js](https://github.com/whscullin/apple2js) (MIT für den Emulator — **nicht** für Drittmedien). ESP][ ist nicht mit apple2js affiliated. Nutzer importieren legal erworbene Images offline (`media:import`); kommerzielle Titel bleiben `USER_SUPPLIED_ONLY`. Details: `docs/media/apple2js.md`, `docs/media/source-audit.md`.

---

## Videomodi

Siehe Tabelle unter Geplante Funktionen. Apple-II-HGR (280 × 192) ist kein gewöhnliches RGB-Bitmap; Artifact-Farbe ist Kernanforderung. Host-Pfad: **HOST_VERIFIED**; Gerät: **PLANNED**.

---

## Audio

Speaker-Toggle → Audio-Engine → lokal und/oder Bluetooth — **PLANNED**.

---

## Gehäuse

Miniatur-Apple-II-Setup; sichtbares Monitor-Viewport ≠ volles AMOLED — **PLANNED**.

---

## Lizenz / rechtliche Hinweise

- Firmware und Projektdokumentation: Open-Source-Hobbyprojekt (Lizenzdatei folgt bei Veröffentlichung).
- **Keine** kommerziellen Apple-II-ROMs oder urheberrechtlich geschützten Disk-Images in diesem Repository.
- Nutzer dürfen **legal erworbene** Images lokal auf der microSD verwenden.
- „Apple II“ und verwandte Marken gehören ihren Rechteinhabern; dieses Projekt ist unabhängig und nicht von Apple endorsed.
- [apple2js](https://github.com/whscullin/apple2js) wird nur als Entwicklungsreferenz / Katalogquelle genutzt; die MIT-Lizenz des Emulators deckt keine kommerziellen Disk-Images der Website ab.

---

## Kurzüberblick Status

| Bereich | Stufe |
| --- | --- |
| PlatformIO-Build / Flash / Serial | VERIFIED |
| CO5300 280×456 Display + Power | VERIFIED |
| FT3168 Touch (I2C 47/48, Mapping) | VERIFIED |
| microSD SPI 38–41 | VERIFIED |
| QMI8658 Live-Sensorik | VERIFIED (Achsen-Mapping offen) |
| Host Apple-II-Maschine / Media-Tooling | HOST_VERIFIED |
| Emulator auf ESP32, Disk II, BT, Audio, UI, Gehäuse | PLANNED |
