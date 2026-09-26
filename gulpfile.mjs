//================================================================
// ESP][ â ÂµGulp / Gulp task runner
// Â© 2026 Meinolf Amekudzi
//
// Groups:
//   Firmware     â build / flash / upload / clean / size
//   Tools        â devices / monitor
//   Docs & Backup â docs / backup:git / backup / backup:all
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

export const ÂµI18xContext = { project: "esp2", product: "ESP][" };

const rootDir = dirname(fileURLToPath(import.meta.url));

/**
 * @param {Function} _task
 * @param {object} _meta
 */
function _Tag(_task, _meta) {
  if (_meta.gulpName) _task.displayName = _meta.gulpName;
  if (_meta.ÂµDisplayName) _task.ÂµDisplayName = _meta.ÂµDisplayName.i18xRegister();
  if (_meta.ÂµDescription) _task.ÂµDescription = _meta.ÂµDescription.i18xRegister();
  if (_meta.ÂµTooltip) _task.ÂµTooltip = _meta.ÂµTooltip.i18xRegister();
  if (_meta.ÂµGroup) _task.ÂµGroup = _meta.ÂµGroup.i18xRegister();
  if (_meta.ÂµIcon != null) _task.ÂµIcon = _meta.ÂµIcon;
  if (_meta.ÂµOrder != null) _task.ÂµOrder = _meta.ÂµOrder;
  if (_meta.ÂµExecutionConcurrency != null) {
    _task.ÂµExecutionConcurrency = _meta.ÂµExecutionConcurrency;
  }
  if (_meta.ÂµExecutionRestrictions) {
    _task.ÂµExecutionRestrictions = _meta.ÂµExecutionRestrictions;
  }
  if (_meta.ÂµParameters) _task.ÂµParameters = _meta.ÂµParameters;
  return _task;
}

//================================================================
// Firmware
//================================================================

export async function build() {
  ReportProgress(0, "build");
  Log('Building ESP][ firmware (env <env/>)â¦<context="task log"/>', {
    env: PIO_ENV,
  });
  await Pio(rootDir, ["run", "-e", PIO_ENV], "pio build");
  ReportProgress(1, "build");
  Log('Build OK.<context="task log"/>');
  PlaySignal("success");
}
_Tag(build, {
  gulpName: "build",
  ÂµDisplayName: 'Build Firmware<context="ÂµDisplayName"/>',
  ÂµDescription:
    'Compiles ESP][ with PlatformIO (env: bringup). Does not change board config.<context="ÂµDescription"/>',
  ÂµIcon: "\u2692",
  ÂµGroup: 'Firmware<context="ÂµGroup"/>',
  ÂµOrder: 10,
  ÂµExecutionConcurrency: false,
});

export async function flash() {
  ReportProgress(0, "flash-build");
  const port = await AskPort(rootDir, "Flash port (build + upload)");
  Log('Build + upload â <port/><context="task log"/>â¦', {
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
  ÂµDisplayName: 'Build & Upload<context="ÂµDisplayName"/>',
  ÂµDescription:
    'Rebuilds and flashes ESP][ firmware in one step.<context="ÂµDescription"/>',
  ÂµTooltip:
    'Set ESP2_PORT=COMx to skip the port dialog.<context="ÂµTooltip"/>',
  ÂµIcon: "\u26A1",
  ÂµGroup: 'Firmware<context="ÂµGroup"/>',
  ÂµOrder: 15,
  ÂµExecutionConcurrency: false,
  ÂµExecutionRestrictions: { deny: ["upload", "build", "backup"] },
});

export async function upload() {
  ReportProgress(0, "upload");
  const port = await AskPort(rootDir, "Upload port");
  Log('Uploading â <port/><context="task log"/>â¦', {
    port: port ?? "auto/ini",
  });
  await Pio(rootDir, UploadArgs(port), "pio upload");
  ReportProgress(1, "upload");
  Log('Upload finished.<context="task log"/>');
  PlaySignal("success");
}
_Tag(upload, {
  gulpName: "upload",
  ÂµDisplayName: 'Upload Firmware<context="ÂµDisplayName"/>',
  ÂµDescription:
    'Flashes the last build. Asks for the COM port.<context="ÂµDescription"/>',
  ÂµTooltip:
    'Set ESP2_PORT=COMx to skip the port dialog.<context="ÂµTooltip"/>',
  ÂµIcon: "\u2191",
  ÂµGroup: 'Firmware<context="ÂµGroup"/>',
  ÂµOrder: 20,
  ÂµExecutionConcurrency: false,
  ÂµExecutionRestrictions: { deny: ["flash", "monitor", "backup"] },
});

export async function size() {
  ReportProgress(0, "size");
  await Pio(rootDir, ["run", "-e", PIO_ENV, "-t", "size"], "pio size");
  ReportProgress(1, "size");
}
_Tag(size, {
  gulpName: "size",
  ÂµDisplayName: 'Memory Size<context="ÂµDisplayName"/>',
  ÂµDescription:
    'Shows RAM / Flash usage of the last firmware build.<context="ÂµDescription"/>',
  ÂµIcon: "\u25A6",
  ÂµGroup: 'Firmware<context="ÂµGroup"/>',
  ÂµOrder: 30,
  ÂµExecutionConcurrency: true,
});

export async function clean() {
  ReportProgress(0, "clean");
  await Pio(rootDir, ["run", "-e", PIO_ENV, "-t", "clean"], "pio clean");
  ReportProgress(1, "clean");
  Log('Build artefacts removed.<context="task log"/>');
}
_Tag(clean, {
  gulpName: "clean",
  ÂµDisplayName: 'Clean Build<context="ÂµDisplayName"/>',
  ÂµDescription:
    'Removes .pio/build artefacts for env bringup.<context="ÂµDescription"/>',
  ÂµIcon: "\u239A",
  ÂµGroup: 'Firmware<context="ÂµGroup"/>',
  ÂµOrder: 40,
  ÂµExecutionConcurrency: true,
});

export const rebuild = gulp.series(clean, build);
_Tag(rebuild, {
  gulpName: "rebuild",
  ÂµDisplayName: 'Rebuild Firmware<context="ÂµDisplayName"/>',
  ÂµDescription: 'clean â build (PlatformIO env bringup).<context="ÂµDescription"/>',
  ÂµIcon: "\u21BB",
  ÂµGroup: 'Firmware<context="ÂµGroup"/>',
  ÂµOrder: 12,
  ÂµExecutionConcurrency: false,
  ÂµExecutionRestrictions: { deny: ["flash", "upload", "backup"] },
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
  ÂµDisplayName: 'List Devices<context="ÂµDisplayName"/>',
  ÂµDescription: 'Lists connected serial ports.<context="ÂµDescription"/>',
  ÂµIcon: "\u2398",
  ÂµGroup: 'Tools<context="ÂµGroup"/>',
  ÂµOrder: 10,
  ÂµExecutionConcurrency: true,
});

export async function monitor() {
  const port = await AskPort(rootDir, "Serial monitor port");
  Log('Opening serial monitor (Ctrl+C to stop)â¦<context="task log"/>');
  await Pio(rootDir, MonitorArgs(port), "pio monitor");
}
_Tag(monitor, {
  gulpName: "monitor",
  ÂµDisplayName: 'Serial Monitor<context="ÂµDisplayName"/>',
  ÂµDescription:
    'Opens the PlatformIO serial monitor at 115200 baud.<context="ÂµDescription"/>',
  ÂµTooltip:
    'Set ESP2_PORT=COMx to skip the port dialog. Free COM5 if another monitor holds it.<context="ÂµTooltip"/>',
  ÂµIcon: "\u2399",
  ÂµGroup: 'Tools<context="ÂµGroup"/>',
  ÂµOrder: 20,
  ÂµExecutionConcurrency: false,
});

//================================================================
// Docs & Backup â NAS form (microGulp / Watchy)
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
          'Primary backup folder. Leave empty to skip. Up to three destinations. ÂµGulp remembers answers; optional defaults from config/nas.targets.local.<context="task parameter"/>'.i18xRegister(),
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
    // ÂµParameters already collected by dashboard before task body
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
      '[NAS] 0 destinations â use the form, set NAS_TARGET_1..3, or config/nas.targets.local.<context="task log"/>'
    );
    ReportProgress(1, "backup-nas");
    PlaySignal("success");
    return { ok: true, results: [], dryRun: resolved.dryRun };
  }

  if (resolved.dryRun) {
    Log('[NAS] DRY-RUN â preview only.<context="task log"/>');
  }

  /** @type {{ index: number, path: string, status: string, detail?: string }[]} */
  const results = [];

  for (let i = 0; i < resolved.destinations.length; i += 1) {
    const label = `NAS${i + 1}`;
    const destination = resolved.destinations[i];
    ReportProgress(i / resolved.destinations.length, "backup-nas");
    try {
      if (!existsSync(destination)) {
        Warn(`[${label}] unavailable â skipped (${destination})<context="task warning"/>`);
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
      Log(`[${label}] Copying â <path/><context="task log"/>`, { path: destResolved });
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
        `[${label}] unavailable/failed â skipped (${err.message})<context="task warning"/>`
      );
      results.push({
        index: i + 1,
        path: destination,
        status: "failed",
        detail: err.message,
      });
    }
  }

  Log("â NAS backup summary â<context=\"task log\"/>");
  for (const r of results) {
    Log(
      `[NAS${r.index}] ${r.status}${r.detail ? " â " + r.detail : ""} â ${r.path}<context="task log"/>`
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
  ÂµDisplayName: 'Compose READMEs<context="ÂµDisplayName"/>',
  ÂµDescription:
    'Generates README.md (en-US) and README.de-DE.md from locale .src.md sources. Does not overwrite sources.<context="ÂµDescription"/>',
  ÂµGroup: 'Docs & Backup<context="ÂµGroup"/>',
  ÂµIcon: "\uE915",
  ÂµOrder: 10,
  ÂµExecutionConcurrency: false,
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
  ÂµDisplayName: 'Compose README (en-US)<context="ÂµDisplayName"/>',
  ÂµDescription:
    'Generates README.md from dev/docs/readme/en-US.src.md.<context="ÂµDescription"/>',
  ÂµGroup: 'Docs & Backup<context="ÂµGroup"/>',
  ÂµIcon: "\uE915",
  ÂµOrder: 11,
  ÂµExecutionConcurrency: false,
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
  ÂµDisplayName: 'Compose README (de-DE)<context="ÂµDisplayName"/>',
  ÂµDescription:
    'Generates README.de-DE.md from dev/docs/readme/de-DE.src.md.<context="ÂµDescription"/>',
  ÂµGroup: 'Docs & Backup<context="ÂµGroup"/>',
  ÂµIcon: "\uE915",
  ÂµOrder: 12,
  ÂµExecutionConcurrency: false,
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
      'Git backup skipped â no .git yet. Run git init when ready.<context="task warning"/>'
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
  ÂµDisplayName: 'Git backup checkpoint<context="ÂµDisplayName"/>',
  ÂµDescription:
    'Checkpoint commit including CLAUDE.md. Pushes when a remote exists. Never force-pushes.<context="ÂµDescription"/>',
  ÂµGroup: 'Docs & Backup<context="ÂµGroup"/>',
  ÂµIcon: "\uE902",
  ÂµOrder: 20,
  ÂµExecutionConcurrency: false,
});

export async function backup() {
  return _RunNasBackup();
}
_Tag(backup, {
  gulpName: "backup",
  ÂµDisplayName: 'Backup to NAS<context="ÂµDisplayName"/>',
  ÂµDescription:
    'Copies non-reproducible ESP][ files to up to three NAS folders. Form picks destinations (remembered). Skips node_modules, .pio, secrets.<context="ÂµDescription"/>',
  ÂµTooltip:
    'Form: Destination 1â3 + dry-run. Or NAS_TARGET_1..3 / config/nas.targets.local.<context="ÂµTooltip"/>',
  ÂµGroup: 'Docs & Backup<context="ÂµGroup"/>',
  ÂµIcon: "\uE902",
  ÂµOrder: 30,
  ÂµExecutionConcurrency: false,
  ÂµParameters: _NasBackupParameters(),
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
  ÂµDisplayName: 'Backup all (docs + Git + NAS)<context="ÂµDisplayName"/>',
  ÂµDescription: 'Runs docs, backup:git, then backup (NAS form).<context="ÂµDescription"/>',
  ÂµGroup: 'Docs & Backup<context="ÂµGroup"/>',
  ÂµIcon: "\uE902",
  ÂµOrder: 40,
  ÂµExecutionConcurrency: false,
});

//================================================================
// Discovery
//================================================================

export default docs;

gulp.task("backup:git", BACKUP_GIT);
gulp.task("backup:all", BACKUP_ALL);
gulp.task("docs:en-US", docsEnUS);
gulp.task("docs:de-DE", docsDeDE);
// npm script backup:nas â gulp backup (no second dashboard entry)

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
    'Syncing apple2js cache (network)â¦<context="task log"/>',
  );
  const r = await syncApple2js({ fetchWebIndex: true });
  Log(
    'apple2js @ <short/> â web catalog <count/> entries.<context="task log"/>',
    { short: r.pin.short, count: String(r.webIndexCount ?? 0) },
  );
  ReportProgress(0.6, "apple2js-audit-diff");
  runNodeCli("dev/tools/apple2js/cli.mjs", ["audit", "--diff"], "apple2js:audit");
  ReportProgress(1, "apple2js-sync");
  PlaySignal("success");
}
_Tag(apple2jsSync, {
  gulpName: "apple2js:sync",
  ÂµDisplayName: 'apple2js Sync<context="ÂµDisplayName"/>',
  ÂµDescription:
    'Clones/updates .cache/apple2js, fetches website index, re-audits with diff (network).<context="ÂµDescription"/>',
  ÂµIcon: "\u2601",
  ÂµGroup: 'Media / apple2js<context="ÂµGroup"/>',
  ÂµOrder: 60,
  ÂµExecutionConcurrency: false,
});

export async function apple2jsCatalog() {
  ReportProgress(0, "apple2js-catalog");
  runNodeCli("dev/tools/apple2js/cli.mjs", ["catalog"], "apple2js:catalog");
  ReportProgress(1, "apple2js-catalog");
}
_Tag(apple2jsCatalog, {
  gulpName: "apple2js:catalog",
  ÂµDisplayName: 'apple2js Catalog<context="ÂµDisplayName"/>',
  ÂµDescription:
    'Summarize cached apple2js git+website catalog (offline if cache present).<context="ÂµDescription"/>',
  ÂµGroup: 'Media / apple2js<context="ÂµGroup"/>',
  ÂµOrder: 61,
  ÂµExecutionConcurrency: false,
});

export async function apple2jsAudit() {
  ReportProgress(0, "apple2js-audit");
  runNodeCli("dev/tools/apple2js/cli.mjs", ["audit"], "apple2js:audit");
  ReportProgress(1, "apple2js-audit");
  PlaySignal("success");
}
_Tag(apple2jsAudit, {
  gulpName: "apple2js:audit",
  ÂµDisplayName: 'apple2js Audit<context="ÂµDisplayName"/>',
  ÂµDescription:
    'Provenance audit + metadata stubs (offline if cache present).<context="ÂµDescription"/>',
  ÂµGroup: 'Media / apple2js<context="ÂµGroup"/>',
  ÂµOrder: 62,
  ÂµExecutionConcurrency: false,
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
  ÂµDisplayName: 'Media Identify<context="ÂµDisplayName"/>',
  ÂµDescription:
    'Offline SHA-256 + catalog match for a user disk image.<context="ÂµDescription"/>',
  ÂµGroup: 'Media / apple2js<context="ÂµGroup"/>',
  ÂµOrder: 63,
  ÂµParameters: {
    file: {
      type: "string",
      description: 'Path to .dsk/.po/.nib/.woz/â¦<context="ÂµParameter"/>',
    },
  },
  ÂµExecutionConcurrency: false,
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
  ÂµDisplayName: 'Media Import<context="ÂµDisplayName"/>',
  ÂµDescription:
    'Import user-supplied disk(s) into a library path (offline; never downloads).<context="ÂµDescription"/>',
  ÂµGroup: 'Media / apple2js<context="ÂµGroup"/>',
  ÂµOrder: 64,
  ÂµParameters: {
    file: { type: "string" },
    directory: { type: "string" },
    library: { type: "string" },
    title: { type: "string" },
    gameId: { type: "string" },
    dryRun: { type: "boolean" },
  },
  ÂµExecutionConcurrency: false,
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
  ÂµDisplayName: 'Media Inspect (a2kit)<context="ÂµDisplayName"/>',
  ÂµDescription:
    'Optional external a2kit oracle if installed; otherwise ESP][ identify only.<context="ÂµDescription"/>',
  ÂµGroup: 'Media / apple2js<context="ÂµGroup"/>',
  ÂµOrder: 65,
  ÂµParameters: { file: { type: "string" } },
  ÂµExecutionConcurrency: false,
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

//================================================================
// Local Apple II media (gitignored local/apple2/)
//================================================================

function cliArg(name) {
  const argv = process.argv;
  const i = argv.indexOf(`--${name}`);
  if (i >= 0 && argv[i + 1] && !argv[i + 1].startsWith("--")) return argv[i + 1];
  const eq = argv.find((a) => a.startsWith(`--${name}=`));
  if (eq) return eq.slice(name.length + 3);
  try {
    const p = GetParameter(name);
    if (p !== undefined && p !== null && p !== "") return p;
  } catch {
    /* ignore */
  }
  return null;
}

function cliFlag(name) {
  if (process.argv.includes(`--${name}`)) return true;
  try {
    return GetParameter(name) === true;
  } catch {
    return false;
  }
}

export async function apple2RomSync() {
  ReportProgress(0, "apple2-rom-sync");
  runNodeCli("dev/tools/apple2-local/cli.mjs", ["rom-sync"], "apple2:rom:sync");
  ReportProgress(1, "apple2-rom-sync");
  PlaySignal("success");
}
_Tag(apple2RomSync, {
  gulpName: "apple2:rom:sync",
  µDisplayName: 'Apple II ROM Sync<context="µDisplayName"/>',
  µDescription:
    'Download ONLY redistributable ROMs (e.g. AppleIIGo) into gitignored local/apple2/roms/.<context="µDescription"/>',
  µGroup: 'Media / local apple2<context="µGroup"/>',
  µOrder: 70,
  µExecutionConcurrency: false,
});

export async function apple2MediaSync() {
  ReportProgress(0, "apple2-media-sync");
  const title = cliArg("title") || "";
  const all = cliFlag("all");
  const args = ["media-sync"];
  if (all) args.push("--all");
  else if (title) args.push("--title", title);
  else {
    throw new Error(
      "apple2:media:sync requires --title <id> or --all (complete LOCAL_TEST_ONLY cache)",
    );
  }
  runNodeCli("dev/tools/apple2-local/cli.mjs", args, "apple2:media:sync");
  ReportProgress(1, "apple2-media-sync");
  PlaySignal("success");
}
_Tag(apple2MediaSync, {
  gulpName: "apple2:media:sync",
  µDisplayName: 'Apple II Media Sync<context="µDisplayName"/>',
  µDescription:
    'Fetch apple2js title(s) into gitignored local/apple2/cache/ (LOCAL_TEST_ONLY; never stages Git).<context="µDescription"/>',
  µGroup: 'Media / local apple2<context="µGroup"/>',
  µOrder: 71,
  µParameters: {
    title: { type: "string", optional: true },
    all: { type: "boolean", optional: true },
  },
  µExecutionConcurrency: false,
});

export async function apple2MediaPrepare() {
  ReportProgress(0, "apple2-media-prepare");
  const title = cliArg("title") || "";
  if (!title) throw new Error("apple2:media:prepare requires --title <id>");
  runNodeCli(
    "dev/tools/apple2-local/cli.mjs",
    ["media-prepare", "--title", title],
    "apple2:media:prepare",
  );
  ReportProgress(1, "apple2-media-prepare");
}
_Tag(apple2MediaPrepare, {
  gulpName: "apple2:media:prepare",
  µDisplayName: 'Apple II Media Prepare<context="µDisplayName"/>',
  µDescription:
    'Normalize cached source to local/apple2/disks/ (offline; prefers DSK when lossless).<context="µDescription"/>',
  µGroup: 'Media / local apple2<context="µGroup"/>',
  µOrder: 72,
  µParameters: { title: { type: "string" } },
  µExecutionConcurrency: false,
});

export async function apple2MediaStatus() {
  runNodeCli(
    "dev/tools/apple2-local/cli.mjs",
    ["media-status"],
    "apple2:media:status",
  );
}
_Tag(apple2MediaStatus, {
  gulpName: "apple2:media:status",
  µDisplayName: 'Apple II Media Status<context="µDisplayName"/>',
  µDescription:
    'Compact table of local titles: downloaded / prepared / provenance.<context="µDescription"/>',
  µGroup: 'Media / local apple2<context="µGroup"/>',
  µOrder: 73,
  µExecutionConcurrency: false,
});

export async function apple2MediaList() {
  runNodeCli(
    "dev/tools/apple2-local/cli.mjs",
    ["media-list"],
    "apple2:media:list",
  );
}
_Tag(apple2MediaList, {
  gulpName: "apple2:media:list",
  µDisplayName: 'Apple II Media List<context="µDisplayName"/>',
  µDescription: 'JSON list of locally cached titles.<context="µDescription"/>',
  µGroup: 'Media / local apple2<context="µGroup"/>',
  µOrder: 74,
  µExecutionConcurrency: false,
});

export async function apple2MediaAudit() {
  runNodeCli(
    "dev/tools/apple2-local/cli.mjs",
    ["media-audit"],
    "apple2:media:audit",
  );
}
_Tag(apple2MediaAudit, {
  gulpName: "apple2:media:audit",
  µDisplayName: 'Apple II Media Audit<context="µDisplayName"/>',
  µDescription:
    'Provenance counts for local cache (download never upgrades rights).<context="µDescription"/>',
  µGroup: 'Media / local apple2<context="µGroup"/>',
  µOrder: 75,
  µExecutionConcurrency: false,
});

export async function apple2MediaClean() {
  const args = ["media-clean"];
  if (cliFlag("cache")) args.push("--cache");
  if (cliFlag("user")) args.push("--user");
  runNodeCli("dev/tools/apple2-local/cli.mjs", args, "apple2:media:clean");
}
_Tag(apple2MediaClean, {
  gulpName: "apple2:media:clean",
  µDisplayName: 'Apple II Media Clean<context="µDisplayName"/>',
  µDescription:
    'Default: remove generated disks/. Optional --cache / --user (never deletes user media by default).<context="µDescription"/>',
  µGroup: 'Media / local apple2<context="µGroup"/>',
  µOrder: 76,
  µParameters: {
    cache: { type: "boolean", optional: true },
    user: { type: "boolean", optional: true },
  },
  µExecutionConcurrency: false,
});

export async function apple2DeviceSync() {
  ReportProgress(0, "apple2-device-sync");
  const title = cliArg("title") || "";
  const port = cliArg("port") || process.env.ESP2_PORT || "";
  if (!title) throw new Error("apple2:device:sync requires --title <id>");
  if (!port) throw new Error("apple2:device:sync requires --port COMx");
  const args = ["device-sync", "--title", title, "--port", port];
  const romId = cliArg("romId") || cliArg("rom-id");
  if (romId) args.push("--rom-id", romId);
  runNodeCli("dev/tools/apple2-local/cli.mjs", args, "apple2:device:sync");
  ReportProgress(1, "apple2-device-sync");
  PlaySignal("success");
}
_Tag(apple2DeviceSync, {
  gulpName: "apple2:device:sync",
  µDisplayName: 'Apple II Device Sync<context="µDisplayName"/>',
  µDescription:
    'Upload ONLY selected title runtime ROM+disk via serial ESPU (not full catalog).<context="µDescription"/>',
  µGroup: 'Media / local apple2<context="µGroup"/>',
  µOrder: 77,
  µParameters: {
    title: { type: "string" },
    port: { type: "string" },
    romId: { type: "string", optional: true },
  },
  µExecutionConcurrency: false,
});

gulp.task("apple2:rom:sync", apple2RomSync);
gulp.task("apple2:media:sync", apple2MediaSync);
gulp.task("apple2:media:prepare", apple2MediaPrepare);
gulp.task("apple2:media:status", apple2MediaStatus);
gulp.task("apple2:media:list", apple2MediaList);
gulp.task("apple2:media:audit", apple2MediaAudit);
gulp.task("apple2:media:clean", apple2MediaClean);
gulp.task("apple2:device:sync", apple2DeviceSync);

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
  ÂµDisplayName: 'Apple II ROM Identify<context="ÂµDisplayName"/>',
  ÂµDescription:
    'Hash/identify a user-supplied motherboard ROM (no download). SKIPPED_NO_ROM if absent.<context="ÂµDescription"/>',
  ÂµGroup: 'Apple II Host<context="ÂµGroup"/>',
  ÂµOrder: 50,
  ÂµParameters: [{ name: "rom", type: "string", optional: true }],
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
  ÂµDisplayName: 'Apple II ROM Test<context="ÂµDisplayName"/>',
  ÂµDescription:
    'Optional bounded host run with user ROM; SKIPPED_NO_ROM without local ROM.<context="ÂµDescription"/>',
  ÂµGroup: 'Apple II Host<context="ÂµGroup"/>',
  ÂµOrder: 51,
  ÂµParameters: [{ name: "rom", type: "string", optional: true }],
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
  ÂµDisplayName: 'Apple II Host Runner<context="ÂµDisplayName"/>',
  ÂµDescription:
    'Build and batch-run esp2_host (synthetic ROM unless --rom given).<context="ÂµDescription"/>',
  ÂµGroup: 'Apple II Host<context="ÂµGroup"/>',
  ÂµOrder: 52,
  ÂµParameters: [{ name: "rom", type: "string", optional: true }],
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
  ÂµDisplayName: 'Apple II Disk Test<context="ÂµDisplayName"/>',
  ÂµDescription:
    'Optional bounded Disk II boot/diagnostics; SKIPPED_NO_DISK without --disk. Does not download media.<context="ÂµDescription"/>',
  ÂµGroup: 'Apple II Host<context="ÂµGroup"/>',
  ÂµOrder: 53,
  ÂµParameters: [
    { name: "disk", type: "string", optional: true },
    { name: "rom", type: "string", optional: true },
    { name: "slot6Rom", type: "string", optional: true },
  ],
});

gulp.task("apple2:disk-test", apple2DiskTest);

export async function apple2Compat() {
  ReportProgress(0, "apple2-compat");
  const test = cliArg("test") || GetParameter("test") || "";
  const run = cliFlag("run") || GetParameter("run") === true;
  const config = cliArg("config") || GetParameter("config") || "";
  const rom = cliArg("rom") || GetParameter("rom") || "";
  const slot6Rom =
    cliArg("slot6Rom") ||
    cliArg("slot6-rom") ||
    GetParameter("slot6Rom") ||
    GetParameter("slot6-rom") ||
    "";
  const disk1 = cliArg("disk1") || GetParameter("disk1") || "";
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
  ÂµDisplayName: 'Apple II Compatibility Harness<context="ÂµDisplayName"/>',
  ÂµDescription:
    'Run real-software compatibility test definitions; SKIPPED_* without user assets. No downloads.<context="ÂµDescription"/>',
  ÂµGroup: 'Apple II Host<context="ÂµGroup"/>',
  ÂµOrder: 54,
  ÂµParameters: [
    { name: "test", type: "string", optional: true },
    { name: "run", type: "boolean", optional: true },
    { name: "config", type: "string", optional: true },
    { name: "rom", type: "string", optional: true },
    { name: "slot6Rom", type: "string", optional: true },
    { name: "disk1", type: "string", optional: true },
  ],
});

gulp.task("apple2:compat", apple2Compat);
//================================================================
// Device media transfer (development)
//================================================================

export async function deviceUpload() {
  ReportProgress(0, "device-upload");
  const port = GetParameter("port") || process.env.ESP2_PORT || "";
  const file = GetParameter("file") || "";
  const target = GetParameter("target") || "";
  const verify = GetParameter("verify") || "";
  if (!port) {
    throw new Error("device:upload requires --port COMx (no arbitrary auto-pick)");
  }
  const args = ["dev/tools/esp2-upload.mjs", "--port", port];
  if (verify) {
    args.push("--verify", verify);
  } else {
    if (!file || !target) {
      throw new Error("device:upload requires --file and --target (or --verify)");
    }
    args.push("--file", file, "--target", target);
  }
  runNodeCli(args[0], args.slice(1), "device:upload");
  ReportProgress(1, "device-upload");
}
_Tag(deviceUpload, {
  gulpName: "device:upload",
  µDisplayName: 'Device Serial Upload<context="µDisplayName"/>',
  µDescription:
    'Upload a local file to /esp2/... via framed serial protocol (dev only).<context="µDescription"/>',
  µGroup: 'Device Storage<context="µGroup"/>',
  µOrder: 70,
  µParameters: [
    { name: "port", type: "string", optional: false },
    { name: "file", type: "string", optional: true },
    { name: "target", type: "string", optional: true },
    { name: "verify", type: "string", optional: true },
  ],
});

export async function deviceUsbStorage() {
  ReportProgress(0, "device-usb-storage");
  const port = GetParameter("port") || process.env.ESP2_PORT || "";
  const leave = GetParameter("leave") === true || GetParameter("leave") === "true";
  if (!port) {
    throw new Error("device:usb-storage requires --port COMx");
  }
  const args = leave
    ? ["--port", port, "--leave-usb-storage"]
    : ["--port", port, "--enter-usb-storage"];
  runNodeCli("dev/tools/esp2-upload.mjs", args, "device:usb-storage");
  ReportProgress(1, "device-usb-storage");
}
_Tag(deviceUsbStorage, {
  gulpName: "device:usb-storage",
  µDisplayName: 'Device USB Storage Mode<context="µDisplayName"/>',
  µDescription:
    'Enter or leave USB MSC ownership of the microSD (TinyUSB OTG).<context="µDescription"/>',
  µGroup: 'Device Storage<context="µGroup"/>',
  µOrder: 71,
  µParameters: [
    { name: "port", type: "string", optional: false },
    { name: "leave", type: "boolean", optional: true },
  ],
});

gulp.task("device:upload", deviceUpload);
gulp.task("device:usb-storage", deviceUsbStorage);
