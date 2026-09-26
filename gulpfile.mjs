//================================================================
// ESP][ — µGulp / Gulp task runner
// © 2026 Meinolf Amekudzi
//
// Groups:
//   Firmware     — build / flash / upload / clean / size
//   Tools        — devices / monitor
//   Docs & Backup — docs / backup:git / backup / backup:all
//
// NAS form = microGulp BACKUP_TO_NAS / Watchy pattern (destination1..3).
// No help clutter. Default = docs (safe).
//================================================================

import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { existsSync, mkdirSync } from "node:fs";
import { spawnSync } from "node:child_process";
import gulp from "gulp";
import {
  Log,
  Warn,
  ReportProgress,
  InstallStringExtensions,
  PlaySignal,
  GetParameter,
  RequestForm,
  IsMicroGulp,
} from "gulp-mu-gulp-api";
import {
  ComposeReadme,
  ComposeReadmeLocale,
  README_SOURCE_RELATIVE,
  README_SOURCES,
} from "./dev/tools/readme-compose.mjs";
import {
  AssertNasBackupTarget,
  LoadNasLocalDefaults,
  MirrorNonReproducible,
  NAS_BACKUP_INCLUDE_DIRS,
  NAS_BACKUP_INCLUDE_FILES,
  NAS_BACKUP_MAX_DESTINATIONS,
  ResolveNasTargets,
  VerifyBackupContents,
} from "./dev/tools/nas-backup.mjs";
import { RunGitBackup } from "./dev/tools/git-backup.mjs";
import {
  AskPort,
  ListPorts,
  MonitorArgs,
  Pio,
  PIO_ENV,
  UploadArgs,
} from "./dev/tools/pio.mjs";
import { syncApple2js } from "./dev/tools/apple2js/sync.mjs";

InstallStringExtensions();

export const µI18xContext = { project: "esp2", product: "ESP][" };

const rootDir = dirname(fileURLToPath(import.meta.url));

/**
 * @param {Function} _task
 * @param {object} _meta
 */
function _Tag(_task, _meta) {
  if (_meta.gulpName) _task.displayName = _meta.gulpName;
  if (_meta.µDisplayName) _task.µDisplayName = _meta.µDisplayName.i18xRegister();
  if (_meta.µDescription) _task.µDescription = _meta.µDescription.i18xRegister();
  if (_meta.µTooltip) _task.µTooltip = _meta.µTooltip.i18xRegister();
  if (_meta.µGroup) _task.µGroup = _meta.µGroup.i18xRegister();
  if (_meta.µIcon != null) _task.µIcon = _meta.µIcon;
  if (_meta.µOrder != null) _task.µOrder = _meta.µOrder;
  if (_meta.µExecutionConcurrency != null) {
    _task.µExecutionConcurrency = _meta.µExecutionConcurrency;
  }
  if (_meta.µExecutionRestrictions) {
    _task.µExecutionRestrictions = _meta.µExecutionRestrictions;
  }
  if (_meta.µParameters) _task.µParameters = _meta.µParameters;
  return _task;
}

//================================================================
// Firmware
//================================================================

export async function build() {
  ReportProgress(0, "build");
  Log('Building ESP][ firmware (env <env/>)…<context="task log"/>', {
    env: PIO_ENV,
  });
  await Pio(rootDir, ["run", "-e", PIO_ENV], "pio build");
  ReportProgress(1, "build");
  Log('Build OK.<context="task log"/>');
  PlaySignal("success");
}
_Tag(build, {
  gulpName: "build",
  µDisplayName: 'Build Firmware<context="µDisplayName"/>',
  µDescription:
    'Compiles ESP][ with PlatformIO (env: bringup). Does not change board config.<context="µDescription"/>',
  µIcon: "\u2692",
  µGroup: 'Firmware<context="µGroup"/>',
  µOrder: 10,
  µExecutionConcurrency: false,
});

export async function flash() {
  ReportProgress(0, "flash-build");
  const port = await AskPort(rootDir, "Flash port (build + upload)");
  Log('Build + upload → <port/><context="task log"/>…', {
    port: port ?? "auto/ini",
  });
  await Pio(rootDir, ["run", "-e", PIO_ENV], "pio build");
  ReportProgress(0.55, "flash-upload");
  await Pio(rootDir, UploadArgs(port), "pio upload");
  ReportProgress(1, "flash-upload");
  Log('Flash finished.<context="task log"/>');
  PlaySignal("success");
}
_Tag(flash, {
  gulpName: "flash",
  µDisplayName: 'Build & Upload<context="µDisplayName"/>',
  µDescription:
    'Rebuilds and flashes ESP][ firmware in one step.<context="µDescription"/>',
  µTooltip:
    'Set ESP2_PORT=COMx to skip the port dialog.<context="µTooltip"/>',
  µIcon: "\u26A1",
  µGroup: 'Firmware<context="µGroup"/>',
  µOrder: 15,
  µExecutionConcurrency: false,
  µExecutionRestrictions: { deny: ["upload", "build", "backup"] },
});

export async function upload() {
  ReportProgress(0, "upload");
  const port = await AskPort(rootDir, "Upload port");
  Log('Uploading → <port/><context="task log"/>…', {
    port: port ?? "auto/ini",
  });
  await Pio(rootDir, UploadArgs(port), "pio upload");
  ReportProgress(1, "upload");
  Log('Upload finished.<context="task log"/>');
  PlaySignal("success");
}
_Tag(upload, {
  gulpName: "upload",
  µDisplayName: 'Upload Firmware<context="µDisplayName"/>',
  µDescription:
    'Flashes the last build. Asks for the COM port.<context="µDescription"/>',
  µTooltip:
    'Set ESP2_PORT=COMx to skip the port dialog.<context="µTooltip"/>',
  µIcon: "\u2191",
  µGroup: 'Firmware<context="µGroup"/>',
  µOrder: 20,
  µExecutionConcurrency: false,
  µExecutionRestrictions: { deny: ["flash", "monitor", "backup"] },
});

export async function size() {
  ReportProgress(0, "size");
  await Pio(rootDir, ["run", "-e", PIO_ENV, "-t", "size"], "pio size");
  ReportProgress(1, "size");
}
_Tag(size, {
  gulpName: "size",
  µDisplayName: 'Memory Size<context="µDisplayName"/>',
  µDescription:
    'Shows RAM / Flash usage of the last firmware build.<context="µDescription"/>',
  µIcon: "\u25A6",
  µGroup: 'Firmware<context="µGroup"/>',
  µOrder: 30,
  µExecutionConcurrency: true,
});

export async function clean() {
  ReportProgress(0, "clean");
  await Pio(rootDir, ["run", "-e", PIO_ENV, "-t", "clean"], "pio clean");
  ReportProgress(1, "clean");
  Log('Build artefacts removed.<context="task log"/>');
}
_Tag(clean, {
  gulpName: "clean",
  µDisplayName: 'Clean Build<context="µDisplayName"/>',
  µDescription:
    'Removes .pio/build artefacts for env bringup.<context="µDescription"/>',
  µIcon: "\u239A",
  µGroup: 'Firmware<context="µGroup"/>',
  µOrder: 40,
  µExecutionConcurrency: true,
});

export const rebuild = gulp.series(clean, build);
_Tag(rebuild, {
  gulpName: "rebuild",
  µDisplayName: 'Rebuild Firmware<context="µDisplayName"/>',
  µDescription: 'clean → build (PlatformIO env bringup).<context="µDescription"/>',
  µIcon: "\u21BB",
  µGroup: 'Firmware<context="µGroup"/>',
  µOrder: 12,
  µExecutionConcurrency: false,
  µExecutionRestrictions: { deny: ["flash", "upload", "backup"] },
});

//================================================================
// Tools
//================================================================

export async function devices() {
  ReportProgress(0, "devices");
  const ports = await ListPorts(rootDir);
  if (ports.length === 0) {
    Warn('No serial ports detected.<context="task log"/>');
  } else {
    Log('Serial ports:<context="task log"/>');
    for (const p of ports) {
      Log('  <port/><context="task log"/>', { port: p.label });
    }
  }
  await Pio(rootDir, ["device", "list"], "pio device list");
  ReportProgress(1, "devices");
}
_Tag(devices, {
  gulpName: "devices",
  µDisplayName: 'List Devices<context="µDisplayName"/>',
  µDescription: 'Lists connected serial ports.<context="µDescription"/>',
  µIcon: "\u2398",
  µGroup: 'Tools<context="µGroup"/>',
  µOrder: 10,
  µExecutionConcurrency: true,
});

export async function monitor() {
  const port = await AskPort(rootDir, "Serial monitor port");
  Log('Opening serial monitor (Ctrl+C to stop)…<context="task log"/>');
  await Pio(rootDir, MonitorArgs(port), "pio monitor");
}
_Tag(monitor, {
  gulpName: "monitor",
  µDisplayName: 'Serial Monitor<context="µDisplayName"/>',
  µDescription:
    'Opens the PlatformIO serial monitor at 115200 baud.<context="µDescription"/>',
  µTooltip:
    'Set ESP2_PORT=COMx to skip the port dialog. Free COM5 if another monitor holds it.<context="µTooltip"/>',
  µIcon: "\u2399",
  µGroup: 'Tools<context="µGroup"/>',
  µOrder: 20,
  µExecutionConcurrency: false,
});

//================================================================
// Docs & Backup — NAS form (microGulp / Watchy)
//================================================================

function _NasBackupParameters() {
  const local = LoadNasLocalDefaults(rootDir);
  return {
    title: 'NAS backup destinations<context="task parameter"/>'.i18xRegister(),
    submitLabel: 'Start backup<context="button text"/>'.i18xRegister(),
    fields: [
      {
        id: "destination1",
        type: "folder",
        required: false,
        default: local.t1,
        label: 'Destination 1 (NAS_TARGET_1)<context="task parameter"/>'.i18xRegister(),
        description:
          'Primary backup folder. Leave empty to skip. Up to three destinations. µGulp remembers answers; optional defaults from config/nas.targets.local.<context="task parameter"/>'.i18xRegister(),
      },
      {
        id: "destination2",
        type: "folder",
        required: false,
        default: local.t2,
        label: 'Destination 2 (NAS_TARGET_2)<context="task parameter"/>'.i18xRegister(),
        description:
          'Second folder when set. Leave empty to skip.<context="task parameter"/>'.i18xRegister(),
      },
      {
        id: "destination3",
        type: "folder",
        required: false,
        default: local.t3,
        label: 'Destination 3 (NAS_TARGET_3)<context="task parameter"/>'.i18xRegister(),
        description:
          'Third folder when set. Leave empty to skip.<context="task parameter"/>'.i18xRegister(),
      },
      {
        id: "dryRun",
        type: "boolean",
        default: local.dryRun,
        label: 'Preview only (robocopy /L)<context="task parameter"/>'.i18xRegister(),
        description:
          'List differences without copying or deleting.<context="task parameter"/>'.i18xRegister(),
      },
    ],
  };
}

async function _ResolveNasForRun() {
  if (
    process.env.NAS_TARGET_1 ||
    process.env.NAS_TARGET_2 ||
    process.env.NAS_TARGET_3 ||
    process.env.ESP2_NAS_BACKUP_PATHS
  ) {
    return ResolveNasTargets(rootDir, null);
  }

  if (process.env.MICROGULP_PARAMS) {
    return ResolveNasTargets(rootDir, {
      destination1: GetParameter("destination1", ""),
      destination2: GetParameter("destination2", ""),
      destination3: GetParameter("destination3", ""),
      dryRun: GetParameter("dryRun", false) === true,
    });
  }

  if (IsMicroGulp()) {
    // µParameters already collected by dashboard before task body
    return ResolveNasTargets(rootDir, {
      destination1: GetParameter("destination1", ""),
      destination2: GetParameter("destination2", ""),
      destination3: GetParameter("destination3", ""),
      dryRun: GetParameter("dryRun", false) === true,
    });
  }

  const values = await RequestForm(_NasBackupParameters());
  return ResolveNasTargets(rootDir, {
    destination1: values?.destination1 ?? "",
    destination2: values?.destination2 ?? "",
    destination3: values?.destination3 ?? "",
    dryRun: values?.dryRun === true,
  });
}

async function _RunNasBackup() {
  ReportProgress(0, "backup-nas");
  const resolved = await _ResolveNasForRun();

  if (resolved.rejectedExtra > 0) {
    throw new Error(
      `NAS backup allows at most ${NAS_BACKUP_MAX_DESTINATIONS} destinations; extra targets were configured.`
    );
  }

  Log('NAS config source: <source/><context="task log"/>', {
    source: resolved.source,
  });
  Log('Include dirs: <dirs/> | files: <files/><context="task log"/>', {
    dirs: NAS_BACKUP_INCLUDE_DIRS.join(", "),
    files: NAS_BACKUP_INCLUDE_FILES.join(", "),
  });

  if (resolved.destinations.length === 0) {
    Log(
      '[NAS] 0 destinations — use the form, set NAS_TARGET_1..3, or config/nas.targets.local.<context="task log"/>'
    );
    ReportProgress(1, "backup-nas");
    PlaySignal("success");
    return { ok: true, results: [], dryRun: resolved.dryRun };
  }

  if (resolved.dryRun) {
    Log('[NAS] DRY-RUN — preview only.<context="task log"/>');
  }

  /** @type {{ index: number, path: string, status: string, detail?: string }[]} */
  const results = [];

  for (let i = 0; i < resolved.destinations.length; i += 1) {
    const label = `NAS${i + 1}`;
    const destination = resolved.destinations[i];
    ReportProgress(i / resolved.destinations.length, "backup-nas");
    try {
      if (!existsSync(destination)) {
        Warn(`[${label}] unavailable — skipped (${destination})<context="task warning"/>`);
        results.push({
          index: i + 1,
          path: destination,
          status: "unavailable",
          detail: "path does not exist",
        });
        continue;
      }
      const destResolved = AssertNasBackupTarget(destination, rootDir);
      if (!resolved.dryRun) mkdirSync(destResolved, { recursive: true });
      Log(`[${label}] Copying → <path/><context="task log"/>`, { path: destResolved });
      await MirrorNonReproducible(rootDir, destResolved, resolved.dryRun);
      if (!resolved.dryRun) {
        const check = VerifyBackupContents(destResolved);
        if (!check.ok) {
          Warn(
            `[${label}] finished but missing: ${check.missing.join(", ")}<context="task warning"/>`
          );
          results.push({
            index: i + 1,
            path: destResolved,
            status: "incomplete",
            detail: check.missing.join(", "),
          });
          continue;
        }
      }
      Log(`[${label}] OK<context="task log"/>`);
      results.push({ index: i + 1, path: destResolved, status: "OK" });
    } catch (err) {
      Warn(
        `[${label}] unavailable/failed — skipped (${err.message})<context="task warning"/>`
      );
      results.push({
        index: i + 1,
        path: destination,
        status: "failed",
        detail: err.message,
      });
    }
  }

  Log("— NAS backup summary —<context=\"task log\"/>");
  for (const r of results) {
    Log(
      `[NAS${r.index}] ${r.status}${r.detail ? " — " + r.detail : ""} — ${r.path}<context="task log"/>`
    );
  }
  const okCount = results.filter((r) => r.status === "OK").length;
  Log(
    `Done: <ok format="int"/> OK / <total format="int"/> configured (max ${NAS_BACKUP_MAX_DESTINATIONS}).<context="task log"/>`,
    { ok: okCount, total: results.length }
  );

  ReportProgress(1, "backup-nas");
  if (okCount > 0 || results.length === 0) PlaySignal("success");
  return {
    ok: okCount > 0 || results.every((r) => r.status === "unavailable"),
    results,
  };
}

export async function docs() {
  ReportProgress(0, "docs");
  Log('Composing READMEs from <path/> + de-DE.src.md<context="task log"/>', {
    path: README_SOURCE_RELATIVE,
  });
  const result = ComposeReadme({ root: rootDir });
  for (const r of result.results) {
    const name = README_SOURCES[r.locale].outputRel;
    if (r.changed) {
      Log('Wrote <path/> (<bytes format="int"/> bytes)<context="task log"/>', {
        path: name,
        bytes: r.bytes,
      });
    } else {
      Log(
        '<path/> already up to date (<bytes format="int"/> bytes).<context="task log"/>',
        { path: name, bytes: r.bytes }
      );
    }
  }
  ReportProgress(1, "docs");
  PlaySignal("success");
}
_Tag(docs, {
  gulpName: "docs",
  µDisplayName: 'Compose READMEs<context="µDisplayName"/>',
  µDescription:
    'Generates README.md (en-US) and README.de-DE.md from locale .src.md sources. Does not overwrite sources.<context="µDescription"/>',
  µGroup: 'Docs & Backup<context="µGroup"/>',
  µIcon: "\uE915",
  µOrder: 10,
  µExecutionConcurrency: false,
});

export async function docsEnUS() {
  ReportProgress(0, "docs-en-US");
  const result = ComposeReadmeLocale({ root: rootDir, locale: "en-US" });
  Log(
    result.changed
      ? 'Wrote README.md (<bytes format="int"/> bytes)<context="task log"/>'
      : 'README.md already up to date (<bytes format="int"/> bytes).<context="task log"/>',
    { bytes: result.bytes }
  );
  ReportProgress(1, "docs-en-US");
  PlaySignal("success");
}
_Tag(docsEnUS, {
  gulpName: "docs:en-US",
  µDisplayName: 'Compose README (en-US)<context="µDisplayName"/>',
  µDescription:
    'Generates README.md from dev/docs/readme/en-US.src.md.<context="µDescription"/>',
  µGroup: 'Docs & Backup<context="µGroup"/>',
  µIcon: "\uE915",
  µOrder: 11,
  µExecutionConcurrency: false,
});

export async function docsDeDE() {
  ReportProgress(0, "docs-de-DE");
  const result = ComposeReadmeLocale({ root: rootDir, locale: "de-DE" });
  Log(
    result.changed
      ? 'Wrote README.de-DE.md (<bytes format="int"/> bytes)<context="task log"/>'
      : 'README.de-DE.md already up to date (<bytes format="int"/> bytes).<context="task log"/>',
    { bytes: result.bytes }
  );
  ReportProgress(1, "docs-de-DE");
  PlaySignal("success");
}
_Tag(docsDeDE, {
  gulpName: "docs:de-DE",
  µDisplayName: 'Compose README (de-DE)<context="µDisplayName"/>',
  µDescription:
    'Generates README.de-DE.md from dev/docs/readme/de-DE.src.md.<context="µDescription"/>',
  µGroup: 'Docs & Backup<context="µGroup"/>',
  µIcon: "\uE915",
  µOrder: 12,
  µExecutionConcurrency: false,
});

export async function BACKUP_GIT() {
  ReportProgress(0, "backup-git");
  const result = await RunGitBackup(rootDir, {
    log: (m) => Log(m + '<context="task log"/>'),
    warn: (m) => Warn(m + '<context="task warning"/>'),
  });
  ReportProgress(1, "backup-git");
  if (result.ok) PlaySignal("success");
  else if (result.reason === "not-a-repo") {
    Warn(
      'Git backup skipped — no .git yet. Run git init when ready.<context="task warning"/>'
    );
  } else {
    Warn('Git backup status: <reason/><context="task warning"/>', {
      reason: result.reason,
    });
  }
  return result;
}
_Tag(BACKUP_GIT, {
  gulpName: "backup:git",
  µDisplayName: 'Git backup checkpoint<context="µDisplayName"/>',
  µDescription:
    'Checkpoint commit including CLAUDE.md. Pushes when a remote exists. Never force-pushes.<context="µDescription"/>',
  µGroup: 'Docs & Backup<context="µGroup"/>',
  µIcon: "\uE902",
  µOrder: 20,
  µExecutionConcurrency: false,
});

export async function backup() {
  return _RunNasBackup();
}
_Tag(backup, {
  gulpName: "backup",
  µDisplayName: 'Backup to NAS<context="µDisplayName"/>',
  µDescription:
    'Copies non-reproducible ESP][ files to up to three NAS folders. Form picks destinations (remembered). Skips node_modules, .pio, secrets.<context="µDescription"/>',
  µTooltip:
    'Form: Destination 1–3 + dry-run. Or NAS_TARGET_1..3 / config/nas.targets.local.<context="µTooltip"/>',
  µGroup: 'Docs & Backup<context="µGroup"/>',
  µIcon: "\uE902",
  µOrder: 30,
  µExecutionConcurrency: false,
  µParameters: _NasBackupParameters(),
});

export async function BACKUP_ALL() {
  ReportProgress(0, "backup-all");
  await docs();
  ReportProgress(0.33, "backup-all");
  await BACKUP_GIT();
  ReportProgress(0.66, "backup-all");
  await backup();
  ReportProgress(1, "backup-all");
  Log('[ALL] Finished.<context="task log"/>');
}
_Tag(BACKUP_ALL, {
  gulpName: "backup:all",
  µDisplayName: 'Backup all (docs + Git + NAS)<context="µDisplayName"/>',
  µDescription: 'Runs docs, backup:git, then backup (NAS form).<context="µDescription"/>',
  µGroup: 'Docs & Backup<context="µGroup"/>',
  µIcon: "\uE902",
  µOrder: 40,
  µExecutionConcurrency: false,
});

//================================================================
// Discovery
//================================================================

export default docs;

gulp.task("backup:git", BACKUP_GIT);
gulp.task("backup:all", BACKUP_ALL);
gulp.task("docs:en-US", docsEnUS);
gulp.task("docs:de-DE", docsDeDE);
// npm script backup:nas → gulp backup (no second dashboard entry)

//================================================================
// apple2js / media (developer; network only for :sync)
//================================================================

function runNodeCli(relScript, args, label) {
  const script = join(rootDir, relScript);
  const r = spawnSync(process.execPath, [script, ...args], {
    cwd: rootDir,
    encoding: "utf8",
    shell: false,
  });
  if (r.stdout) process.stdout.write(r.stdout);
  if (r.stderr) process.stderr.write(r.stderr);
  if (r.status !== 0) {
    throw new Error(`${label} failed (exit ${r.status})`);
  }
}

export async function apple2jsSync() {
  ReportProgress(0, "apple2js-sync");
  Log(
    'Syncing apple2js cache (network)…<context="task log"/>',
  );
  const r = await syncApple2js({ fetchWebIndex: true });
  Log(
    'apple2js @ <short/> — web catalog <count/> entries.<context="task log"/>',
    { short: r.pin.short, count: String(r.webIndexCount ?? 0) },
  );
  ReportProgress(0.6, "apple2js-audit-diff");
  runNodeCli("dev/tools/apple2js/cli.mjs", ["audit", "--diff"], "apple2js:audit");
  ReportProgress(1, "apple2js-sync");
  PlaySignal("success");
}
_Tag(apple2jsSync, {
  gulpName: "apple2js:sync",
  µDisplayName: 'apple2js Sync<context="µDisplayName"/>',
  µDescription:
    'Clones/updates .cache/apple2js, fetches website index, re-audits with diff (network).<context="µDescription"/>',
  µIcon: "\u2601",
  µGroup: 'Media / apple2js<context="µGroup"/>',
  µOrder: 60,
  µExecutionConcurrency: false,
});

export async function apple2jsCatalog() {
  ReportProgress(0, "apple2js-catalog");
  runNodeCli("dev/tools/apple2js/cli.mjs", ["catalog"], "apple2js:catalog");
  ReportProgress(1, "apple2js-catalog");
}
_Tag(apple2jsCatalog, {
  gulpName: "apple2js:catalog",
  µDisplayName: 'apple2js Catalog<context="µDisplayName"/>',
  µDescription:
    'Summarize cached apple2js git+website catalog (offline if cache present).<context="µDescription"/>',
  µGroup: 'Media / apple2js<context="µGroup"/>',
  µOrder: 61,
  µExecutionConcurrency: false,
});

export async function apple2jsAudit() {
  ReportProgress(0, "apple2js-audit");
  runNodeCli("dev/tools/apple2js/cli.mjs", ["audit"], "apple2js:audit");
  ReportProgress(1, "apple2js-audit");
  PlaySignal("success");
}
_Tag(apple2jsAudit, {
  gulpName: "apple2js:audit",
  µDisplayName: 'apple2js Audit<context="µDisplayName"/>',
  µDescription:
    'Provenance audit + metadata stubs (offline if cache present).<context="µDescription"/>',
  µGroup: 'Media / apple2js<context="µGroup"/>',
  µOrder: 62,
  µExecutionConcurrency: false,
});

export async function mediaIdentify() {
  const file = GetParameter("file");
  if (!file) {
    throw new Error("media:identify requires parameter file=<path>");
  }
  runNodeCli(
    "dev/tools/media/cli.mjs",
    ["identify", "--file", file],
    "media:identify",
  );
}
_Tag(mediaIdentify, {
  gulpName: "media:identify",
  µDisplayName: 'Media Identify<context="µDisplayName"/>',
  µDescription:
    'Offline SHA-256 + catalog match for a user disk image.<context="µDescription"/>',
  µGroup: 'Media / apple2js<context="µGroup"/>',
  µOrder: 63,
  µParameters: {
    file: {
      type: "string",
      description: 'Path to .dsk/.po/.nib/.woz/…<context="µParameter"/>',
    },
  },
  µExecutionConcurrency: false,
});

export async function mediaImport() {
  const file = GetParameter("file");
  const directory = GetParameter("directory");
  const library =
    GetParameter("library") || GetParameter("target") || "library/user";
  const title = GetParameter("title");
  const gameId = GetParameter("gameId") || GetParameter("game-id");
  const dryRun = GetParameter("dryRun") === true || GetParameter("dry-run") === true;
  if (!file && !directory) {
    throw new Error("media:import requires file= or directory=");
  }
  const args = ["import", "--library", library];
  if (file) args.push("--file", file);
  if (directory) args.push("--directory", directory);
  if (title) args.push("--title", title);
  if (gameId) args.push("--game-id", gameId);
  if (dryRun) args.push("--dry-run");
  runNodeCli("dev/tools/media/cli.mjs", args, "media:import");
}
_Tag(mediaImport, {
  gulpName: "media:import",
  µDisplayName: 'Media Import<context="µDisplayName"/>',
  µDescription:
    'Import user-supplied disk(s) into a library path (offline; never downloads).<context="µDescription"/>',
  µGroup: 'Media / apple2js<context="µGroup"/>',
  µOrder: 64,
  µParameters: {
    file: { type: "string" },
    directory: { type: "string" },
    library: { type: "string" },
    title: { type: "string" },
    gameId: { type: "string" },
    dryRun: { type: "boolean" },
  },
  µExecutionConcurrency: false,
});

export async function mediaInspect() {
  const file = GetParameter("file");
  if (!file) throw new Error("media:inspect requires file=");
  runNodeCli(
    "dev/tools/media/inspect_a2kit.mjs",
    ["--file", file],
    "media:inspect",
  );
}
_Tag(mediaInspect, {
  gulpName: "media:inspect",
  µDisplayName: 'Media Inspect (a2kit)<context="µDisplayName"/>',
  µDescription:
    'Optional external a2kit oracle if installed; otherwise ESP][ identify only.<context="µDescription"/>',
  µGroup: 'Media / apple2js<context="µGroup"/>',
  µOrder: 65,
  µParameters: { file: { type: "string" } },
  µExecutionConcurrency: false,
});

export async function sdPrepare() {
  const target = GetParameter("target");
  if (!target) {
    throw new Error("sd:prepare requires explicit target=<path>");
  }
  // Default dry-run=true; set dryRun=false to actually copy.
  const dry = !(
    GetParameter("dryRun") === false || GetParameter("dry-run") === false
  );
  const confirmOverwrite =
    GetParameter("confirmOverwrite") === true ||
    GetParameter("confirm-overwrite") === true;
  const { prepareSdLibrary } = await import("./dev/tools/sd/prepare.mjs");
  const result = prepareSdLibrary({
    sourceLibrary: join(rootDir, "library"),
    targetRoot: target,
    dryRun: dry,
    confirmOverwrite,
  });
  console.log(JSON.stringify(result, null, 2));
  if (!dry) PlaySignal("success");
}
_Tag(sdPrepare, {
  gulpName: "sd:prepare",
  µDisplayName: 'SD Prepare Library<context="µDisplayName"/>',
  µDescription:
    'Copy host library into target/apple2 (default dry-run; never formats drives).<context="µDescription"/>',
  µGroup: 'Media / apple2js<context="µGroup"/>',
  µOrder: 66,
  µParameters: {
    target: { type: "string" },
    dryRun: { type: "boolean" },
    confirmOverwrite: { type: "boolean" },
  },
  µExecutionConcurrency: false,
});

export async function apple2RomIdentify() {
  ReportProgress(0, "apple2-rom-identify");
  const rom = GetParameter("rom") || "";
  const args = ["--identify-only"];
  if (rom) args.push("--rom", rom);
  runNodeCli("host/tools/rom_test.mjs", args, "apple2:rom-identify");
  ReportProgress(1, "apple2-rom-identify");
}
_Tag(apple2RomIdentify, {
  gulpName: "apple2:rom-identify",
  µDisplayName: 'Apple II ROM Identify<context="µDisplayName"/>',
  µDescription:
    'Hash/identify a user-supplied motherboard ROM (no download). SKIPPED_NO_ROM if absent.<context="µDescription"/>',
  µGroup: 'Apple II Host<context="µGroup"/>',
  µOrder: 50,
  µParameters: [{ name: "rom", type: "string", optional: true }],
});

export async function apple2RomTest() {
  ReportProgress(0, "apple2-rom-test");
  const rom = GetParameter("rom") || "";
  const args = [];
  if (rom) args.push("--rom", rom);
  runNodeCli("host/tools/rom_test.mjs", args, "apple2:rom-test");
  ReportProgress(1, "apple2-rom-test");
}
_Tag(apple2RomTest, {
  gulpName: "apple2:rom-test",
  µDisplayName: 'Apple II ROM Test<context="µDisplayName"/>',
  µDescription:
    'Optional bounded host run with user ROM; SKIPPED_NO_ROM without local ROM.<context="µDescription"/>',
  µGroup: 'Apple II Host<context="µGroup"/>',
  µOrder: 51,
  µParameters: [{ name: "rom", type: "string", optional: true }],
});

export async function apple2Host() {
  ReportProgress(0, "apple2-host");
  const rom = GetParameter("rom") || "";
  // Ensure built
  runNodeCli("host/tools/build_and_test_apple2.mjs", [], "apple2:host-build");
  const exe = join(rootDir, "host/.out/esp2_host.exe");
  const args = ["--diagnostics", "--text", "--batch", "--cycles", "100000"];
  if (rom) args.push("--rom", rom);
  const r = spawnSync(exe, args, { cwd: rootDir, encoding: "utf8", shell: false });
  if (r.stdout) process.stdout.write(r.stdout);
  if (r.stderr) process.stderr.write(r.stderr);
  if (r.status !== 0) throw new Error(`apple2:host failed (${r.status})`);
  ReportProgress(1, "apple2-host");
}
_Tag(apple2Host, {
  gulpName: "apple2:host",
  µDisplayName: 'Apple II Host Runner<context="µDisplayName"/>',
  µDescription:
    'Build and batch-run esp2_host (synthetic ROM unless --rom given).<context="µDescription"/>',
  µGroup: 'Apple II Host<context="µGroup"/>',
  µOrder: 52,
  µParameters: [{ name: "rom", type: "string", optional: true }],
});

gulp.task("apple2:rom-identify", apple2RomIdentify);
gulp.task("apple2:rom-test", apple2RomTest);
gulp.task("apple2:host", apple2Host);

export async function apple2DiskTest() {
  ReportProgress(0, "apple2-disk-test");
  const disk = GetParameter("disk") || "";
  const rom = GetParameter("rom") || "";
  const slot6 = GetParameter("slot6Rom") || GetParameter("slot6-rom") || "";
  runNodeCli("host/tools/build_and_test_apple2.mjs", [], "apple2:disk-test-build");
  const exe = join(rootDir, "host/.out/esp2_host.exe");
  if (!disk) {
    console.log("SKIPPED_NO_DISK  pass --disk <path-or-Esp2BootTest>");
    ReportProgress(1, "apple2-disk-test");
    return;
  }
  const args = ["--diagnostics", "--text", "--batch", "--cycles", "2000000", "--disk1", disk];
  if (rom) args.push("--rom", rom);
  if (slot6) args.push("--slot6-rom", slot6);
  else if (disk.includes("Esp2BootTest")) args.push("--slot6", "cleanroom");
  const r = spawnSync(exe, args, { cwd: rootDir, encoding: "utf8", shell: false });
  if (r.stdout) process.stdout.write(r.stdout);
  if (r.stderr) process.stderr.write(r.stderr);
  if (r.status !== 0) throw new Error(`apple2:disk-test failed (${r.status})`);
  ReportProgress(1, "apple2-disk-test");
}
_Tag(apple2DiskTest, {
  gulpName: "apple2:disk-test",
  µDisplayName: 'Apple II Disk Test<context="µDisplayName"/>',
  µDescription:
    'Optional bounded Disk II boot/diagnostics; SKIPPED_NO_DISK without --disk. Does not download media.<context="µDescription"/>',
  µGroup: 'Apple II Host<context="µGroup"/>',
  µOrder: 53,
  µParameters: [
    { name: "disk", type: "string", optional: true },
    { name: "rom", type: "string", optional: true },
    { name: "slot6Rom", type: "string", optional: true },
  ],
});

gulp.task("apple2:disk-test", apple2DiskTest);

export async function apple2Compat() {
  ReportProgress(0, "apple2-compat");
  const test = GetParameter("test") || "";
  const run = GetParameter("run") === true;
  const config = GetParameter("config") || "";
  const rom = GetParameter("rom") || "";
  const slot6Rom = GetParameter("slot6Rom") || GetParameter("slot6-rom") || "";
  const disk1 = GetParameter("disk1") || "";
  if (!test) {
    runNodeCli("host/tools/compat_runner.mjs", ["--list"], "apple2:compat-list");
    ReportProgress(1, "apple2-compat");
    return;
  }
  const args = ["--test", test];
  if (run) args.push("--run");
  if (config) args.push("--config", config);
  if (rom) args.push("--rom", rom);
  if (slot6Rom) args.push("--slot6-rom", slot6Rom);
  if (disk1) args.push("--disk1", disk1);
  runNodeCli("host/tools/compat_runner.mjs", args, "apple2:compat");
  ReportProgress(1, "apple2-compat");
}
_Tag(apple2Compat, {
  gulpName: "apple2:compat",
  µDisplayName: 'Apple II Compatibility Harness<context="µDisplayName"/>',
  µDescription:
    'Run real-software compatibility test definitions; SKIPPED_* without user assets. No downloads.<context="µDescription"/>',
  µGroup: 'Apple II Host<context="µGroup"/>',
  µOrder: 54,
  µParameters: [
    { name: "test", type: "string", optional: true },
    { name: "run", type: "boolean", optional: true },
    { name: "config", type: "string", optional: true },
    { name: "rom", type: "string", optional: true },
    { name: "slot6Rom", type: "string", optional: true },
    { name: "disk1", type: "string", optional: true },
  ],
});

gulp.task("apple2:compat", apple2Compat);