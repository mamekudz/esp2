<p align="center">
  <img src="docs/assets/esp2-pear-logo.svg" alt="ESP][ Logo" width="220">
</p>

**ESP][ — An Apple in a hand.**

*Better an Apple in the hand than a dove on the roof.*
*(bewusst leicht absurde Anspielung auf: „Lieber den Spatz in der Hand als die Taube auf dem Dach.“)*

<p align="center">
  <img src="docs/assets/an-apple-in-a-hand.gif" alt="ESP][ — An Apple in a hand" width="420">
</p>

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

Echte Apple-II-Software läuft auf dem Board über den normalen Emulator-Stack — **kein** natives Galaxian-Port, **kein** fertiges Screenshot-Overlay, **keine** Fake-Firmware-Animation:

```
Apple-II+-System-ROM (user-supplied)
    → NMOS-6502-Emulation
    → Disk-II-Emulation
    → Galaxian-Diskettenabbild (user-supplied)
    → Apple-II-HGR-Videospeicher
    → Artifact-Color-Renderer (generischer HGR-Pfad)
    → physisches CO5300-AMOLED
```

Physisch verifiziert (Kurzfassung):

| Check | Ergebnis |
| --- | --- |
| Apple II+ Reset / interaktiver ROM-Start | PASS |
| Galaxian-DSK-Mount / Autostart-Boot | PASS |
| Cracktro → Tastatur **A** → zweiter Disk-Load | PASS |
| Erkennbares Galaxian-HGR-Playfield | PASS |
| Presentation-Matrix (Orient × Monitor × Effekt inkl. CRT) | PASS (physisch geprüft) |
| Emulierte Geschwindigkeit | ≈ **1,023 MHz** (Artifact Color kostet Renderzeit; Takt nicht abgesenkt) |
| Stabilität | mehrminütige Läufe ohne SPI-Panic / Watchdog |
| Windows-Tastatur-Bridge | PASS |
| Startup-Makro (`galaxian-start`) | Infrastruktur **IMPLEMENTED** |
| 8BitDo-Gameplay | **NOT_TESTED** |
| Logitech Dial → Paddle-Pfad | **DEV** (Host-Bridge; kein physischer PASS-Anspruch) |
| Physischer Lautsprecher / Bluetooth-Audio | **NOT_TESTED** |

Echtes Galaxian hat generische Disk-II-Genauigkeitsarbeit freigelegt (DOS-3.3 Address-Field-Mapping und Half-Track-Stepper) — keine titel-spezifischen Emulator-Hacks. Details: `docs/apple2/boot-forensics.md`, `docs/apple2/disk-ii.md`.

---

## Damals und heute

Während des Wehrdienstes durfte ich meinen Apple II mitnehmen. Ich habe ihn im Koffer transportiert. Diese Konfiguration mitzuschleppen war eine Menge Hardware — eine Menge Schleppen.

| Damals | Heute |
| --- | --- |
| Apple II im Koffer | **ESP][ in der Hand** |

**ESP][ — An Apple in a hand.**

Speicher ist nicht mehr der limitierende Faktor. **Das menschliche Auge ist es.** Auf dem winzigen AMOLED könnte eine Lupe das nützlichste optionale Zubehör sein.

---

## Wie viele Disketten passen auf 2 TB?

Ein standardmäßiges DOS-3.3-Apple-II-5,25″-Diskettenabbild enthält:

```
35 Tracks × 16 Sektoren × 256 Bytes = 143 360 Bytes ≈ 140 KiB
```

Nominal **2 TB** (= 2 000 000 000 000 Bytes) Rohkapazität fassen etwa:

```
2 000 000 000 000 / 143 360 ≈ 13,95 Millionen
```

rohe **140-KiB-DSK-Äquivalente** — rund **13,95 Millionen** Apple-II-Disketten.

Das ist ein **spaßiger theoretischer Vergleich**. Dateisystem-Overhead, andere ESP][-Dateien, praktische Limits und größere Formate (WOZ/NIB) sind nicht eingerechnet. Es ist **keine** WOZ-Zählung und behauptet **nicht**, eine 2‑TB-Karte speichere 13,95 Millionen WOZ-Dateien.

Bei illustrativen ~**1,5 mm** pro physischer Diskette wäre der Stapel etwa:

```
13,95e6 × 1,5 mm ≈ 21 km
```

Disketten hoch. Die Dicke ist eine Spaß-Näherung — keine Labormessung.

---

## Persistente Konfiguration & Startup-Makros

ESP][ kann ein **Geräteprofil** auf der microSD speichern (`/esp2/config/system.json` + `macros.json`):

- System-ROM-Pfad
- Laufwerk-1- / Laufwerk-2-Abbildpfade
- Boot von Diskette (Autostart)
- Presentation (Ausrichtung + HGR-Farbmodus)
- Bildschirmschoner-Idle-Timeout
- optionales **Startup-Makro**
- benannte Makros, die auch manuell laufen können

Das ist **generische Infrastruktur**. Galaxian ist ein Demo-Profil — kein fest verdrahtetes Titelverhalten im Emulatorkern.

Beispielprofil **galaxian-demo** (nur Pfade; Medien nicht in Git):

| Feld | Wert |
| --- | --- |
| ROM | `/esp2/roms/system.rom` |
| Drive 1 | `/esp2/disks/Galaxian.dsk` |
| Boot from disk | `true` |
| Orientation | `landscape` |
| Color | `artifact` |
| Screensaver | `300` Sekunden |
| Startup-Makro | `galaxian-start` |

**Standalone-Formulierung:** Der persistente Startup-Pfad ist **implementiert** und für Betrieb nur mit USB-Stromversorgung (ohne PC) gedacht. Die unabhängige Bestätigung eines reinen Netzteil-**Kaltstarts** bis zur vollen Galaxian-Demo wird noch validiert — `STANDALONE_GALAXIAN = PASS` ist noch nicht als erledigt zu behandeln. Details: `docs/architecture/device-config.md`.

### microSD-Backup / -Wiederherstellung

µGulp-**Device**-Aktionen sichern und stellen den ESP][-Baum `/esp2` als
**logische**, SHA-256-verifizierte Dateisystemkopie unter gitignored
`local/sd-backups/` her (privat NAS-gesichert). Kein rohes Kartenabbild —
Quell- und Zielkartenkapazität dürfen sich unterscheiden, wenn die Daten
passen. Details: `docs/architecture/sd-backup-restore.md`.

---

## Presentation (Classic vs. Landscape)

Das sind **Display-Presentation**-Wahlmöglichkeiten — keine Apple-II-Softswitches.

**Classic + Sharp** bleibt die etablierte Default-Baseline.

| Modus | Rolle |
| --- | --- |
| **Classic** (Default) | 280×192-Viewport auf dem Hochformat-Panel |
| **Landscape** (optional) | 90° CW + Nearest-Neighbor-Skalierung, mehr Panel-Fläche |

Warum Landscape: Auf diesem winzigen AMOLED ist das klassische Apple-II-Bild extrem klein. Landscape dreht und skaliert die Darstellung, damit deutlich mehr Panel-Fläche genutzt wird.

Gemessene Geometrie (aktuelle Firmware-Evidenz):

| | Viewport |
| --- | --- |
| Classic | 280 × 192 @ (0, 48) |
| Landscape | 280 × 408 @ (0, 24) |

Das Panel kann den vollständigen Apple-II-Framebuffer nicht in perfekter ganzzahliger **2×**-Skalierung zeigen. Landscape **maximiert nutzbare Fläche bei erhaltenem Seitenverhältnis**. Es ist lesbar/praktisch — kein pixelperfektes 2× und kein Emulator-Defekt.

Presentation besteht aus drei unabhängigen Dimensionen:

| Dimension | Wahl |
| --- | --- |
| **Ausrichtung** | Classic · Landscape |
| **Monitor** | Weiß (Mono) · Grün · Bernstein · Artifact Color |
| **Effekt** | Clean (Default) · CRT/TV (optional, leichtgewichtig) |

| Monitor | Rolle |
| --- | --- |
| **Weiß** | Echtes Mono aus Apple-II-Bits / Luminanz (früher „Sharp“) |
| **Grün / Bernstein** | Derselbe Mono-Pfad → Phosphor-RGB565 |
| **Artifact Color** | Generischer Apple-II-HGR-Composite-/Phasen-Pfad → RGB565 |

Artifact Color ist ein **generischer HGR-Pfad**, unter Galaxian auf dem physischen AMOLED ausgeübt — keine Galaxian-spezifische Palette. CRT/TV ist ein separater Post-Monitor-Pass.

**Zukünftiges Landscape-Gehäuse:** nur `LANDSCAPE_ENCLOSURE_VARIANT = CANDIDATE` — gleiches 1,64″-Board, mögliche spätere Monitor-Geometrie. **Noch kein CAD.** Eine spätere größere ESP][-Variante kann ein anderes Board nutzen, wenn genug rechteckige Auflösung für saubere 2×-Skalierung verfügbar wird; Waveshare 1,64″ bleibt **VERIFIED_BASELINE**.

---

## Display-Power / Bildschirmschoner

AMOLED-Power: ACTIVE → SCREENSAVER → OFF (nur Panel — kein ESP32-Deep-Sleep).

Während das Panel schläft oder aus ist, **läuft die Apple-II-Emulation weiter**. Nutzeraktivität (Touch / Tastatur / Pad) weckt das Display. Konfigurierbares Idle-Timeout (Demo-Profil: 300 s). Siehe `docs/architecture/display-power.md`, `docs/architecture/device-config.md`.

---

## Aktueller Status

| Stufe | Bedeutung |
| --- | --- |
| **VERIFIED** | Auf dem physischen Board nachvollzogen |
| **HOST_VERIFIED** | Auf dem Entwicklungsrechner getestet, nicht auf dem ESP32 |
| **IN DEVELOPMENT** | Teilweise vorhanden, Verifikation unvollständig |
| **PLANNED** | Architektur/Ziel, noch nicht gebaut |

### VERIFIED (physisches Board)

- CO5300 AMOLED **280 × 456**, Touch, microSD, IMU (Phase‑1)
- Display-Power ACTIVE → SCREENSAVER → OFF
- User-supplied Apple-II+-ROM + Disk II + DSK-Autostart
- TEXT / LORES / HGR auf dem CO5300
- HGR Sharp + Artifact Color; Classic- + Landscape-Presentation
- Windows-11→USB-CDC-Tastatur-Bridge (Galaxian)
- Windows-11→USB-CDC-Multi-Input-Bridge (Tastatur + Logitech Dial → PDL0) für Little Brick Out
- Little Brick Out physisches LORES+MIXED-Spielfeld (WHITE/CLEAN)
- Persistente Gerätekonfiguration + Makros (Pfade auf der SD)

### HOST_VERIFIED

- Host-Apple-II-Maschine: **fake6502**, Bus, Video, Disk II, Compat-Harness
- Media-Import / Katalog / Provenance
- Keine Apple-ROMs / kommerziellen Disks in Git

### PLANNED / NOT_TESTED (Beispiele)

- Control Screen; Bluetooth-Tastatur/-Gamepad/-Audio als Primärpfad
- Lokaler Piezo; verdrahtete Paddles; natives USB-HID-Host
- WOZ / voller Write-Pfad
- Finale Gehäuse-CAD-Varianten

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

Geplante Zusatzhardware: Piezo / lokaler Lautsprecher, physischer Reset-/Control-Taster, Bluetooth-Tastatur/-Gamepad/-Kopfhörer, später verdrahtete Analog-Paddles.

---

## Kompatibilität (Auszug)

| Titel | Host | ESP32 |
| --- | --- | --- |
| Galaxian | PASS / PLAYFIELD | **PASS / PLAYFIELD** (Input/Audio partiell) |
| Little Brick Out | PASS / LORES+MIXED (HOST) | **PASS / LORES+MIXED** (physisch; Dial-Bridge LIVE) |
| ESP][ Boot Test | COMPLETED_TEST_PATH | Level-4 Spot PASS |

Vollmatrix: `docs/compatibility/titles.json`. Nur user-supplied Medien — Galaxian (HGR) und Little Brick Out (LORES + Paddle / Logitech Dial über Windows-CDC) sind **verifizierte Kompatibilitätstitel**, keine mitgelieferten Firmware-Medien.

---

## Emulator-Architektur (kurz)

Getrennte Belange: `apple2/` (CPU, Bus, Video, Disk II) vs. Display-Presentation vs. CDC Media/Input. Diskformate: **DSK/DO**, **PO**, **NIB** auf dem Host; **WOZ** / Writes **PLANNED**. Details: `docs/apple2/`, `CLAUDE.md`.

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

Quellen: `dev/docs/readme/en-US.src.md` und `de-DE.src.md` → eine zweisprachige `README.md` (Englisch zuerst, dann Deutsch; Sprung `#deutsch`).

Ausführliche Projekthistorie: Root-**`RELEASES.json`** und der µGulp-Workflow `releases:history` — hier nicht als Changelog verdoppelt.

NAS-/Git-Backup: siehe englische README / `docs/tooling/backup.md`.

---

## Danksagung

Die spiel-/titelorientierte Bibliotheks-UX von ESP][ ist von **[Apple ][js](https://www.scullinsteel.com/apple2)** von Will Scullin inspiriert — danke für diesen hervorragenden Browser-Apple-II-Emulator und dafür, die Plattform online zugänglich zu halten.

---

## Lizenz / Drittanbieter

Firmware und Tooling sind projekteigen, sofern nicht anders vermerkt. Drittanbieter-Code behält seine Lizenzen (z. B. **fake6502** CC0). Proprietäre Apple-ROMs und kommerzielle Diskettenabbilder nicht committen.
