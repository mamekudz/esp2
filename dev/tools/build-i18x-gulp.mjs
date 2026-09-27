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
  'Media / apple2js<context="µGroup"/>': "Medien / apple2js",
  'Apple II/Media<context="µGroup"/>': "Apple II/Medien",
  'Apple II/Emulator<context="µGroup"/>': "Apple II/Emulator",
  'Apple II/Compatibility<context="µGroup"/>': "Apple II/Kompatibilität",
  'Apple II/Device<context="µGroup"/>': "Apple II/Gerät",
  'Device Storage<context="µGroup"/>': "Gerätespeicher",
  'Apple II Host<context="µGroup"/>': "Apple II Host",
  'Media / local apple2<context="µGroup"/>': "Medien / lokal apple2",

  // Firmware
  'Build Firmware<context="µDisplayName"/>': "Firmware bauen",
  'Compiles ESP][ with PlatformIO (env: bringup). Does not change board config.<context="µDescription"/>':
    "Kompiliert ESP][ mit PlatformIO (Umgebung: bringup). Ändert die Board-Konfiguration nicht.",
  'Compiles ESP][ with PlatformIO. Default env from platformio.ini / ESP2_PIO_ENV (bringup|core_smoke|apple2_text).<context="µDescription"/>':
    "Kompiliert ESP][ mit PlatformIO. Standard-Umgebung aus platformio.ini / ESP2_PIO_ENV (bringup|core_smoke|apple2_text).",
  'Show Firmware Env<context="µDisplayName"/>': "Firmware-Umgebung anzeigen",
  'Prints the active PlatformIO environment and known envs from platformio.ini.<context="µDescription"/>':
    "Zeigt die aktive PlatformIO-Umgebung und bekannte Envs aus platformio.ini.",
  'Build & Upload<context="µDisplayName"/>': "Bauen & hochladen",
  'Rebuilds and flashes ESP][ firmware in one step.<context="µDescription"/>':
    "Baut die ESP][-Firmware neu und flasht sie in einem Schritt.",
  'Set ESP2_PORT=COMx to skip the port dialog.<context="µTooltip"/>':
    "ESP2_PORT=COMx setzen, um den Port-Dialog zu überspringen.",
  'Upload Firmware<context="µDisplayName"/>': "Firmware hochladen",
  'Memory Size<context="µDisplayName"/>': "Speicherbelegung",
  'Clean Build<context="µDisplayName"/>': "Build bereinigen",
  'Removes .pio/build artefacts for the active PlatformIO env (ESP2_PIO_ENV / default bringup).<context="µDescription"/>':
    "Entfernt .pio/build-Artefakte der aktiven PlatformIO-Umgebung (ESP2_PIO_ENV / Standard bringup).",
  'Rebuild Firmware<context="µDisplayName"/>': "Firmware neu bauen",
  'clean → build for the active PlatformIO env.<context="µDescription"/>':
    "clean → build für die aktive PlatformIO-Umgebung.",

  // Tools
  'List Devices<context="µDisplayName"/>': "Geräte auflisten",
  'Serial Monitor<context="µDisplayName"/>': "Seriellmonitor",
  'Format Check<context="µDisplayName"/>': "Formatprüfung",
  'Reports clang-format availability for project-owned C/C++ (.clang-format). Does not rewrite files.<context="µDescription"/>':
    "Meldet die Verfügbarkeit von clang-format für projekteigene C/C++ (.clang-format). Schreibt keine Dateien um.",
  'Rebuild Gulp i18x Dictionaries<context="µDisplayName"/>':
    "Gulp-i18x-Wörterbücher neu erzeugen",
  'Regenerates i18x/gulp en-US and de-DE dictionaries from build-i18x-gulp.mjs.<context="µDescription"/>':
    "Erzeugt i18x/gulp en-US- und de-DE-Wörterbücher aus build-i18x-gulp.mjs neu.",

  // Docs / Git / Backup
  'Compose READMEs<context="µDisplayName"/>': "READMEs erzeugen",
  'Compose README (en-US)<context="µDisplayName"/>': "README erzeugen (en-US)",
  'Compose README (de-DE)<context="µDisplayName"/>': "README erzeugen (de-DE)",
  'Release history up to date V<version/><context="µDisplayName"/>':
    "Release-Historie aktuell V<version/>",
  'Update release history — pending V<version/><context="µDisplayName"/>':
    "Release-Historie aktualisieren — ausstehend V<version/>",
  'Release history V<version/><context="µDisplayName"/>':
    "Release-Historie V<version/>",
  'Release context CHECK V<version/><context="µDisplayName"/>':
    "Release-Kontext CHECK V<version/>",
  'Release context FIX V<version/><context="µDisplayName"/>':
    "Release-Kontext FIX V<version/>",
  'Release i18x update V<version/><context="µDisplayName"/>':
    "Release-i18x aktualisieren V<version/>",
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
  'ESP][ device configuration V<version/><context="µDisplayName"/>':
    "ESP][-Gerätekonfiguration V<version/>",
  'Run device macro V<version/><context="µDisplayName"/>':
    "Geräte-Makro ausführen V<version/>",
  'Stage and upload /esp2/config/system.json + macros.json from a named profile (µGulp form). Does not embed media bytes.<context="µDescription"/>':
    "Stellt /esp2/config/system.json + macros.json aus einem Profil bereit und lädt hoch (µGulp-Formular). Keine Media-Bytes.",
  'Execute a named input macro on a live ESP][ (#ESP2MACRO RUN). Same engine as startup macros.<context="µDescription"/>':
    "Führt ein benanntes Eingabe-Makro auf einem laufenden ESP][ aus (#ESP2MACRO RUN). Dieselbe Engine wie Startup-Makros.",
  'Configuration profile<context="task parameter"/>': "Konfigurationsprofil",
  'Folder under config/device/profiles/ (e.g. galaxian-demo).<context="task parameter"/>':
    "Ordner unter config/device/profiles/ (z. B. galaxian-demo).",
  'Serial port<context="task parameter"/>': "Serielle Schnittstelle",
  'COMx for upload. Not required with dry-run.<context="task parameter"/>':
    "COMx für Upload. Bei Dry-Run nicht nötig.",
  'Dry-run (stage only)<context="task parameter"/>': "Dry-Run (nur bereitstellen)",
  'Macro id<context="task parameter"/>': "Makro-ID",
  'Device config profile=<profile/> uploaded=<uploaded/> (paths only; no ROM/disk bytes).<context="task log"/>':
    "Geräte-Config Profil=<profile/> hochgeladen=<uploaded/> (nur Pfade; keine ROM/Disk-Bytes).",
  'Git backup checkpoint<context="µDisplayName"/>': "Git-Backup-Checkpoint",
  'Git Checkpoint Commit<context="µDisplayName"/>': "Git-Checkpoint-Commit",
  'Git Status (Dry-Run)<context="µDisplayName"/>': "Git-Status (Dry-Run)",
  'Shows Git status and would-be checkpoint paths. Never commits or pushes. Respects GIT_BACKUP_NEVER_STAGE / local media exclusions.<context="µDescription"/>':
    "Zeigt Git-Status und geplante Checkpoint-Pfade. Commitet und pusht nie. Beachtet GIT_BACKUP_NEVER_STAGE / lokale Medienausschlüsse.",
  'Git Commit Checkpoint<context="µDisplayName"/>': "Git-Commit-Checkpoint",
  'Creates a local Git checkpoint commit (same rules as backup:git) without pushing. Never stages local/apple2, proprietary media, or secrets.<context="µDescription"/>':
    "Erstellt einen lokalen Git-Checkpoint-Commit (gleiche Regeln wie backup:git) ohne Push. Stagt nie local/apple2, proprietäre Medien oder Secrets.",
  'Git Push<context="µDisplayName"/>': "Git Push",
  'Pushes the current branch to its upstream remote. Does not stage or commit.<context="µDescription"/>':
    "Pusht den aktuellen Branch zum Upstream-Remote. Stagt und commitet nicht.",
  'Backup to NAS<context="µDisplayName"/>': "NAS-Backup",
  'Backup to NAS (alias)<context="µDisplayName"/>': "NAS-Backup (Alias)",
  'Alias of backup — NAS form / NAS_TARGET_1..3. Includes local/apple2 privately; skips disposable caches.<context="µDescription"/>':
    "Alias von backup — NAS-Formular / NAS_TARGET_1..3. Sichert local/apple2 privat; lässt disposable Caches weg.",
  'Copies ESP][ project trees plus local/downloaded assets (local/apple2, local/roms, _refs, 3dprint) to up to three private NAS folders. Skips node_modules, .pio, and other disposable caches. Gitignore ≠ NAS exclude.<context="µDescription"/>':
    "Kopiert ESP][-Projektbäume plus lokale/heruntergeladene Assets (local/apple2, local/roms, _refs, 3dprint) auf bis zu drei private NAS-Ordner. Lässt node_modules, .pio und andere disposable Caches weg. Gitignore ≠ NAS-Ausschluss.",
  'List NAS Backups<context="µDisplayName"/>': "NAS-Backups auflisten",
  'Shows configured NAS destinations and whether essential restore files are present. Read-only.<context="µDescription"/>':
    "Zeigt konfigurierte NAS-Ziele und ob essenzielle Restore-Dateien vorhanden sind. Nur Lesen.",
  'Verify NAS Backup<context="µDisplayName"/>': "NAS-Backup prüfen",
  'Checks primary NAS destination reachability and essential restore files. Read-only; never restores.<context="µDescription"/>':
    "Prüft Erreichbarkeit des primären NAS-Ziels und essenzielle Restore-Dateien. Nur Lesen; stellt nie wieder her.",
  'Backup all (docs + Git + NAS)<context="µDisplayName"/>':
    "Alles sichern (Docs + Git + NAS)",
  'Backup All (Docs + Git + NAS)<context="µDisplayName"/>':
    "Alles sichern (Docs + Git + NAS)",

  // Tests
  'Run Infrastructure Tests<context="µDisplayName"/>':
    "Infrastruktur-Tests ausführen",
  'Runs node:test for NAS/Git/docs infrastructure helpers (no real NAS write / no Git commit).<context="µDescription"/>':
    "Führt node:test für NAS/Git/Docs-Infrastrukturhelfer aus (kein echtes NAS-Schreiben / kein Git-Commit).",
  'Run µGulp Catalog Tests<context="µDisplayName"/>':
    "µGulp-Katalog-Tests ausführen",
  'Regression tests for first-party task catalog, metadata, and UTF-8 integrity.<context="µDescription"/>':
    "Regressionstests für First-Party-Task-Katalog, Metadaten und UTF-8-Integrität.",
  'Run Host Apple II Tests<context="µDisplayName"/>':
    "Host-Apple-II-Tests ausführen",
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
  'apple2js Sync<context="µDisplayName"/>': "apple2js synchronisieren",
  'apple2js Catalog<context="µDisplayName"/>': "apple2js-Katalog",
  'apple2js Audit<context="µDisplayName"/>': "apple2js prüfen",
  'Media Identify<context="µDisplayName"/>': "Medium identifizieren",
  'Media Import<context="µDisplayName"/>': "Medium importieren",
  'Media Inspect (a2kit)<context="µDisplayName"/>': "Medium prüfen (a2kit)",
  'SD Prepare Library<context="µDisplayName"/>': "SD-Bibliothek vorbereiten",

  // Apple II local media
  'Download Free Apple II ROMs<context="µDisplayName"/>':
    "Freie Apple-II-ROMs herunterladen",
  'Download ONLY redistributable ROMs (e.g. AppleIIGo) into gitignored local/apple2/roms/.<context="µDescription"/>':
    "Lädt NUR weiterverbreitbare ROMs (z. B. AppleIIGo) nach gitignored local/apple2/roms/.",
  'Download Apple II Media<context="µDisplayName"/>':
    "Apple-II-Medien herunterladen",
  'Fetch apple2js title(s) into gitignored local/apple2/cache/ (LOCAL_TEST_ONLY; never stages Git).<context="µDescription"/>':
    "Holt apple2js-Titel nach gitignored local/apple2/cache/ (LOCAL_TEST_ONLY; kein Git-Staging).",
  'Prepare Apple II Runtime Disks<context="µDisplayName"/>':
    "Apple-II-Laufzeitdisketten vorbereiten",
  'Normalize cached source to local/apple2/disks/ (offline; prefers DSK when lossless).<context="µDescription"/>':
    "Normalisiert Cache-Quellen nach local/apple2/disks/ (offline; bevorzugt DSK wenn verlustfrei).",
  'Apple II Media Status<context="µDisplayName"/>': "Apple-II-Medienstatus",
  'Compact table of local titles: downloaded / prepared / provenance.<context="µDescription"/>':
    "Kompakte Tabelle lokaler Titel: heruntergeladen / vorbereitet / Provenienz.",
  'List Local Apple II Media<context="µDisplayName"/>':
    "Lokale Apple-II-Medien auflisten",
  'JSON list of locally cached titles.<context="µDescription"/>':
    "JSON-Liste lokal gecachter Titel.",
  'Audit Apple II Media Provenance<context="µDisplayName"/>':
    "Apple-II-Medien-Provenienz prüfen",
  'Provenance counts for local cache (download never upgrades rights).<context="µDescription"/>':
    "Provenienz-Zähler für den lokalen Cache (Download wertet Rechte nie auf).",
  'Clean Generated Apple II Disks<context="µDisplayName"/>':
    "Erzeugte Apple-II-Disketten bereinigen",
  'Default: remove generated disks/. Optional --cache / --user (never deletes user media by default).<context="µDescription"/>':
    "Standard: erzeugte disks/ entfernen. Optional --cache / --user (löscht Nutzer-Medien standardmäßig nie).",
  'Sync Title to Device<context="µDisplayName"/>':
    "Titel auf Gerät synchronisieren",
  'Upload ONLY selected title runtime ROM+disk via serial ESPU (not full catalog).<context="µDescription"/>':
    "Lädt NUR ROM+Diskette des gewählten Titels per serieller ESPU hoch (nicht den ganzen Katalog).",

  // Emulator / compat
  'Identify Apple II ROM<context="µDisplayName"/>': "Apple-II-ROM identifizieren",
  'Test Apple II ROM on Host<context="µDisplayName"/>':
    "Apple-II-ROM auf dem Host testen",
  'Run Apple II Host Emulator<context="µDisplayName"/>':
    "Apple-II-Host-Emulator starten",
  'Test Apple II Disk Boot on Host<context="µDisplayName"/>':
    "Apple-II-Diskettenboot auf dem Host testen",
  'Run Compatibility Test<context="µDisplayName"/>':
    "Kompatibilitätstest ausführen",
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
  'Device Serial Upload<context="µDisplayName"/>': "Serieller Geräte-Upload",
  'Device USB Storage Mode<context="µDisplayName"/>': "USB-Speichermodus",

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
