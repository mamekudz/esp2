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
  'Tools<context="µGroup"/>': "Werkzeuge",
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
  'Build & Upload<context="µDisplayName"/>': "Bauen & hochladen",
  'Rebuilds and flashes ESP][ firmware in one step.<context="µDescription"/>':
    "Baut die ESP][-Firmware neu und flasht sie in einem Schritt.",
  'Set ESP2_PORT=COMx to skip the port dialog.<context="µTooltip"/>':
    "ESP2_PORT=COMx setzen, um den Port-Dialog zu überspringen.",
  'Upload Firmware<context="µDisplayName"/>': "Firmware hochladen",
  'Memory Size<context="µDisplayName"/>': "Speicherbelegung",
  'Clean Build<context="µDisplayName"/>': "Build bereinigen",
  'Rebuild Firmware<context="µDisplayName"/>': "Firmware neu bauen",

  // Tools
  'List Devices<context="µDisplayName"/>': "Geräte auflisten",
  'Serial Monitor<context="µDisplayName"/>': "Seriellmonitor",

  // Docs & Backup
  'Compose READMEs<context="µDisplayName"/>': "READMEs erzeugen",
  'Compose README (en-US)<context="µDisplayName"/>': "README erzeugen (en-US)",
  'Compose README (de-DE)<context="µDisplayName"/>': "README erzeugen (de-DE)",
  'Git backup checkpoint<context="µDisplayName"/>': "Git-Backup-Checkpoint",
  'Backup to NAS<context="µDisplayName"/>': "NAS-Backup",
  'Backup all (docs + Git + NAS)<context="µDisplayName"/>':
    "Alles sichern (Docs + Git + NAS)",
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
