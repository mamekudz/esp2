/**
 * Build i18x/gulp dictionaries for ESP][ µGulp task metadata.
 * Source language is English (keys). de-DE provides German UI strings.
 *
 * Run: node dev/tools/build-i18x-gulp.mjs
 */
import { mkdirSync, writeFileSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "../..");
const outDir = join(root, "i18x", "gulp");
mkdirSync(outDir, { recursive: true });

/** @type {Record<string, string>} */
const de = {
  // Groups
  'Firmware<context="µGroup"/>': "Firmware",
  'Tests<context="µGroup"/>': "Tests",
  'Tools<context="µGroup"/>': "Werkzeuge",
  'Docs<context="µGroup"/>': "Docs",
  'Git<context="µGroup"/>': "Git",
  'Backup<context="µGroup"/>': "Backup",
  'Docs & Backup<context="µGroup"/>': "Docs & Backup",
  'Media / Catalog<context="µGroup"/>': "Medien / Katalog",
  'Apple II/Media<context="µGroup"/>': "Apple II/Medien",
  'Apple II/Emulator<context="µGroup"/>': "Apple II/Emulator",
  'Apple II/Compatibility<context="µGroup"/>': "Apple II/Kompatibilität",
  'Apple II/Device<context="µGroup"/>': "Apple II/Gerät",
  'Device<context="µGroup"/>': "Gerät",
  'Apple II Host<context="µGroup"/>': "Apple II Host",
  'Media / local apple2<context="µGroup"/>': "Medien / lokal apple2",

  // Firmware
  'Build Firmware V<version/><context="µDisplayName"/>': "Firmware bauen V<version/>",
  'Compiles ESP][ with PlatformIO (env: bringup). Does not change board config.<context="µDescription"/>':
    "Kompiliert ESP][ mit PlatformIO (Umgebung: bringup). Ändert die Board-Konfiguration nicht.",
  'Compiles ESP][ with PlatformIO. Default env from platformio.ini / ESP2_PIO_ENV (bringup|core_smoke|apple2_text).<context="µDescription"/>':
    "Kompiliert ESP][ mit PlatformIO. Standard-Umgebung aus platformio.ini / ESP2_PIO_ENV (bringup|core_smoke|apple2_text).",
  'Show Active Firmware Env V<version/><context="µDisplayName"/>': "Aktive Firmware-Umgebung anzeigen V<version/>",
  'Prints the active PlatformIO environment and known envs from platformio.ini.<context="µDescription"/>':
    "Zeigt die aktive PlatformIO-Umgebung und bekannte Envs aus platformio.ini.",
  'Build & Upload Firmware V<version/><context="µDisplayName"/>': "Firmware bauen & hochladen V<version/>",
  'Rebuilds and flashes ESP][ firmware in one step.<context="µDescription"/>':
    "Baut die ESP][-Firmware neu und flasht sie in einem Schritt.",
  'Set ESP2_PORT=COMx to skip the port dialog.<context="µTooltip"/>':
    "ESP2_PORT=COMx setzen, um den Port-Dialog zu überspringen.",
  'Upload Firmware V<version/><context="µDisplayName"/>': "Firmware hochladen V<version/>",
  'Firmware Memory Usage V<version/><context="µDisplayName"/>': "Firmware-Speicherbelegung V<version/>",
  'Clean Firmware Build Files V<version/><context="µDisplayName"/>': "Firmware-Build-Dateien bereinigen V<version/>",
  'Removes .pio/build artefacts for the active PlatformIO env (ESP2_PIO_ENV / default bringup).<context="µDescription"/>':
    "Entfernt .pio/build-Artefakte der aktiven PlatformIO-Umgebung (ESP2_PIO_ENV / Standard bringup).",
  'Rebuild Firmware V<version/><context="µDisplayName"/>': "Firmware neu bauen V<version/>",
  'clean → build for the active PlatformIO env.<context="µDescription"/>':
    "clean → build für die aktive PlatformIO-Umgebung.",

  // Tools
  'First-run setup (download + config) V<version/><context="µDisplayName"/>':
    "Erststart-Setup (Download + Config) V<version/>",
  'For a fresh clone: download redistributable Apple II ROMs and a demo title from the network, prepare the runtime disk, and seed local device configuration. Does not flash firmware or upload to the ESP][.<context="µDescription"/>':
    "Nach frischem Clone: lädt weiterverbreitbare Apple-II-ROMs und einen Demo-Titel aus dem Netz, bereitet die Laufzeitdiskette vor und setzt die lokale Gerätekonfiguration. Flasht keine Firmware und lädt nichts auf den ESP][ hoch.",
  'Highlighted after clone when local media/config is missing. Network required for downloads.<context="µTooltip"/>':
    "Hervorgehoben nach dem Clone, wenn lokale Medien/Config fehlen. Netzwerk für Downloads nötig.",
  'Fresh clone / missing local media — run first-run setup.<context="µAttentionTooltip"/>':
    "Frischer Clone / fehlende lokale Medien — Erststart-Setup ausführen.",
  'ESP][ first-run setup<context="task parameter"/>': "ESP][-Erststart-Setup",
  'Start setup<context="button text"/>': "Setup starten",
  'Demo title id (apple2js)<context="task parameter"/>': "Demo-Titel-ID (apple2js)",
  'Downloads redistributable ROMs and this LOCAL_TEST_ONLY title from the network, prepares a runtime disk, and seeds local/device/config from the matching profile when present.<context="task parameter"/>':
    "Lädt weiterverbreitbare ROMs und diesen LOCAL_TEST_ONLY-Titel aus dem Netz, bereitet eine Laufzeitdiskette vor und setzt local/device/config aus dem passenden Profil, falls vorhanden.",
  'ESP][ device configuration — choose profile<context="task parameter"/>':
    "ESP][-Gerätekonfiguration — Profil wählen",
  'ESP][ device configuration — edit values<context="task parameter"/>':
    "ESP][-Gerätekonfiguration — Werte bearbeiten",
  'Load profile into form<context="button text"/>': "Profil in Formular laden",
  'Custom (blank / local fields)<context="task parameter"/>':
    "Benutzerdefiniert (leer / lokale Felder)",
  'Selecting a named preset (e.g. galaxian-demo) fills the next form from that profile. Nothing is uploaded yet.<context="task parameter"/>':
    "Ein benanntes Preset (z. B. galaxian-demo) füllt das nächste Formular aus diesem Profil. Noch nichts wird hochgeladen.",
  'Two-step form: choose a preset (fills fields), then edit/save locally or Apply to device. Opening the form uploads nothing; media bytes are never uploaded here.<context="µDescription"/>':
    "Zweistufiges Formular: Preset wählen (füllt Felder), dann bearbeiten/lokal speichern oder auf Gerät anwenden. Formular öffnen lädt nichts hoch; Media-Bytes nie hier.",
  'Pick galaxian-demo (or another preset) first — the next form shows those values. Apply is explicit.<context="µTooltip"/>':
    "Zuerst galaxian-demo (oder anderes Preset) wählen — das nächste Formular zeigt diese Werte. Anwenden ist ausdrücklich.",
  'List Serial Ports V<version/><context="µDisplayName"/>': "Serielle Ports auflisten V<version/>",
  'Serial Monitor V<version/><context="µDisplayName"/>': "Seriellmonitor V<version/>",
  'Format Check V<version/><context="µDisplayName"/>': "Formatprüfung V<version/>",
  'Reports clang-format availability for project-owned C/C++ (.clang-format). Does not rewrite files.<context="µDescription"/>':
    "Meldet die Verfügbarkeit von clang-format für projekteigene C/C++ (.clang-format). Schreibt keine Dateien um.",
  'Rebuild i18x Dictionaries V<version/><context="µDisplayName"/>':
    "i18x-Wörterbücher neu erzeugen V<version/>",
  'Regenerates i18x/gulp dictionaries and extracts release en-US source strings. Reports missing de-DE — does not AI-translate.<context="µDescription"/>':
    "Erzeugt i18x/gulp-Wörterbücher neu und extrahiert Release-en-US-Quellen. Meldet fehlende de-DE — keine KI-Übersetzung.",
  'Regenerates i18x/gulp en-US and de-DE dictionaries from build-i18x-gulp.mjs.<context="µDescription"/>':
    "Erzeugt i18x/gulp en-US- und de-DE-Wörterbücher aus build-i18x-gulp.mjs neu.",

  // Docs / Git / Backup
  'Compose READMEs V<version/><context="µDisplayName"/>': "READMEs erzeugen V<version/>",
  'Compose README (en-US) V<version/><context="µDisplayName"/>': "README erzeugen (en-US) V<version/>",
  'Compose README (de-DE) V<version/><context="µDisplayName"/>': "README erzeugen (de-DE) V<version/>",
  'Generates one bilingual README.md (English then German, #deutsch) plus locale baselines from .src.md sources. Does not overwrite sources.<context="µDescription"/>':
    "Erzeugt eine zweisprachige README.md (Englisch dann Deutsch, #deutsch) sowie Locale-Baselines aus .src.md-Quellen. Überschreibt die Quellen nicht.",
  'Refreshes locale baselines and the bilingual README.md (same as docs).<context="µDescription"/>':
    "Aktualisiert Locale-Baselines und die zweisprachige README.md (wie docs).",
  'Update Release History (up to date) V<version/><context="µDisplayName"/>':
    "Release-Historie aktualisieren (aktuell) V<version/>",
  'Update Release History — pending V<version/><context="µDisplayName"/>':
    "Release-Historie aktualisieren — ausstehend V<version/>",
  'View Release History V<version/><context="µDisplayName"/>':
    "Release-Historie anzeigen V<version/>",
  'Release context CHECK V<version/><context="µDisplayName"/>':
    "Release-Kontext CHECK V<version/>",
  'Release context FIX V<version/><context="µDisplayName"/>':
    "Release-Kontext FIX V<version/>",
  'Release i18x update V<version/><context="µDisplayName"/>':
    "Release-i18x aktualisieren V<version/>",
  'Merges contributor notes into RELEASES.json, normalizes release-info context tags, validates, and updates release i18x sources. Does not AI-translate.<context="µDescription"/>':
    "Übernimmt Contributor-Notizen in RELEASES.json, normalisiert release-info-Kontext-Tags, prüft und aktualisiert Release-i18x-Quellen. Keine KI-Übersetzung.",
  'Merges fresh contributor notes from dev/releases/*.json into RELEASES.json (30-day window, fingerprint duplicates, release-info context tags). Does not auto-translate.<context="µDescription"/>':
    "Übernimmt frische Contributor-Notizen aus dev/releases/*.json in RELEASES.json (30-Tage-Fenster, Fingerprint-Duplikate, release-info-Kontext-Tags). Übersetzt nicht automatisch.",
  'ACTION_AVAILABLE when unmerged contributor notes exist — not a build failure. Safe to re-run (idempotent).<context="µTooltip"/>':
    "ACTION_AVAILABLE wenn unvermergte Contributor-Notizen existieren — kein Build-Fehler. Erneutes Ausführen ist sicher (idempotent).",
  'Unmerged contributor release notes available.<context="µAttentionTooltip"/>':
    "Unvermergte Contributor-Release-Notizen verfügbar.",
  'Shows localized ESP][ release history from RELEASES.json (date, version, info lines). Does not dump raw JSON.<context="µDescription"/>':
    "Zeigt lokalisierte ESP][-Release-Historie aus RELEASES.json (Datum, Version, Info-Zeilen). Kein Roh-JSON-Dump.",
  'Read-only history view. Translations: i18x/gulp/releases/{en-US,de-DE}.json.<context="µTooltip"/>':
    "Nur-Lesen-Historie. Übersetzungen: i18x/gulp/releases/{en-US,de-DE}.json.",
  'Read-only: verifies release-info context tags on translatable strings; flags machine-data tags.<context="µDescription"/>':
    "Nur Lesen: prüft release-info-Kontext-Tags auf übersetzbaren Strings; markiert Machine-Data-Tags.",
  'Normalizes RELEASES.json release-info context tags (idempotent). Does not auto-translate.<context="µDescription"/>':
    "Normalisiert RELEASES.json release-info-Kontext-Tags (idempotent). Übersetzt nicht automatisch.",
  'Extracts en-US release strings and preserves existing de-DE translations. Reports missing German strings — does not invent AI translations.<context="µDescription"/>':
    "Extrahiert en-US-Release-Strings und bewahrt vorhandene de-DE-Übersetzungen. Meldet fehlende deutsche Strings — erfindet keine KI-Übersetzungen.",
  'Configure ESP][ V<version/><context="µDisplayName"/>':
    "ESP][ konfigurieren V<version/>",
  'Configure ESP][ — Galaxian demo V<version/><context="µDisplayName"/>':
    "ESP][ konfigurieren — Galaxian-Demo V<version/>",
  'Run Macro V<version/><context="µDisplayName"/>':
    "Makro ausführen V<version/>",
  'Interactive ESP][ configuration form (ROM, disks, boot, macro, presentation, screensaver). Save locally or explicitly Apply to device — opening the form uploads nothing.<context="µDescription"/>':
    "Interaktives ESP][-Konfigurationsformular (ROM, Disks, Boot, Makro, Präsentation, Bildschirmschoner). Lokal speichern oder ausdrücklich auf das Gerät anwenden — Formular öffnen lädt nichts hoch.",
  'Form only until you choose Apply. Media bytes are never uploaded here.<context="µTooltip"/>':
    "Nur Formular, bis Sie „Anwenden“ wählen. Media-Bytes werden hier nie hochgeladen.",
  'Execute a named input macro on a live ESP][ (#ESP2MACRO RUN). Asks for COM port like flash/upload.<context="µDescription"/>':
    "Führt ein benanntes Eingabe-Makro auf einem laufenden ESP][ aus (#ESP2MACRO RUN). Fragt nach COM-Port wie flash/upload.",
  'Set ESP2_PORT=COMx to skip the port dialog.<context="µTooltip"/>':
    "ESP2_PORT=COMx setzen, um den Port-Dialog zu überspringen.",
  'ESP][ device configuration<context="task parameter"/>': "ESP][-Gerätekonfiguration",
  'Continue<context="button text"/>': "Weiter",
  'Preset profile<context="task parameter"/>': "Preset-Profil",
  'Named presets under config/device/profiles/. Does not upload. Enable “Reload from preset” to replace field values from that file on Continue.<context="task parameter"/>':
    "Benannte Presets unter config/device/profiles/. Lädt nicht hoch. „Preset neu laden“ ersetzt die Feldwerte aus der Datei bei Weiter.",
  'Custom (use fields below)<context="task parameter"/>': "Benutzerdefiniert (Felder unten)",
  'Reload from preset on Continue<context="task parameter"/>': "Preset bei Weiter neu laden",
  'When on, load the selected preset file (ignores field edits for this run). When off, use the editable fields below.<context="task parameter"/>':
    "Ein: gewählte Preset-Datei laden (Feldänderungen für diesen Lauf ignorieren). Aus: bearbeitbare Felder unten verwenden.",
  'Profile name (local save)<context="task parameter"/>': "Profilname (lokales Speichern)",
  'Saved under local/device/config/ and optionally config/device/profiles/<name>/.<context="task parameter"/>':
    "Gespeichert unter local/device/config/ und optional config/device/profiles/<name>/.",
  'System ROM (device path)<context="task parameter"/>': "System-ROM (Gerätepfad)",
  'Absolute path on the ESP][ SD, e.g. /esp2/roms/system.rom — not a host file upload.<context="task parameter"/>':
    "Absoluter Pfad auf der ESP][-SD, z. B. /esp2/roms/system.rom — kein Host-Datei-Upload.",
  'Drive 1 image (device path)<context="task parameter"/>': "Laufwerk-1-Abbild (Gerätepfad)",
  'e.g. /esp2/disks/Galaxian.dsk — path only; disk bytes are not uploaded by this form.<context="task parameter"/>':
    "z. B. /esp2/disks/Galaxian.dsk — nur Pfad; Disketten-Bytes lädt dieses Formular nicht hoch.",
  'Drive 2 image (optional)<context="task parameter"/>': "Laufwerk-2-Abbild (optional)",
  'Leave empty when unused. Path under /esp2/ only.<context="task parameter"/>':
    "Leer lassen wenn unbenutzt. Nur Pfade unter /esp2/.",
  'Boot from disk (Autostart)<context="task parameter"/>': "Von Diskette booten (Autostart)",
  'Startup macro<context="task parameter"/>': "Startup-Makro",
  'none<context="task parameter"/>': "keines",
  'Screen orientation<context="task parameter"/>': "Bildschirmausrichtung",
  'Monitor appearance<context="task parameter"/>': "Monitor-Erscheinungsbild",
  'White / Monochrome<context="task parameter"/>': "Weiß / Monochrom",
  'Green phosphor<context="task parameter"/>': "Grünphosphor",
  'Amber phosphor<context="task parameter"/>': "Bernsteinphosphor",
  'Display effect<context="task parameter"/>': "Display-Effekt",
  'Clean<context="task parameter"/>': "Clean",
  'CRT / TV<context="task parameter"/>': "CRT / TV",
  'Classic<context="task parameter"/>': "Klassisch",
  'Landscape<context="task parameter"/>': "Querformat",
  'HGR presentation<context="task parameter"/>': "HGR-Darstellung",
  'Sharp<context="task parameter"/>': "Scharf",
  'Artifact Color<context="task parameter"/>': "Artefaktfarbe",
  'Screensaver timeout (seconds, 0 = disabled)<context="task parameter"/>':
    "Bildschirmschoner-Timeout (Sekunden, 0 = aus)",
  'Idle seconds before AMOLED screensaver. 0 disables. Unit: seconds.<context="task parameter"/>':
    "Idle-Sekunden bis AMOLED-Bildschirmschoner. 0 = aus. Einheit: Sekunden.",
  '▶ Press any button on the desired pad…<context="task parameter"/>':
    "▶ Beliebige Taste am gewünschten Pad drücken…",
  'Choose a connected pad, or “Press any button…” after Continue. 8BitDo must be in X-input mode. Dial has no press for identify. Asleep slots stay selectable.<context="task parameter"/>':
    "Verbundenes Pad wählen oder nach Weiter „Taste drücken…“. 8BitDo muss im X-Input-Modus sein. Dial-Druck zählt nicht zur Erkennung. Schlafende Slots bleiben wählbar.",
  'Choose a connected pad, or “Press any button…” — after Continue, press a button; the pad rumble-confirms. Offline pads wake on press.<context="task parameter"/>':
    "Verbundenes Pad wählen oder „Taste drücken…“ — nach Weiter Taste drücken; das Pad bestätigt per Rumble. Offline-Pads wachen beim Druck auf.",
  'Preset → edit profile (host gamepad: pick slot or press a button to identify) → save. Opening uploads nothing; media bytes are never uploaded here.<context="µDescription"/>':
    "Preset → Profil bearbeiten (Host-Gamepad: Slot wählen oder Taste drücken) → speichern. Öffnen lädt nichts hoch; Media-Bytes werden hier nie hochgeladen.",
  'Host gamepad is in the edit form. “Press any button…” rumble-identifies the controller after Continue.<context="µTooltip"/>':
    "Host-Gamepad steht im Edit-Formular. „Taste drücken…“ identifiziert den Controller nach Weiter per Rumble.",
  'No gamepad button press detected — wake/reconnect the pad or pick a slot manually.<context="task error"/>':
    "Kein Gamepad-Tastendruck erkannt — Pad wecken/neu verbinden oder Slot manuell wählen.",
  'Live XInput pads. Prefer “Press a button…” in the previous step to identify which controller is which.<context="task parameter"/>':
    "Live-XInput-Pads. Im vorherigen Schritt „Taste drücken…“ wählen, um den Controller zu identifizieren.",
  'Host gamepad (Windows bridge)<context="task parameter"/>':
    "Host-Gamepad (Windows-Bridge)",
  'XInput slot for device:input-bridge. auto = first connected. Firmware ignores this; host-only.<context="task parameter"/>':
    "XInput-Slot für device:input-bridge. auto = erstes verbundenes Pad. Firmware ignoriert das; nur Host.",
  'None<context="task parameter"/>': "Keines",
  'Auto (first connected)<context="task parameter"/>': "Auto (erstes verbundenes)",
  'XInput #0<context="task parameter"/>': "XInput #0",
  'XInput #1<context="task parameter"/>': "XInput #1",
  'XInput #2<context="task parameter"/>': "XInput #2",
  'XInput #3<context="task parameter"/>': "XInput #3",
  'Gamepad name filter (optional)<context="task parameter"/>':
    "Gamepad-Namensfilter (optional)",
  'Regex against XInput name/id (e.g. 8BitDo). Overrides slot when set.<context="task parameter"/>':
    "Regex gegen XInput-Name/ID (z. B. 8BitDo). Überschreibt den Slot, wenn gesetzt.",
  'PDL0 source (host bridge)<context="task parameter"/>': "PDL0-Quelle (Host-Bridge)",
  'PDL1 source (host bridge)<context="task parameter"/>': "PDL1-Quelle (Host-Bridge)",
  'PB0 source (host bridge)<context="task parameter"/>': "PB0-Quelle (Host-Bridge)",
  'PB0 button mapping (not pad identity)<context="task parameter"/>':
    "PB0-Tastenmapping (nicht Pad-Identität)",
  'Which button becomes Apple II PB0. MX Dial has no physical press switch — use gamepad A (or keyboard).<context="task parameter"/>':
    "Welche Taste Apple-II-PB0 wird. MX Dial hat keinen Druckschalter — Gamepad A (oder Tastatur) nutzen.",
  'Which physical button becomes Apple II PB0 — not which controller. Pick the controller above.<context="task parameter"/>':
    "Welche physische Taste Apple-II-PB0 wird — nicht welcher Controller. Controller oben wählen.",
  'A / Cross button<context="task parameter"/>': "A- / Cross-Taste",
  'Enable Logitech MX Dial (host bridge)<context="task parameter"/>':
    "Logitech MX Dial aktivieren (Host-Bridge)",
  'Rotation only → absolute PDL0 (no press switch on MX Dial). Options+ must not steal the wheel path.<context="task parameter"/>':
    "Nur Drehung → absolut PDL0 (kein Druckschalter am MX Dial). Options+ darf den Rad-Pfad nicht abfangen.",
  'Enable Logitech Dial (host bridge)<context="task parameter"/>':
    "Logitech Dial aktivieren (Host-Bridge)",
  'Relative crown → absolute PDL0 when PDL0 is dial/auto. Options+ may steal volume.<context="task parameter"/>':
    "Relative Krone → absolut PDL0 wenn PDL0=dial/auto. Options+ kann die Lautstärke abfangen.",
  'Dial press OR A button<context="task parameter"/>': "Dial-Druck ODER A-Taste",
  'Dial press<context="task parameter"/>': "Dial-Druck",
  'Dial press OR gamepad A<context="task parameter"/>': "Dial-Druck ODER Gamepad A",
  'ESP][ — which host gamepad?<context="task parameter"/>':
    "ESP][ — welches Host-Gamepad?",
  'Gamepad for this profile<context="task parameter"/>': "Gamepad für dieses Profil",
  'Connected controllers are listed by name. Choose “Press any button…” to identify by touch; the pad will rumble when detected.<context="task parameter"/>':
    "Verbundene Controller stehen mit Namen. „Taste drücken…“ identifiziert per Tastendruck; das Pad vibriert bei Erkennung.",
  'Keep current profile value<context="task parameter"/>': "Aktuellen Profilwert behalten",
  'Press any button on the desired pad (identify)<context="task parameter"/>':
    "Beliebige Taste am gewünschten Pad drücken (identifizieren)",
  'Host gamepad <index/>: <label/><context="task log"/>':
    "Host-Gamepad <index/>: <label/>",
  'Press any button on the gamepad you want for this profile (20s)…<context="task log"/>':
    "Beliebige Taste am gewünschten Gamepad für dieses Profil drücken (20 s)…",
  'Detected gamepad slot <index/> (<label/>) — rumble pulsed.<context="task log"/>':
    "Gamepad-Slot <index/> erkannt (<label/>) — Rumble ausgelöst.",
  'No gamepad button press detected — reconnect the pad or pick a slot manually.<context="task error"/>':
    "Kein Gamepad-Tastendruck erkannt — Pad neu verbinden oder Slot manuell wählen.",
  'Preset → pick host gamepad (press a button to identify) → edit/save. Opening uploads nothing; media bytes are never uploaded here.<context="µDescription"/>':
    "Preset → Host-Gamepad wählen (Taste drücken zum Identifizieren) → bearbeiten/speichern. Öffnen lädt nichts hoch; Media-Bytes werden hier nie hochgeladen.",
  'Connected pads are listed by name. “Press any button…” rumble-identifies the controller for the profile.<context="µTooltip"/>':
    "Verbundene Pads stehen mit Namen. „Taste drücken…“ identifiziert den Controller für das Profil per Rumble.",
  'Auto<context="task parameter"/>': "Auto",
  'Gamepad left stick X<context="task parameter"/>': "Gamepad linker Stick X",
  'Gamepad left stick Y<context="task parameter"/>': "Gamepad linker Stick Y",
  'Logitech Dial<context="task parameter"/>': "Logitech Dial",
  'Gamepad A<context="task parameter"/>': "Gamepad A",
  'Dial press<context="task parameter"/>': "Dial-Druck",
  'Dial press OR gamepad A<context="task parameter"/>': "Dial-Druck ODER Gamepad A",
  'Enable Logitech Dial (host bridge)<context="task parameter"/>':
    "Logitech Dial aktivieren (Host-Bridge)",
  'Relative crown → absolute PDL0 when PDL0 is dial/auto. Options+ may steal volume.<context="task parameter"/>':
    "Relative Krone → absolut PDL0 wenn PDL0=dial/auto. Options+ kann die Lautstärke abfangen.",
  'Set ESP2_PORT=COMx to skip the port dialog. Free COM5 if another monitor holds it.<context="µTooltip"/>':
    "ESP2_PORT=COMx setzen, um den Port-Dialog zu überspringen. COM mit device:port:release freigeben, wenn etwas den Port hält.",
  'Release COM Port V<version/><context="µDisplayName"/>':
    "COM-Port freigeben V<version/>",
  'Stops ESP][ host processes that lock the USB CDC port (input bridge, PlatformIO monitor, upload helpers) and probes that the port opens again.<context="µDescription"/>':
    "Beendet ESP][-Host-Prozesse, die den USB-CDC-Port sperren (Input-Bridge, PlatformIO-Monitor, Upload-Helfer), und prüft, ob der Port wieder öffnet.",
  'Use when flash/monitor/input-bridge says Access denied. Set ESP2_PORT=COMx or pick the port.<context="µTooltip"/>':
    "Nutzen bei Access denied von Flash/Monitor/Input-Bridge. ESP2_PORT=COMx setzen oder Port wählen.",
  'Serial port (optional)<context="task parameter"/>': "Serieller Port (optional)",
  'e.g. COM5. Empty → ESP2_PORT or AskPort. Probe verifies this port is free after kill.<context="task parameter"/>':
    "z. B. COM5. Leer → ESP2_PORT oder AskPort. Probe prüft nach dem Kill, ob der Port frei ist.",
  'Dry run (list only)<context="task parameter"/>': "Dry-Run (nur auflisten)",
  'Releasing COM holders (input-bridge / PIO monitor / upload)… port=<port/><context="task log"/>':
    "COM-Halter werden freigegeben (Input-Bridge / PIO-Monitor / Upload)… Port=<port/>",
  'Stopped <count/> holder process(es).<context="task log"/>':
    "<count/> Halter-Prozess(e) beendet.",
  'No matching holder processes were running.<context="task log"/>':
    "Keine passenden Halter-Prozesse liefen.",
  'Port <port/> is free.<context="task log"/>': "Port <port/> ist frei.",
  'Press any button NOW on the desired pad (25s). Wake Bluetooth pads first. Dial press is not used here.<context="task log"/>':
    "JETZT beliebige Taste am gewünschten Pad drücken (25 s). Bluetooth-Pads zuerst wecken. Dial-Druck zählt hier nicht.",
  'No gamepad button press detected — wake the pad, use X-input mode (8BitDo), or pick slot #0/#1/#2/#3 manually.<context="task error"/>':
    "Kein Gamepad-Tastendruck erkannt — Pad wecken, X-Input-Modus (8BitDo) oder Slot #0/#1/#2/#3 manuell wählen.",
  'PnP gamepad-like device: <name/><context="task log"/>':
    "PnP-Gamepad-ähnliches Gerät: <name/>",
  'RawGameController <index/>: <name/><context="task log"/>':
    "RawGameController <index/>: <name/>",
  'Detected gamepad slot <index/> (<label/>, api=<api/>) — rumble pulsed.<context="task log"/>':
    "Gamepad-Slot <index/> erkannt (<label/>, api=<api/>) — Rumble ausgelöst.",
  'Press any button on the gamepad you want for this profile (20s)…<context="task log"/>':
    "Beliebige Taste am gewünschten Gamepad für dieses Profil drücken (20 s)…",
  'No gamepad button press detected — wake/reconnect the pad or pick a slot manually.<context="task error"/>':
    "Kein Gamepad-Tastendruck erkannt — Pad wecken/neu verbinden oder Slot manuell wählen.",
  'Detected gamepad slot <index/> (<label/>) — rumble pulsed.<context="task log"/>':
    "Gamepad-Slot <index/> erkannt (<label/>) — Rumble ausgelöst.",
  'Input Bridge V<version/><context="µDisplayName"/>': "Eingabe-Bridge V<version/>",
  'Opens a Windows console for the CDC input bridge (keyboard + XInput gamepad + optional Dial). Reads input.hostBridge from the chosen profile or local/device/config.<context="µDescription"/>':
    "Öffnet eine Windows-Konsole für die CDC-Eingabe-Bridge (Tastatur + XInput-Gamepad + optional Dial). Liest input.hostBridge aus dem gewählten Profil oder local/device/config.",
  'Set ESP2_PORT=COMx. Pad must be in X-input mode. Profile galaxian-demo or little-brick-out; empty profile uses local config (gamepad may be none).<context="µTooltip"/>':
    "ESP2_PORT=COMx setzen. Pad muss im X-Input-Modus sein. Profil galaxian-demo oder little-brick-out; leeres Profil nutzt lokale Config (Gamepad kann none sein).",
  'Profile id (hostBridge)<context="task parameter"/>': "Profil-ID (hostBridge)",
  'config/device/profiles/<id>/ — e.g. galaxian-demo or little-brick-out. Empty uses local/device/config (check gamepad is not none).<context="task parameter"/>':
    "config/device/profiles/<id>/ — z. B. galaxian-demo oder little-brick-out. Leer nutzt local/device/config (prüfen, dass gamepad nicht none ist).",
  'Diagnostic pad/key log<context="task parameter"/>': "Diagnose-Pad/Tasten-Log",
  'Opening Input Bridge console on <port/> (profile=<profile/>). Focus that window for keyboard; gamepads need XInput (not D-input).<context="task log"/>':
    "Eingabe-Bridge-Konsole auf <port/> (Profil=<profile/>). Dieses Fenster für Tastatur fokussieren; Gamepads brauchen XInput (nicht D-Input).",
  'No profile set — local/device/config/system.json is used. If gamepad is "none" there, pads stay disabled.<context="task warning"/>':
    "Kein Profil gesetzt — local/device/config/system.json wird genutzt. Steht dort gamepad auf \"none\", bleiben Pads deaktiviert.",
  'Bridge runs in a separate console. Close that window (or device:port:release) to stop.<context="task log"/>':
    "Bridge läuft in einer eigenen Konsole. Fenster schließen (oder device:port:release) zum Beenden.",
  'Starting Windows input bridge on <port/> (profile=<profile/>). Gamepad uses XInput; keep this terminal focused for keyboard.<context="task log"/>':
    "Windows-Eingabe-Bridge startet auf <port/> (Profil=<profile/>). Gamepad nutzt XInput; Terminal für Tastatur fokussiert lassen.",
  'config/device/profiles/<id>/system.json — empty uses local/device/config when present.<context="task parameter"/>':
    "config/device/profiles/<id>/system.json — leer nutzt local/device/config falls vorhanden.",
  'Windows CDC input bridge: keyboard + XInput gamepad (+ optional Dial). Reads gamepad selection from the device profile / local system.json input.hostBridge.<context="µDescription"/>':
    "Windows-CDC-Eingabe-Bridge: Tastatur + XInput-Gamepad (+ optional Dial). Liest die Gamepad-Auswahl aus dem Geräteprofil / local system.json input.hostBridge.",
  'Set ESP2_PORT=COMx. Use device:config to pick which XInput gamepad belongs to the profile.<context="µTooltip"/>':
    "ESP2_PORT=COMx setzen. Mit device:config wählen, welches XInput-Gamepad zum Profil gehört.",
  'Action<context="task parameter"/>': "Aktion",
  'Save locally writes JSON on the PC only. Apply to device uploads system.json + macros.json (paths only) — never ROM/disk media.<context="task parameter"/>':
    "Lokal speichern schreibt nur JSON auf dem PC. Auf Gerät anwenden lädt system.json + macros.json (nur Pfade) — nie ROM/Disk-Media.",
  'Save locally (no device)<context="task parameter"/>': "Lokal speichern (kein Gerät)",
  'Apply to device (upload config)<context="task parameter"/>': "Auf Gerät anwenden (Config hochladen)",
  'Serial port for Apply (optional)<context="task parameter"/>': "Serieller Port für Anwenden (optional)",
  'e.g. COM5. Empty → AskPort dialog. Used only when Action is Apply to device.<context="task parameter"/>':
    "z. B. COM5. Leer → AskPort-Dialog. Nur bei Aktion „Auf Gerät anwenden“.",
  'Macro id<context="task parameter"/>': "Makro-ID",
  'Saved device config locally → <path/> (profile=<profile/>). ROM/disk media were NOT uploaded.<context="task log"/>':
    "Geräte-Config lokal gespeichert → <path/> (Profil=<profile/>). ROM/Disk-Media wurden NICHT hochgeladen.",
  'Applying config to device via <port/> (system.json + macros.json only).<context="task log"/>':
    "Config wird über <port/> auf das Gerät angewendet (nur system.json + macros.json).",
  'Device config applied. Power-cycle ESP][ to load. Media images were not uploaded.<context="task log"/>':
    "Geräte-Config angewendet. ESP][ neu starten zum Laden. Media-Abbilder wurden nicht hochgeladen.",
  'Action=Save locally — no COM port, no upload, no flash.<context="task log"/>':
    "Aktion=Lokal speichern — kein COM-Port, kein Upload, kein Flash.",
  'Running macro <macro/> on <port/><context="task log"/>…':
    "Makro <macro/> auf <port/> wird ausgeführt…",
  'No serial port — set ESP2_PORT=COMx, fill the port field, or pick a port in the dialog.<context="task error"/>':
    "Kein serieller Port — ESP2_PORT=COMx setzen, Port-Feld füllen oder Port im Dialog wählen.",
  'Git backup checkpoint<context="µDisplayName"/>': "Git-Backup-Checkpoint",
  'Git Checkpoint Commit<context="µDisplayName"/>': "Git-Checkpoint-Commit",
  'Git Status V<version/><context="µDisplayName"/>': "Git-Status V<version/>",
  'Shows Git status and would-be checkpoint paths. Never commits or pushes. Respects GIT_BACKUP_NEVER_STAGE / local media exclusions.<context="µDescription"/>':
    "Zeigt Git-Status und geplante Checkpoint-Pfade. Commitet und pusht nie. Beachtet GIT_BACKUP_NEVER_STAGE / lokale Medienausschlüsse.",
  'Git Commit V<version/><context="µDisplayName"/>': "Git-Commit V<version/>",
  'Creates a local Git checkpoint commit (same rules as backup:git) without pushing. Never stages local/apple2, proprietary media, or secrets.<context="µDescription"/>':
    "Erstellt einen lokalen Git-Checkpoint-Commit (gleiche Regeln wie backup:git) ohne Push. Stagt nie local/apple2, proprietäre Medien oder Secrets.",
  'Git Push V<version/><context="µDisplayName"/>': "Git Push V<version/>",
  'Pushes the current branch to its upstream remote. Does not stage or commit.<context="µDescription"/>':
    "Pusht den aktuellen Branch zum Upstream-Remote. Stagt und commitet nicht.",
  'Backup to NAS V<version/><context="µDisplayName"/>': "NAS-Backup V<version/>",
  'Backup to NAS (alias)<context="µDisplayName"/>': "NAS-Backup (Alias)" /* obsolete */,
  'Alias of backup — NAS form / NAS_TARGET_1..3. Includes local/apple2 privately; skips disposable caches.<context="µDescription"/>':
    "Alias von backup — NAS-Formular / NAS_TARGET_1..3. Sichert local/apple2 privat; lässt disposable Caches weg.",
  'Copies ESP][ project trees plus local/downloaded assets (local/apple2, local/roms, _refs, 3dprint) to up to three private NAS folders. Skips node_modules, .pio, and other disposable caches. Gitignore ≠ NAS exclude.<context="µDescription"/>':
    "Kopiert ESP][-Projektbäume plus lokale/heruntergeladene Assets (local/apple2, local/roms, _refs, 3dprint) auf bis zu drei private NAS-Ordner. Lässt node_modules, .pio und andere disposable Caches weg. Gitignore ≠ NAS-Ausschluss.",
  'List NAS Backups V<version/><context="µDisplayName"/>': "NAS-Backups auflisten V<version/>",
  'Shows configured NAS destinations and whether essential restore files are present. Read-only.<context="µDescription"/>':
    "Zeigt konfigurierte NAS-Ziele und ob essenzielle Restore-Dateien vorhanden sind. Nur Lesen.",
  'Verify NAS Backup V<version/><context="µDisplayName"/>': "NAS-Backup prüfen V<version/>",
  'Checks primary NAS destination reachability and essential restore files. Read-only; never restores.<context="µDescription"/>':
    "Prüft Erreichbarkeit des primären NAS-Ziels und essenzielle Restore-Dateien. Nur Lesen; stellt nie wieder her.",
  'Backup all (docs + Git + NAS)<context="µDisplayName"/>':
    "Alles sichern (Docs + Git + NAS)",
  'Save Project (Git + NAS) V<version/><context="µDisplayName"/>':
    "Projekt speichern (Git + NAS) V<version/>",

  // Tests
  'Run Infrastructure Tests V<version/><context="µDisplayName"/>':
    "Infrastruktur-Tests ausführen V<version/>",
  'Runs node:test for NAS/Git/docs infrastructure helpers (no real NAS write / no Git commit).<context="µDescription"/>':
    "Führt node:test für NAS/Git/Docs-Infrastrukturhelfer aus (kein echtes NAS-Schreiben / kein Git-Commit).",
  'Run µGulp Catalog Tests V<version/><context="µDisplayName"/>':
    "µGulp-Katalog-Tests ausführen V<version/>",
  'Regression tests for first-party task catalog, metadata, and UTF-8 integrity.<context="µDescription"/>':
    "Regressionstests für First-Party-Task-Katalog, Metadaten und UTF-8-Integrität.",
  'Run Host Apple II Tests V<version/><context="µDisplayName"/>':
    "Host-Apple-II-Tests ausführen V<version/>",
  'Runs host-side Apple II JS test suite (no proprietary media required).<context="µDescription"/>':
    "Führt die Host-seitige Apple-II-JS-Testsuite aus (keine proprietären Medien nötig).",
  'NAS backup destinations<context="task parameter"/>': "NAS-Backup-Ziele",
  'Start backup<context="button text"/>': "Backup starten",
  'Destination 1 (NAS_TARGET_1)<context="task parameter"/>':
    "Ziel 1 (NAS_TARGET_1)",
  'Destination 2 (NAS_TARGET_2)<context="task parameter"/>':
    "Ziel 2 (NAS_TARGET_2)",
  'Destination 3 (NAS_TARGET_3)<context="task parameter"/>':
    "Ziel 3 (NAS_TARGET_3)",
  'Primary backup folder. Leave empty to skip. Up to three destinations. µGulp remembers answers; optional defaults from config/nas.targets.local.<context="task parameter"/>':
    "Primärer Backup-Ordner. Leer lassen zum Überspringen. Bis zu drei Ziele. µGulp merkt Antworten; optionale Defaults aus config/nas.targets.local.",
  'Second folder when set. Leave empty to skip.<context="task parameter"/>':
    "Zweiter Ordner, falls gesetzt. Leer lassen zum Überspringen.",
  'Third folder when set. Leave empty to skip.<context="task parameter"/>':
    "Dritter Ordner, falls gesetzt. Leer lassen zum Überspringen.",
  'Preview only (robocopy /L)<context="task parameter"/>':
    "Nur Vorschau (robocopy /L)",
  'List differences without copying or deleting.<context="task parameter"/>':
    "Unterschiede auflisten, ohne zu kopieren oder zu löschen.",

  // apple2js / media helpers
  'Sync Online Catalog V<version/><context="µDisplayName"/>': "Online-Katalog synchronisieren V<version/>",
  'Show Online Catalog V<version/><context="µDisplayName"/>': "Online-Katalog anzeigen V<version/>",
  'Audit Online Catalog V<version/><context="µDisplayName"/>': "Online-Katalog prüfen V<version/>",
  'Identify Media File V<version/><context="µDisplayName"/>': "Mediendatei identifizieren V<version/>",
  'Import Local Media V<version/><context="µDisplayName"/>': "Lokale Medien importieren V<version/>",
  'Inspect Media (a2kit) V<version/><context="µDisplayName"/>': "Medium prüfen (a2kit) V<version/>",
  'Prepare SD Library Layout V<version/><context="µDisplayName"/>': "SD-Bibliotheksstruktur vorbereiten V<version/>",

  // Apple II local media
  'Download Free Apple II ROMs V<version/><context="µDisplayName"/>':
    "Freie Apple-II-ROMs herunterladen V<version/>",
  'Download ONLY redistributable ROMs (e.g. AppleIIGo) into gitignored local/apple2/roms/.<context="µDescription"/>':
    "Lädt NUR weiterverbreitbare ROMs (z. B. AppleIIGo) nach gitignored local/apple2/roms/.",
  'Download Apple II Media V<version/><context="µDisplayName"/>':
    "Apple-II-Medien herunterladen V<version/>",
  'Fetch apple2js title(s) into gitignored local/apple2/cache/ (LOCAL_TEST_ONLY; never stages Git).<context="µDescription"/>':
    "Holt apple2js-Titel nach gitignored local/apple2/cache/ (LOCAL_TEST_ONLY; kein Git-Staging).",
  'Prepare Runtime Disk V<version/><context="µDisplayName"/>':
    "Laufzeitdiskette vorbereiten V<version/>",
  'Normalize cached source to local/apple2/disks/ (offline; prefers DSK when lossless).<context="µDescription"/>':
    "Normalisiert Cache-Quellen nach local/apple2/disks/ (offline; bevorzugt DSK wenn verlustfrei).",
  'Show Local Media Status V<version/><context="µDisplayName"/>': "Lokalen Medienstatus anzeigen V<version/>",
  'Compact table of local titles: downloaded / prepared / provenance.<context="µDescription"/>':
    "Kompakte Tabelle lokaler Titel: heruntergeladen / vorbereitet / Provenienz.",
  'Show Local Media V<version/><context="µDisplayName"/>':
    "Lokale Medien anzeigen V<version/>",
  'JSON list of locally cached titles.<context="µDescription"/>':
    "JSON-Liste lokal gecachter Titel.",
  'Verify Media Provenance V<version/><context="µDisplayName"/>':
    "Medien-Provenienz prüfen V<version/>",
  'Provenance counts for local cache (download never upgrades rights).<context="µDescription"/>':
    "Provenienz-Zähler für den lokalen Cache (Download wertet Rechte nie auf).",
  'Clean Generated Disks V<version/><context="µDisplayName"/>':
    "Erzeugte Disketten bereinigen V<version/>",
  'Default: remove generated disks/. Optional --cache / --user (never deletes user media by default).<context="µDescription"/>':
    "Standard: erzeugte disks/ entfernen. Optional --cache / --user (löscht Nutzer-Medien standardmäßig nie).",
  'Sync Game to ESP][ V<version/><context="µDisplayName"/>':
    "Spiel auf ESP][ synchronisieren V<version/>",
  'Upload ONLY selected title runtime ROM+disk via serial ESPU (not full catalog).<context="µDescription"/>':
    "Lädt NUR ROM+Diskette des gewählten Titels per serieller ESPU hoch (nicht den ganzen Katalog).",

  // Emulator / compat
  'Identify Apple II ROM V<version/><context="µDisplayName"/>': "Apple-II-ROM identifizieren V<version/>",
  'Test Apple II ROM on Host V<version/><context="µDisplayName"/>':
    "Apple-II-ROM auf dem Host testen V<version/>",
  'Run Apple II Host Emulator V<version/><context="µDisplayName"/>':
    "Apple-II-Host-Emulator starten V<version/>",
  'Test Apple II Disk Boot on Host V<version/><context="µDisplayName"/>':
    "Apple-II-Diskettenboot auf dem Host testen V<version/>",
  'Run Compatibility Test V<version/><context="µDisplayName"/>':
    "Kompatibilitätstest ausführen V<version/>",
  'Run real-software compatibility test definitions; SKIPPED_* without user assets. No downloads.<context="µDescription"/>':
    "Führt Kompatibilitätstests mit echter Software aus; SKIPPED_* ohne Nutzer-Assets. Keine Downloads.",

  // Legacy display names (pre-rename, keep for dictionary completeness)
  'Apple II ROM Sync<context="µDisplayName"/>': "Apple-II-ROM synchronisieren",
  'Apple II Media Sync<context="µDisplayName"/>':
    "Apple-II-Medien synchronisieren",
  'Apple II Media Prepare<context="µDisplayName"/>':
    "Apple-II-Medien vorbereiten",
  'Apple II Media List<context="µDisplayName"/>': "Apple-II-Medien auflisten",
  'Apple II Media Audit<context="µDisplayName"/>': "Apple-II-Medien prüfen",
  'Apple II Media Clean<context="µDisplayName"/>': "Apple-II-Medien bereinigen",
  'Apple II Device Sync<context="µDisplayName"/>':
    "Apple-II-Gerät synchronisieren",
  'Apple II ROM Identify<context="µDisplayName"/>':
    "Apple-II-ROM identifizieren",
  'Apple II ROM Test<context="µDisplayName"/>': "Apple-II-ROM-Test",
  'Apple II Host Runner<context="µDisplayName"/>': "Apple-II-Host-Runner",
  'Apple II Disk Test<context="µDisplayName"/>': "Apple-II-Disketten-Test",
  'Apple II Compatibility Harness<context="µDisplayName"/>':
    "Apple-II-Kompatibilitätsharness",

  // Device storage
  'Transfer File V<version/><context="µDisplayName"/>': "Datei übertragen V<version/>",
  'USB Storage Mode V<version/><context="µDisplayName"/>': "USB-Speichermodus V<version/>",
  'SD card on computer V<version/><context="µDisplayName"/>': "SD-Karte am Computer V<version/>",
  'Make the ESP][ microSD available to Windows/macOS (USB Storage), or return it to ESP][. While on the computer, Apple II Disk II cannot use the SD.<context="µDescription"/>':
    "Stellt die ESP][-microSD Windows/macOS zur Verfügung (USB-Speicher) oder gibt sie an ESP][ zurück. Am Computer kann Disk II die SD nicht nutzen.",
  'Uses the same USB Storage ownership path as SD backup/restore. Requires TinyUSB MSC on device (USB_MODE=0).<context="µTooltip"/>':
    "Nutzt denselben USB-Speicher-Ownership-Pfad wie SD-Backup/Restore. Erfordert TinyUSB-MSC am Gerät (USB_MODE=0).",
  'SD card on computer<context="task parameter"/>': "SD-Karte am Computer",
  'Start<context="button text"/>': "Start",
  'Make available to computer<context="task parameter"/>': "Am Computer verfügbar machen",
  'Return to ESP][<context="task parameter"/>': "An ESP][ zurückgeben",
  'Leave empty to use ESP2_PORT or the normal port picker.<context="task parameter"/>':
    "Leer lassen für ESP2_PORT oder den normalen Port-Dialog.",
  'Open in Explorer / Finder after mounting<context="task parameter"/>':
    "Nach dem Mounten in Explorer / Finder öffnen",
  'USB connection behavior<context="task parameter"/>': "USB-Verbindungsverhalten",
  'Normal keeps the SD on ESP][ when a PC is connected. Auto-mount exposes the SD via USB Storage only when a real USB data host enumerates — never on power-only adapters.<context="task parameter"/>':
    "Normal lässt die SD bei PC-Anschluss bei ESP][. Auto-Mount gibt die SD nur frei, wenn ein echter USB-Datenhost enumeriert — nie bei reinen Netzteilen.",
  'Normal — ESP][ keeps SD<context="task parameter"/>': "Normal — ESP][ behält die SD",
  'Auto-mount SD on computer<context="task parameter"/>': "SD automatisch am Computer mounten",
  'Returning SD to ESP][ on <port/>…<context="task log"/>':
    "Gebe SD an ESP][ auf <port/> zurück…",
  'If the SD is still visible in Explorer/Finder, eject it first.<context="task log"/>':
    "Wenn die SD noch in Explorer/Finder sichtbar ist, zuerst auswerfen.",
  'SD is on the computer at <path/>. Disk II/runtime SD access is unavailable until returned.<context="task log"/>':
    "SD ist am Computer unter <path/>. Disk-II-/Runtime-SD-Zugriff erst nach Rückgabe wieder möglich.",
  'When finished: eject in Explorer/Finder, then run this task with “Return to ESP][”.<context="task log"/>':
    "Danach: in Explorer/Finder auswerfen, dann diese Aufgabe mit „An ESP][ zurückgeben“ ausführen.",
  'SD owned by ESP][ — switching to computer…<context="task log"/>':
    "SD gehört ESP][ — Wechsel zum Computer…",
  'Waiting for Windows/macOS to mount the ESP][ SD…<context="task log"/>':
    "Warte, bis Windows/macOS die ESP][-SD mountet…",
  'SD mounted on computer: <info/><context="task log"/>':
    "SD am Computer gemountet: <info/>",
  'Opened file manager.<context="task log"/>': "Dateimanager geöffnet.",
  'Could not open file manager: <err/><context="task log"/>':
    "Dateimanager konnte nicht geöffnet werden: <err/>",
  'Returning SD to ESP][…<context="task log"/>': "Gebe SD an ESP][ zurück…",
  'SD returned to ESP][.<context="task log"/>': "SD an ESP][ zurückgegeben.",
  'Timed out waiting for the ESP][ SD to appear after USB Storage. MSC requires TinyUSB (USB_MODE=0) on device, or use Advanced card-reader fallback for backup/restore.<context="task error"/>':
    "Zeitüberschreitung nach USB-Speicher. MSC erfordert TinyUSB (USB_MODE=0) am Gerät, oder erweiterten Kartenleser-Fallback für Backup/Restore nutzen.",
  'Back up SD card V<version/><context="µDisplayName"/>': "SD-Karte sichern V<version/>",
  'Read-only logical backup of the SD currently in the connected ESP][ (USB Storage → hash-verified copy under local/sd-backups). Not a raw card image.<context="µDescription"/>':
    "Schreibgeschützte logische Sicherung der SD im verbundenen ESP][ (USB-Speicher → hash-verifizierte Kopie unter local/sd-backups). Kein rohes Kartenabbild.",
  'Select ESP][ COM port if needed. Destination defaults to local/sd-backups/. Advanced card-reader path available if MSC auto-detect fails.<context="µTooltip"/>':
    "Bei Bedarf ESP][-COM-Port wählen. Zielstandard: local/sd-backups/. Erweiterter Kartenleser-Pfad, falls MSC-Autoerkennung scheitert.",
  'Restore SD card V<version/><context="µDisplayName"/>':
    "SD-Karte wiederherstellen V<version/>",
  'Restores a verified backup onto the SD currently in the connected ESP][ (USB Storage). Card sizes may differ if data fits. Requires confirmation after the destination is detected.<context="µDescription"/>':
    "Stellt ein verifiziertes Backup auf die SD im verbundenen ESP][ wieder her (USB-Speicher). Kartengrößen dürfen sich unterscheiden, wenn die Daten passen. Bestätigung nach Zielerkennung erforderlich.",
  'Pick a backup under local/sd-backups/. Destination mount is auto-detected via USB Storage — never guessed by card size alone.<context="µTooltip"/>':
    "Backup unter local/sd-backups/ wählen. Ziel-Mount wird per USB-Speicher erkannt — nie allein nach Kartengröße geraten.",
  'Back up ESP][ SD card<context="task parameter"/>': "ESP][-SD-Karte sichern",
  'Start backup<context="button text"/>': "Sicherung starten",
  'ESP][ device / COM port<context="task parameter"/>': "ESP][-Gerät / COM-Port",
  'Leave empty to use ESP2_PORT or the normal port picker. The SD currently in this ESP][ is backed up via USB Storage.<context="task parameter"/>':
    "Leer lassen für ESP2_PORT oder den normalen Port-Dialog. Die SD in diesem ESP][ wird über USB-Speicher gesichert.",
  'Backup destination<context="task parameter"/>': "Sicherungsziel",
  'Default: project local/sd-backups/. A new timestamped subdirectory is created. Read-only on the card.<context="task parameter"/>':
    "Standard: Projektordner local/sd-backups/. Neues Unterverzeichnis mit Zeitstempel. Karte bleibt unverändert (nur lesen).",
  'Source<context="task parameter"/>': "Quelle",
  'Normal: SD card inserted in the connected ESP][. Advanced: only if the card is already mounted on the PC (card reader) or auto-detect failed.<context="task parameter"/>':
    "Normal: SD-Karte im verbundenen ESP][. Erweitert: nur wenn die Karte bereits am PC gemountet ist (Kartenleser) oder die Autoerkennung scheiterte.",
  'ESP][ device (USB Storage)<context="task parameter"/>': "ESP][-Gerät (USB-Speicher)",
  'Advanced: manually select mounted SD<context="task parameter"/>':
    "Erweitert: gemountete SD manuell wählen",
  'Fallback only — select a Windows mount that already contains esp2/ (e.g. card reader).<context="task parameter"/>':
    "Nur Fallback — Windows-Mount wählen, der bereits esp2/ enthält (z. B. Kartenleser).",
  'Restore ESP][ SD card (destructive)<context="task parameter"/>':
    "ESP][-SD-Karte wiederherstellen (destruktiv)",
  'Continue<context="button text"/>': "Weiter",
  'Backup<context="task parameter"/>': "Sicherung",
  'Complete, hash-verified backups under local/sd-backups/. Or pick a folder below (NAS copy).<context="task parameter"/>':
    "Vollständige, hash-verifizierte Backups unter local/sd-backups/. Oder unten einen Ordner wählen (NAS-Kopie).",
  'Backup folder<context="task parameter"/>': "Backup-Ordner",
  'Select a backup directory (must contain manifest.json). Overrides the list when it points at a complete backup.<context="task parameter"/>':
    "Backup-Verzeichnis wählen (muss manifest.json enthalten). Überschreibt die Liste, wenn es auf ein vollständiges Backup zeigt.",
  'Leave empty to use ESP2_PORT or the normal port picker. The card currently in this ESP][ receives the restore via USB Storage.<context="task parameter"/>':
    "Leer lassen für ESP2_PORT oder den normalen Port-Dialog. Die SD in diesem ESP][ empfängt die Wiederherstellung über USB-Speicher.",
  'Destination<context="task parameter"/>': "Ziel",
  'Normal: SD card inserted in the connected ESP][. Advanced: only for an already-mounted card reader volume.<context="task parameter"/>':
    "Normal: SD-Karte im verbundenen ESP][. Erweitert: nur für bereits gemountetes Kartenleser-Volume.",
  'Fallback only — select the Windows mount for the target card (esp2/ may be missing on a new card).<context="task parameter"/>':
    "Nur Fallback — Windows-Mount der Zielkarte wählen (esp2/ darf auf einer neuen Karte fehlen).",
  'Dry run (no writes)<context="task parameter"/>': "Probelauf (keine Schreibvorgänge)",
  'Confirm restore to detected SD<context="task parameter"/>':
    "Wiederherstellung auf erkannte SD bestätigen",
  'Restore now<context="button text"/>': "Jetzt wiederherstellen",
  'Detected destination SD (read-only)<context="task parameter"/>':
    "Erkannte Ziel-SD (nur Anzeige)",
  'I confirm restore to this destination SD<context="task parameter"/>':
    "Ich bestätige die Wiederherstellung auf diese Ziel-SD",
  'Writes/replaces the ESP][ /esp2 tree from the backup. Files outside esp2/ are left alone. Empty cards without esp2/ are OK.<context="task parameter"/>':
    "Schreibt/ersetzt den ESP][-Baum /esp2 aus dem Backup. Dateien außerhalb von esp2/ bleiben. Leere Karten ohne esp2/ sind OK.",
  'Select ESP][ SD volume<context="task parameter"/>': "ESP][-SD-Volume wählen",
  'Select destination SD volume<context="task parameter"/>': "Ziel-SD-Volume wählen",
  'Use this volume<context="button text"/>': "Dieses Volume verwenden",
  'Detected volumes<context="task parameter"/>': "Erkannte Volumes",
  'Select a backup.<context="task error"/>': "Backup auswählen.",
  'Confirmation required after reviewing the detected destination SD.<context="task error"/>':
    "Bestätigung erforderlich nach Prüfung der erkannten Ziel-SD.",
  'ESP][ COM port is required for USB Storage mode.<context="task error"/>':
    "ESP][-COM-Port ist für den USB-Speichermodus erforderlich.",
  'Advanced mode requires manually selecting a mounted SD.<context="task error"/>':
    "Erweiterter Modus erfordert die manuelle Auswahl einer gemounteten SD.",
  'Timed out waiting for the ESP][ SD to appear in Windows after USB Storage. Try Advanced: manually select mounted SD, or check MSC support.<context="task error"/>':
    "Zeitüberschreitung: ESP][-SD erschien nach USB-Speicher nicht in Windows. „Erweitert: gemountete SD manuell wählen“ nutzen oder MSC-Unterstützung prüfen.",
  'Multiple new volumes appeared — select one candidate explicitly.<context="task error"/>':
    "Mehrere neue Volumes erschienen — einen Kandidaten ausdrücklich wählen.",
  'Selected volume is not among detected candidates.<context="task error"/>':
    "Gewähltes Volume ist nicht unter den erkannten Kandidaten.",
  'Connecting to ESP][ on <port/><context="task log"/>':
    "Verbinde mit ESP][ auf <port/>",
  'Switching SD to USB storage…<context="task log"/>':
    "Wechsle SD zu USB-Speicher…",
  'Waiting for Windows to mount the ESP][ SD…<context="task log"/>':
    "Warte, bis Windows die ESP][-SD mountet…",
  'Waiting for ESP][ SD card...<context="task log"/>':
    "Warte auf ESP][-SD-Karte…",
  'SD detected<context="task log"/>': "SD erkannt",
  'SD detected: <info/><context="task log"/>': "SD erkannt: <info/>",
  'Copying…<context="task log"/>': "Kopiere…",
  'Verifying…<context="task log"/>': "Verifiziere…",
  'Finished.<context="task log"/>': "Fertig.",
  'Dry run…<context="task log"/>': "Probelauf…",
  'Using advanced manual SD mount <path/><context="task log"/>':
    "Verwende erweiterte manuelle SD-Mount <path/>",
  'Releasing USB storage / returning SD to ESP][…<context="task log"/>':
    "USB-Speicher freigeben / SD an ESP][ zurückgeben…",
  'USB storage leave failed — eject the SD volume in Windows, then leave MSC manually if needed: <err/><context="task log"/>':
    "USB-Speicher-Leave fehlgeschlagen — SD-Volume in Windows auswerfen, ggf. MSC manuell verlassen: <err/>",
  'SD backup (logical /esp2 tree, not raw image). source=<source/> fs=<fs/> capacity=<cap/><context="task log"/>':
    "SD-Sicherung (logischer /esp2-Baum, kein Rohabbild). Quelle=<source/> FS=<fs/> Kapazität=<cap/>",
  'SD backup PASS → <path/> files=<files/> bytes=<bytes/> verified SHA-256 (~<mbs/> MB/s)<context="task log"/>':
    "SD-Sicherung PASS → <path/> Dateien=<files/> Bytes=<bytes/> SHA-256 verifiziert (~<mbs/> MB/s)",
  'RESTORE plan backup=<backup/> → dest=<dest/> files=<files/> bytes=<bytes/> info=<info/><context="task log"/>':
    "RESTORE-Plan Backup=<backup/> → Ziel=<dest/> Dateien=<files/> Bytes=<bytes/> Info=<info/>",
  'Dry run only — no writes. Would restore <files/> files (<bytes/> bytes); stale remove=<stale/>.<context="task log"/>':
    "Nur Probelauf — keine Schreibvorgänge. Würde <files/> Dateien (<bytes/> Bytes) wiederherstellen; veraltet entfernen=<stale/>.",
  'SD restore PASS → <dest/> files=<files/> bytes=<bytes/> SHA-256 verified (~<mbs/> MB/s)<context="task log"/>':
    "SD-Wiederherstellung PASS → <dest/> Dateien=<files/> Bytes=<bytes/> SHA-256 verifiziert (~<mbs/> MB/s)",

  // Attention
  'Next Apple II media readiness step.<context="µAttentionTooltip"/>':
    "Nächster Bereitschaftsschritt für Apple-II-Medien.",
};

/** en-US identity dictionary (source phrases). */
const en = Object.fromEntries(Object.keys(de).map((k) => [k, k.replace(/<context="[^"]+"\/>$/, "").replace(/<context='[^']+'\/>$/, "")]));
// Better: en-US maps key → English display without requiring strip; µGulp uses source text as en-US.
// Identity key→key is the established pattern when source already embeds English.
const enUS = Object.fromEntries(Object.keys(de).map((k) => [k, k]));

writeFileSync(join(outDir, "de-DE.json"), JSON.stringify(de, null, "\t") + "\n", "utf8");
writeFileSync(join(outDir, "en-US.json"), JSON.stringify(enUS, null, "\t") + "\n", "utf8");

// Validate gulpfile phrases for DisplayName/Group are covered in de-DE
const gulp = readFileSync(join(root, "gulpfile.mjs"), "utf8");
const phrases = [
  ...gulp.matchAll(/µDisplayName:\s*'([^']+)'/g),
  ...gulp.matchAll(/µGroup:\s*'([^']+)'/g),
].map((m) => m[1]);
const missing = [...new Set(phrases)].filter((p) => !(p in de));
if (missing.length) {
  console.error("Missing de-DE entries:");
  for (const p of missing) console.error(" ", p);
  process.exitCode = 1;
} else {
  console.log(
    `Wrote ${Object.keys(de).length} phrases to i18x/gulp/{de-DE,en-US}.json`,
  );
}
