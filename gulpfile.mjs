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

import { dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { existsSync, mkdirSync } from "node:fs";
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
import { ComposeReadme, README_SOURCE_RELATIVE } from "./dev/tools/readme-compose.mjs";
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
  Log('Composing README.md from <path/><context="task log"/>', {
    path: README_SOURCE_RELATIVE,
  });
  const result = ComposeReadme({ root: rootDir });
  if (result.changed) {
    Log('Wrote <path/> (<bytes format="int"/> bytes)<context="task log"/>', {
      path: "README.md",
      bytes: result.bytes,
    });
  } else {
    Log(
      'README.md already up to date (<bytes format="int"/> bytes).<context="task log"/>',
      { bytes: result.bytes }
    );
  }
  ReportProgress(1, "docs");
  PlaySignal("success");
}
_Tag(docs, {
  gulpName: "docs",
  µDisplayName: 'Compose README<context="µDisplayName"/>',
  µDescription:
    'Generates README.md from dev/docs/readme/de-DE.src.md. Does not overwrite the source.<context="µDescription"/>',
  µGroup: 'Docs & Backup<context="µGroup"/>',
  µIcon: "\uE915",
  µOrder: 10,
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
// npm script backup:nas → gulp backup (no second dashboard entry)
