//================================================================
// ESP][ — µGulp / Gulp task runner
// © 2026 Meinolf Amekudzi
//
// Dashboard groups (all start collapsed):
//   Firmware / Tests / Tools / Docs / Git / Backup
//   Media / Catalog / Apple II/* / Device
//
// CLI-only (registered, not dashboard-exported): backup:git, backup:nas,
//   releases:context-check, releases:context-fix, releases:i18x-update
// Default = docs (safe).
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
  RevealTask,
  SetTaskEmphasis,
  SetGroupState,
  NotifyTasksChanged,
  GetLid,
  LogAccordion,
} from "gulp-mu-gulp-api";
import {
  ComposeReadme,
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
  SaveNasLocalDefaults,
  VerifyBackupContents,
} from "./dev/tools/nas-backup.mjs";
import { RunGitBackup } from "./dev/tools/git-backup.mjs";
import {
  AskPort,
  ListPorts,
  MonitorArgs,
  Pio,
  PIO_ENV,
  PIO_ENVS,
  ResolvePioEnv,
  UploadArgs,
} from "./dev/tools/pio.mjs";
import { syncApple2js } from "./dev/tools/apple2js/sync.mjs";
import {
  bindApple2ReadinessAttention,
  computeApple2MediaReadiness,
} from "./dev/tools/apple2-local/mugulp-readiness.mjs";
import { GetProjectVersionLabel } from "./dev/tools/project-version.mjs";
import { ReleasesPaths } from "./dev/tools/releases/paths.mjs";
import {
  CountPendingContributorMerges,
  MergeDeveloperReleases,
} from "./dev/tools/releases/release-merge.mjs";
import {
  BuildReleaseHistoryAccordion,
  FormatReleaseHistoryText,
} from "./dev/tools/releases/release-history.mjs";
import {
  CheckReleaseContexts,
  FixReleaseContexts,
} from "./dev/tools/releases/release-context-audit.mjs";
import {
  CheckReleaseI18xCompleteness,
  UpdateReleaseI18xSources,
} from "./dev/tools/releases/release-i18x.mjs";
import {
  applyConfigToDevice,
  configFromFormValues,
  formDefaultsFromConfig,
  formDefaultsFromPreset,
  listProfileIds,
  loadProfileSystemJson,
  needsFirstRunSetup,
  normalizeSystemConfig,
  saveConfigLocal,
} from "./dev/tools/device-config-form.mjs";

InstallStringExtensions();

const rootDirEarly = dirname(fileURLToPath(import.meta.url));
export const µI18xContext = {
  project: "esp2",
  product: "ESP][",
  version: GetProjectVersionLabel(rootDirEarly),
};

/** Dashboard start layout for µGroup sections (nested Apple II groups). */
export const µGroups = {
  collapsed: true,
  groups: {
    'Firmware<context="µGroup"/>': "collapsed",
    'Tests<context="µGroup"/>': "collapsed",
    'Tools<context="µGroup"/>': "collapsed",
    'Docs<context="µGroup"/>': "collapsed",
    'Git<context="µGroup"/>': "collapsed",
    'Backup<context="µGroup"/>': "collapsed",
    'Media / Catalog<context="µGroup"/>': "collapsed",
    'Apple II/Media<context="µGroup"/>': "collapsed",
    'Apple II/Emulator<context="µGroup"/>': "collapsed",
    'Apple II/Compatibility<context="µGroup"/>': "collapsed",
    'Apple II/Device<context="µGroup"/>': "collapsed",
    'Device<context="µGroup"/>': "collapsed",
  },
};

const rootDir = dirname(fileURLToPath(import.meta.url));

/**
 * @param {Function} _task
 * @param {object} _meta
 */
function _Tag(_task, _meta) {
  if (_meta.gulpName) _task.displayName = _meta.gulpName;
  if (typeof _meta.µDisplayName === "function") {
    _task.µDisplayName = _meta.µDisplayName;
  } else if (_meta.µDisplayName) {
    _task.µDisplayName = _meta.µDisplayName.i18xRegister();
  }
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
  if (_meta.µAttention != null) _task.µAttention = _meta.µAttention;
  if (_meta.µAttentionTooltip) {
    _task.µAttentionTooltip = _meta.µAttentionTooltip.i18xRegister();
  }
  if (_meta.µAttentionWatch) _task.µAttentionWatch = _meta.µAttentionWatch;
  return _task;
}

function _ReleaseMergeOptions() {
  const paths = ReleasesPaths(rootDir);
  return {
    developerDir: paths.developerDir,
    releasesPath: paths.releasesPath,
    maxAgeDays: 30,
  };
}

function _PendingReleaseMerges() {
  return CountPendingContributorMerges(_ReleaseMergeOptions());
}

/** Keep i18x keys stable — never bake numbers into the registered phrase. */
function _ReleasesUpdateDisplayName() {
  if (_PendingReleaseMerges() <= 0) {
    return 'Update Release History (up to date) V<version/><context="µDisplayName"/>'.i18xRegister();
  }
  return 'Update Release History — pending V<version/><context="µDisplayName"/>'.i18xRegister();
}

function _ReleasesHistoryDisplayName() {
  return 'View Release History V<version/><context="µDisplayName"/>'.i18xRegister();
}

function _RunReleaseMaintenancePipeline() {
  const fixed = FixReleaseContexts({ root: rootDir, write: true });
  const checked = CheckReleaseContexts({ root: rootDir });
  if (!checked.ok) {
    throw new Error(
      `RELEASES context validation failed (${checked.issues.length} issue(s))`,
    );
  }
  const extracted = UpdateReleaseI18xSources({ root: rootDir, write: true });
  return { fixed, checked, extracted };
}

//================================================================
// Firmware
//================================================================

function firmwareEnv() {
  try {
    const p = GetParameter("env");
    if (p) return ResolvePioEnv(String(p));
  } catch {
    /* parameter API may be unavailable outside µGulp */
  }
  return ResolvePioEnv();
}


export async function build() {
  ReportProgress(0, "build");
  Log('Building ESP][ firmware (env <env/>)…<context="task log"/>', {
    env: firmwareEnv(),
  });
  await Pio(rootDir, ["run", "-e", firmwareEnv()], "pio build");
  ReportProgress(1, "build");
  Log('Build OK.<context="task log"/>');
  PlaySignal("success");
}
_Tag(build, {
  gulpName: "build",
  µDisplayName: 'Build Firmware V<version/><context="µDisplayName"/>',
  µDescription:
    'Compiles ESP][ with PlatformIO. Default env from platformio.ini / ESP2_PIO_ENV (bringup|core_smoke|apple2_text).<context="µDescription"/>',
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
  await Pio(rootDir, ["run", "-e", firmwareEnv()], "pio build");
  ReportProgress(0.55, "flash-upload");
  await Pio(rootDir, UploadArgs(port, firmwareEnv()), "pio upload");
  ReportProgress(1, "flash-upload");
  Log('Flash finished.<context="task log"/>');
  PlaySignal("success");
}
_Tag(flash, {
  gulpName: "flash",
  µDisplayName: 'Build & Upload Firmware V<version/><context="µDisplayName"/>',
  µDescription:
    'Rebuilds firmware and flashes it to the ESP][ (asks for COM port). Overwrites device firmware.<context="µDescription"/>',
  µTooltip:
    'Set ESP2_PORT=COMx to skip the port dialog.<context="µTooltip"/>',
  µIcon: "\u26A1",
  µGroup: 'Firmware<context="µGroup"/>',
  µOrder: 40,
  µExecutionConcurrency: false,
  µExecutionRestrictions: { deny: ["upload", "build", "backup"] },
});

export async function upload() {
  ReportProgress(0, "upload");
  const port = await AskPort(rootDir, "Upload port");
  Log('Uploading → <port/><context="task log"/>…', {
    port: port ?? "auto/ini",
  });
  await Pio(rootDir, UploadArgs(port, firmwareEnv()), "pio upload");
  ReportProgress(1, "upload");
  Log('Upload finished.<context="task log"/>');
  PlaySignal("success");
}
_Tag(upload, {
  gulpName: "upload",
  µDisplayName: 'Upload Firmware V<version/><context="µDisplayName"/>',
  µDescription:
    'Flashes the last build. Asks for the COM port.<context="µDescription"/>',
  µTooltip:
    'Set ESP2_PORT=COMx to skip the port dialog.<context="µTooltip"/>',
  µIcon: "\u2191",
  µGroup: 'Firmware<context="µGroup"/>',
  µOrder: 30,
  µExecutionConcurrency: false,
  µExecutionRestrictions: { deny: ["flash", "monitor", "backup"] },
});

export async function size() {
  ReportProgress(0, "size");
  await Pio(rootDir, ["run", "-e", firmwareEnv(), "-t", "size"], "pio size");
  ReportProgress(1, "size");
}
_Tag(size, {
  gulpName: "size",
  µDisplayName: 'Firmware Memory Usage V<version/><context="µDisplayName"/>',
  µDescription:
    'Shows RAM / Flash usage of the last firmware build.<context="µDescription"/>',
  µIcon: "\u25A6",
  µGroup: 'Firmware<context="µGroup"/>',
  µOrder: 50,
  µExecutionConcurrency: true,
});

export async function clean() {
  ReportProgress(0, "clean");
  await Pio(rootDir, ["run", "-e", firmwareEnv(), "-t", "clean"], "pio clean");
  ReportProgress(1, "clean");
  Log('Build artefacts removed.<context="task log"/>');
}
_Tag(clean, {
  gulpName: "clean",
  µDisplayName: 'Clean Firmware Build Files V<version/><context="µDisplayName"/>',
  µDescription:
    'Removes .pio/build artefacts for the active PlatformIO env (ESP2_PIO_ENV / default bringup).<context="µDescription"/>',
  µIcon: "\u239A",
  µGroup: 'Firmware<context="µGroup"/>',
  µOrder: 60,
  µExecutionConcurrency: true,
});

export const rebuild = gulp.series(clean, build);
_Tag(rebuild, {
  gulpName: "rebuild",
  µDisplayName: 'Rebuild Firmware V<version/><context="µDisplayName"/>',
  µDescription: 'clean → build for the active PlatformIO env.<context="µDescription"/>',
  µIcon: "\u21BB",
  µGroup: 'Firmware<context="µGroup"/>',
  µOrder: 20,
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
  µDisplayName: 'List Serial Ports V<version/><context="µDisplayName"/>',
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
  µDisplayName: 'Serial Monitor V<version/><context="µDisplayName"/>',
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

function _NasFormFromParameters() {
  return {
    destination1: GetParameter("destination1", ""),
    destination2: GetParameter("destination2", ""),
    destination3: GetParameter("destination3", ""),
    dryRun: GetParameter("dryRun", false) === true,
  };
}

/**
 * Resolve NAS targets for backup / list / verify.
 * Form slots win only when at least one path is set; otherwise
 * config/nas.targets.local (and env) apply — so list/verify work after a
 * form-driven backup that persisted destinations.
 */
async function _ResolveNasForRun() {
  if (
    process.env.NAS_TARGET_1 ||
    process.env.NAS_TARGET_2 ||
    process.env.NAS_TARGET_3 ||
    process.env.ESP2_NAS_BACKUP_PATHS
  ) {
    return ResolveNasTargets(rootDir, null);
  }

  if (process.env.MICROGULP_PARAMS || IsMicroGulp()) {
    return ResolveNasTargets(rootDir, _NasFormFromParameters());
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

  // Remember form destinations for backup:list / backup:verify (gitignored).
  if (resolved.source === "form") {
    try {
      const savedPath = SaveNasLocalDefaults(rootDir, {
        destination1: resolved.destinations[0] || "",
        destination2: resolved.destinations[1] || "",
        destination3: resolved.destinations[2] || "",
        dryRun: resolved.dryRun,
      });
      Log(
        'Remembered NAS destinations → <path/> (gitignored).<context="task log"/>',
        { path: savedPath },
      );
    } catch (err) {
      Warn(
        'Could not write config/nas.targets.local: <message/><context="task warning"/>',
        { message: err.message },
      );
    }
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
  Log(
    'Composing bilingual README.md from <path/> + de-DE.src.md (microCSS style)<context="task log"/>',
    { path: README_SOURCE_RELATIVE },
  );
  const result = ComposeReadme({ root: rootDir });
  for (const r of result.results) {
    const name = README_SOURCES[r.locale].baselineRel;
    if (r.changed) {
      Log('Wrote <path/> (<bytes format="int"/> bytes)<context="task log"/>', {
        path: name,
        bytes: r.bytes,
      });
    } else {
      Log(
        '<path/> already up to date (<bytes format="int"/> bytes).<context="task log"/>',
        { path: name, bytes: r.bytes },
      );
    }
  }
  Log(
    result.changed
      ? 'Wrote README.md (bilingual, <bytes format="int"/> bytes)<context="task log"/>'
      : 'README.md already up to date (<bytes format="int"/> bytes).<context="task log"/>',
    { bytes: result.bytes },
  );
  ReportProgress(1, "docs");
  PlaySignal("success");
}
_Tag(docs, {
  gulpName: "docs",
  µDisplayName: 'Compose READMEs V<version/><context="µDisplayName"/>',
  µDescription:
    'Generates one bilingual README.md (English then German, #deutsch) plus locale baselines from .src.md sources. Does not overwrite sources.<context="µDescription"/>',
  µGroup: 'Docs<context="µGroup"/>',
  µIcon: "\uE915",
  µOrder: 10,
  µExecutionConcurrency: false,
});

export async function docsEnUS() {
  ReportProgress(0, "docs-en-US");
  const result = ComposeReadme({ root: rootDir });
  Log(
    result.changed
      ? 'Wrote bilingual README.md (<bytes format="int"/> bytes)<context="task log"/>'
      : 'README.md already up to date (<bytes format="int"/> bytes).<context="task log"/>',
    { bytes: result.bytes },
  );
  ReportProgress(1, "docs-en-US");
  PlaySignal("success");
}
_Tag(docsEnUS, {
  gulpName: "docs:en-US",
  µDisplayName: 'Compose README (en-US) V<version/><context="µDisplayName"/>',
  µDescription:
    'Refreshes locale baselines and the bilingual README.md (same as docs).<context="µDescription"/>',
  µGroup: 'Docs<context="µGroup"/>',
  µIcon: "\uE915",
  µOrder: 11,
  µExecutionConcurrency: false,
});

export async function docsDeDE() {
  ReportProgress(0, "docs-de-DE");
  const result = ComposeReadme({ root: rootDir });
  Log(
    result.changed
      ? 'Wrote bilingual README.md (<bytes format="int"/> bytes)<context="task log"/>'
      : 'README.md already up to date (<bytes format="int"/> bytes).<context="task log"/>',
    { bytes: result.bytes },
  );
  ReportProgress(1, "docs-de-DE");
  PlaySignal("success");
}
_Tag(docsDeDE, {
  gulpName: "docs:de-DE",
  µDisplayName: 'Compose README (de-DE) V<version/><context="µDisplayName"/>',
  µDescription:
    'Refreshes locale baselines and the bilingual README.md (same as docs).<context="µDescription"/>',
  µGroup: 'Docs<context="µGroup"/>',
  µIcon: "\uE915",
  µOrder: 12,
  µExecutionConcurrency: false,
});

//================================================================
// RELEASES — Docs group (en-US history; no auto-translate on merge)
//================================================================

export async function RELEASES_UPDATE() {
  ReportProgress(0, "releases-update");
  Log(
    'Merging contributor notes (dev/releases/*.json) into RELEASES.json…<context="task log"/>'
  );
  const summary = MergeDeveloperReleases({
    ..._ReleaseMergeOptions(),
    write: true,
  });
  Log(
    'scanned=<scanned format="int"/> new=<merged format="int"/> duplicates=<duplicates format="int"/> expired=<expired format="int"/> invalid=<invalid format="int"/><context="task log"/>',
    {
      scanned: summary.scanned,
      merged: summary.merged,
      duplicates: summary.duplicates,
      expired: summary.expired,
      invalid: summary.invalid,
    }
  );
  ReportProgress(0.55, "releases-maintain");
  const maintenance = _RunReleaseMaintenancePipeline();
  Log(
    'Release maintenance: contextFix=<fixed/> i18xBodies=<bodies format="int"/> missingDe=<missing format="int"/><context="task log"/>',
    {
      fixed: maintenance.fixed.changed ? "yes" : "no",
      bodies: maintenance.extracted.bodies,
      missing: maintenance.extracted.missingDe.length,
    }
  );
  for (const body of maintenance.extracted.missingDe.slice(0, 20)) {
    Warn('missing de-DE release translation: <text/><context="task warning"/>', {
      text: body.slice(0, 80),
    });
  }
  µI18xContext.version = GetProjectVersionLabel(rootDir);
  SetTaskEmphasis("RELEASES_UPDATE", null);
  NotifyTasksChanged();
  ReportProgress(1, "releases-update");
  PlaySignal("success");
  return { summary, maintenance };
}
_Tag(RELEASES_UPDATE, {
  gulpName: "releases:update",
  µDisplayName: () => _ReleasesUpdateDisplayName(),
  µDescription:
    'Merges contributor notes into RELEASES.json, normalizes release-info context tags, validates, and updates release i18x sources. Does not AI-translate.<context="µDescription"/>',
  µTooltip:
    'ACTION_AVAILABLE when unmerged contributor notes exist. Safe to re-run (idempotent).<context="µTooltip"/>',
  µGroup: 'Docs<context="µGroup"/>',
  µIcon: "\uE915",
  µOrder: 20,
  µExecutionConcurrency: false,
  µAttention: () => _PendingReleaseMerges() > 0,
  µAttentionTooltip:
    'Unmerged contributor release notes available.<context="µAttentionTooltip"/>',
  µAttentionWatch: {
    files: ["RELEASES.json", "dev/releases"],
    intervalMs: 10_000,
  },
});

export async function RELEASES_HISTORY() {
  ReportProgress(0, "releases-history");
  const paths = ReleasesPaths(rootDir);
  let lid = "en-US";
  try {
    const active = typeof GetLid === "function" ? GetLid() : null;
    if (active === "de-DE" || active === "en-US") lid = active;
  } catch {
    /* CLI */
  }
  const accordion = BuildReleaseHistoryAccordion({
    releasesPath: paths.releasesPath,
    root: rootDir,
    lid,
    maxReleases: 12,
  });
  Log(
    'Release history: <count format="int"/> version block(s), <entries format="int"/> info line(s).<context="task log"/>',
    { count: accordion.items.length, entries: accordion.entryCount }
  );
  if (typeof LogAccordion === "function") {
    LogAccordion(accordion);
  } else {
    process.stdout.write(FormatReleaseHistoryText(accordion));
  }
  ReportProgress(1, "releases-history");
  PlaySignal("success");
  return accordion;
}
_Tag(RELEASES_HISTORY, {
  gulpName: "releases:history",
  µDisplayName: () => _ReleasesHistoryDisplayName(),
  µDescription:
    'Read-only: shows localized ESP][ release history from RELEASES.json (date, version, info lines). Does not dump raw JSON.<context="µDescription"/>',
  µTooltip:
    'Translations: i18x/gulp/releases/{en-US,de-DE}.json.<context="µTooltip"/>',
  µGroup: 'Docs<context="µGroup"/>',
  µIcon: "\uE914",
  µOrder: 21,
  µExecutionConcurrency: true,
});

/** CLI-only — dashboard uses releases:update pipeline. */
async function RELEASES_CONTEXT_CHECK() {
  ReportProgress(0, "releases-context-check");
  const result = CheckReleaseContexts({ root: rootDir });
  Log(
    'Context CHECK scanned=<scanned format="int"/> issues=<issues format="int"/><context="task log"/>',
    { scanned: result.scanned, issues: result.issues.length }
  );
  for (const issue of result.issues.slice(0, 40)) {
    Warn(
      'context <kind/> at <path/><context="task warning"/>',
      { kind: issue.kind, path: issue.path }
    );
  }
  ReportProgress(1, "releases-context-check");
  if (!result.ok) {
    PlaySignal("error");
    throw new Error(`RELEASES context CHECK failed (${result.issues.length} issue(s))`);
  }
  PlaySignal("success");
  return result;
}
/** CLI-only — dashboard uses releases:update pipeline. */
async function RELEASES_CONTEXT_FIX() {
  ReportProgress(0, "releases-context-fix");
  const result = FixReleaseContexts({ root: rootDir, write: true });
  Log(
    'Context FIX changed=<changed/> issuesSeen=<n format="int"/><context="task log"/>',
    { changed: result.changed ? "yes" : "no", n: result.issuesFixed }
  );
  const recheck = CheckReleaseContexts({ root: rootDir });
  ReportProgress(1, "releases-context-fix");
  if (!recheck.ok) {
    PlaySignal("error");
    throw new Error(`RELEASES context FIX left ${recheck.issues.length} issue(s)`);
  }
  PlaySignal("success");
  return result;
}
/** CLI-only — also invoked from releases:update / i18x:gulp. */
async function RELEASES_I18X_UPDATE() {
  ReportProgress(0, "releases-i18x");
  const extracted = UpdateReleaseI18xSources({ root: rootDir, write: true });
  Log(
    'Release i18x: bodies=<bodies format="int"/> missingDe=<missing format="int"/> staleDropped=<stale format="int"/><context="task log"/>',
    {
      bodies: extracted.bodies,
      missing: extracted.missingDe.length,
      stale: extracted.staleDe.length,
    }
  );
  for (const body of extracted.missingDe.slice(0, 20)) {
    Warn('missing de-DE release translation: <text/><context="task warning"/>', {
      text: body.slice(0, 80),
    });
  }
  const complete = CheckReleaseI18xCompleteness(rootDir);
  ReportProgress(1, "releases-i18x");
  PlaySignal(complete.complete ? "success" : "warning");
  return { extracted, complete };
}
/** CLI-only commit(+push). Dashboard: git:commit / git:push. */
async function BACKUP_GIT() {
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
export async function backup() {
  return _RunNasBackup();
}
_Tag(backup, {
  gulpName: "backup",
  µDisplayName: 'Backup to NAS V<version/><context="µDisplayName"/>',
  µDescription:
    'Copies project trees plus local/downloaded assets (local/apple2, local/roms, _refs, 3dprint) to NAS folders. Skips node_modules/.pio caches. May overwrite destination files.<context="µDescription"/>',
  µTooltip:
    'Form: Destination 1–3 + dry-run. Or NAS_TARGET_1..3 / config/nas.targets.local.<context="µTooltip"/>',
  µGroup: 'Backup<context="µGroup"/>',
  µIcon: "\uE902",
  µOrder: 10,
  µExecutionConcurrency: false,
  µParameters: _NasBackupParameters(),
});

export async function BACKUP_ALL() {
  ReportProgress(0, "backup-all");
  await gitCommit();
  ReportProgress(0.5, "backup-all");
  await backup();
  ReportProgress(1, "backup-all");
  Log('[ALL] Git commit + NAS backup finished.<context="task log"/>');
}
_Tag(BACKUP_ALL, {
  gulpName: "backup:all",
  µDisplayName: 'Save Project (Git + NAS) V<version/><context="µDisplayName"/>',
  µDescription:
    'Creates a local Git commit (project rules), then runs the NAS backup form. Does not regenerate READMEs. Never force-pushes.<context="µDescription"/>',
  µGroup: 'Backup<context="µGroup"/>',
  µIcon: "\uE902",
  µOrder: 40,
  µExecutionConcurrency: false,
});

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

function runNodeTest(relScript, label) {
  const script = join(rootDir, relScript);
  const r = spawnSync(process.execPath, ["--test", script], {
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

//================================================================
// Discovery
//================================================================

export default docs;

gulp.task("backup:git", BACKUP_GIT);
gulp.task("backup:all", BACKUP_ALL);
gulp.task("docs:en-US", docsEnUS);
gulp.task("docs:de-DE", docsDeDE);
gulp.task("releases:update", RELEASES_UPDATE);
gulp.task("releases:history", RELEASES_HISTORY);
gulp.task("releases:context-check", RELEASES_CONTEXT_CHECK);
gulp.task("releases:context-fix", RELEASES_CONTEXT_FIX);
gulp.task("releases:i18x-update", RELEASES_I18X_UPDATE);
gulp.task("backup:nas", backup);


//===== CATALOG_RECOVERY_START =====
//================================================================
// Catalog recovery — Git / Backup / Firmware env / Tests / Format
//================================================================

export async function firmwareEnvTask() {
  ReportProgress(0, "firmware-env");
  const env = firmwareEnv();
  Log('Active PlatformIO env: <env/><context="task log"/>', { env });
  Log('Available envs: <list/><context="task log"/>', {
    list: PIO_ENVS.join(", "),
  });
  Log('Override via ESP2_PIO_ENV or µGulp parameter env.<context="task log"/>');
  ReportProgress(1, "firmware-env");
  PlaySignal("success");
  return { env, available: [...PIO_ENVS] };
}
_Tag(firmwareEnvTask, {
  gulpName: "firmware:env",
  µDisplayName: 'Show Active Firmware Env V<version/><context="µDisplayName"/>',
  µDescription:
    'Read-only: prints the active PlatformIO environment and known envs from platformio.ini.<context="µDescription"/>',
  µGroup: 'Tools<context="µGroup"/>',
  µIcon: "\u2398",
  µOrder: 5,
  µExecutionConcurrency: true,
});

export async function gitStatus() {
  ReportProgress(0, "git-status");
  const result = await RunGitBackup(rootDir, {
    dryRun: true,
    push: false,
    log: (m) => Log(m + '<context="task log"/>'),
    warn: (m) => Warn(m + '<context="task warning"/>'),
  });
  ReportProgress(1, "git-status");
  if (result.ok || result.reason === "clean" || result.dryRun) PlaySignal("success");
  else PlaySignal("warning");
  return result;
}
_Tag(gitStatus, {
  gulpName: "git:status",
  µDisplayName: 'Git Status V<version/><context="µDisplayName"/>',
  µDescription:
    'Read-only: shows Git status and paths that would be committed. Never commits or pushes. Respects GIT_BACKUP_NEVER_STAGE / local media exclusions.<context="µDescription"/>',
  µGroup: 'Git<context="µGroup"/>',
  µIcon: "\u2398",
  µOrder: 10,
  µExecutionConcurrency: true,
});

export async function gitCommit() {
  ReportProgress(0, "git-commit");
  const result = await RunGitBackup(rootDir, {
    dryRun: false,
    push: false,
    log: (m) => Log(m + '<context="task log"/>'),
    warn: (m) => Warn(m + '<context="task warning"/>'),
  });
  ReportProgress(1, "git-commit");
  if (result.ok) PlaySignal("success");
  else PlaySignal("warning");
  return result;
}
_Tag(gitCommit, {
  gulpName: "git:commit",
  µDisplayName: 'Git Commit V<version/><context="µDisplayName"/>',
  µDescription:
    'Creates a local Git commit (project include/exclude rules). Does not push. Never stages local/apple2, proprietary media, or secrets.<context="µDescription"/>',
  µGroup: 'Git<context="µGroup"/>',
  µIcon: "\uE902",
  µOrder: 20,
  µExecutionConcurrency: false,
});

export async function gitPush() {
  ReportProgress(0, "git-push");
  const r = spawnSync("git", ["push"], {
    cwd: rootDir,
    encoding: "utf8",
    shell: false,
  });
  if (r.stdout) process.stdout.write(r.stdout);
  if (r.stderr) process.stderr.write(r.stderr);
  if (r.status !== 0) {
    throw new Error(`git:push failed (exit ${r.status})`);
  }
  Log('git push OK.<context="task log"/>');
  ReportProgress(1, "git-push");
  PlaySignal("success");
}
_Tag(gitPush, {
  gulpName: "git:push",
  µDisplayName: 'Git Push V<version/><context="µDisplayName"/>',
  µDescription:
    'Pushes the current branch to its upstream remote. Does not stage or create a commit. Never force-pushes.<context="µDescription"/>',
  µGroup: 'Git<context="µGroup"/>',
  µIcon: "\u2191",
  µOrder: 30,
  µExecutionConcurrency: false,
});


export async function backupList() {
  ReportProgress(0, "backup-list");
  const resolved = await _ResolveNasForRun();
  if (!resolved.destinations.length) {
    Warn('No NAS destinations configured.<context="task log"/>');
    ReportProgress(1, "backup-list");
    return { destinations: [] };
  }
  Log('Configured NAS destination(s):<context="task log"/>');
  const rows = [];
  for (const dest of resolved.destinations) {
    if (!existsSync(dest)) {
      Warn('  missing: <path/><context="task log"/>', { path: dest });
      rows.push({ path: dest, ok: false, missing: ["(unreachable)"] });
      continue;
    }
    const check = VerifyBackupContents(dest);
    Log('  <path/> essential=<ok/><context="task log"/>', {
      path: dest,
      ok: check.ok ? "OK" : "missing:" + check.missing.join(","),
    });
    rows.push({ path: dest, ok: check.ok, missing: check.missing });
  }
  ReportProgress(1, "backup-list");
  PlaySignal("success");
  return { destinations: rows };
}
_Tag(backupList, {
  gulpName: "backup:list",
  µDisplayName: 'List NAS Backups V<version/><context="µDisplayName"/>',
  µDescription:
    'Shows configured NAS destinations and whether essential restore files are present. Read-only.<context="µDescription"/>',
  µGroup: 'Backup<context="µGroup"/>',
  µIcon: "\u2398",
  µOrder: 20,
  µExecutionConcurrency: true,
});

export async function backupVerify() {
  ReportProgress(0, "backup-verify");
  const resolved = await _ResolveNasForRun();
  const primary = resolved.destinations[0];
  if (!primary) {
    throw new Error("backup:verify — no NAS destination configured");
  }
  AssertNasBackupTarget(primary, rootDir);
  if (!existsSync(primary)) {
    throw new Error("backup:verify — destination not reachable: " + primary);
  }
  const check = VerifyBackupContents(primary);
  Log('Backup target: <path/><context="task log"/>', { path: primary });
  if (!check.ok) {
    throw new Error("backup:verify failed — missing: " + check.missing.join(", "));
  }
  Log('backup:verify OK — essential files present.<context="task log"/>');
  ReportProgress(1, "backup-verify");
  PlaySignal("success");
  return check;
}
_Tag(backupVerify, {
  gulpName: "backup:verify",
  µDisplayName: 'Verify NAS Backup V<version/><context="µDisplayName"/>',
  µDescription:
    'Checks primary NAS destination reachability and essential restore files. Read-only; never restores.<context="µDescription"/>',
  µGroup: 'Backup<context="µGroup"/>',
  µIcon: "\u2713",
  µOrder: 30,
  µExecutionConcurrency: true,
});

export async function testInfraTask() {
  ReportProgress(0, "test-infra");
  runNodeTest("dev/tools/infra.test.mjs", "test:infra");
  ReportProgress(1, "test-infra");
  PlaySignal("success");
}
_Tag(testInfraTask, {
  gulpName: "test:infra",
  µDisplayName: 'Run Infrastructure Tests V<version/><context="µDisplayName"/>',
  µDescription:
    'Runs node:test for NAS/Git/docs infrastructure helpers (no real NAS write / no Git commit).<context="µDescription"/>',
  µGroup: 'Tests<context="µGroup"/>',
  µIcon: "\u2713",
  µOrder: 10,
  µExecutionConcurrency: false,
});

export async function testMugulpCatalog() {
  ReportProgress(0, "test-mugulp");
  runNodeTest("dev/tools/mugulp-task-catalog.test.mjs", "test:mugulp-catalog");
  ReportProgress(1, "test-mugulp");
  PlaySignal("success");
}
_Tag(testMugulpCatalog, {
  gulpName: "test:mugulp-catalog",
  µDisplayName: 'Run µGulp Catalog Tests V<version/><context="µDisplayName"/>',
  µDescription:
    'Regression tests for first-party task catalog, metadata, and UTF-8 integrity.<context="µDescription"/>',
  µGroup: 'Tests<context="µGroup"/>',
  µIcon: "\u2713",
  µOrder: 20,
  µExecutionConcurrency: false,
});

export async function testHost() {
  ReportProgress(0, "test-host");
  runNodeCli("host/test/run.mjs", [], "test:host");
  ReportProgress(1, "test-host");
  PlaySignal("success");
}
_Tag(testHost, {
  gulpName: "test:host",
  µDisplayName: 'Run Host Apple II Tests V<version/><context="µDisplayName"/>',
  µDescription:
    'Runs host-side Apple II JS test suite (no proprietary media required).<context="µDescription"/>',
  µGroup: 'Tests<context="µGroup"/>',
  µIcon: "\u2713",
  µOrder: 30,
  µExecutionConcurrency: false,
});

export async function formatCheck() {
  ReportProgress(0, "format-check");
  const r = spawnSync("clang-format", ["--version"], {
    cwd: rootDir,
    encoding: "utf8",
    shell: true,
  });
  if (r.status !== 0) {
    Warn('clang-format not available — format:check skipped.<context="task warning"/>');
  } else {
    Log('clang-format available — project uses .clang-format for C/C++.<context="task log"/>');
  }
  ReportProgress(1, "format-check");
  PlaySignal("success");
}
_Tag(formatCheck, {
  gulpName: "format:check",
  µDisplayName: 'Format Check V<version/><context="µDisplayName"/>',
  µDescription:
    'Reports clang-format availability for project-owned C/C++ (.clang-format). Does not rewrite files.<context="µDescription"/>',
  µGroup: 'Tools<context="µGroup"/>',
  µIcon: "\u270E",
  µOrder: 30,
  µExecutionConcurrency: true,
});

export async function i18xGulp() {
  ReportProgress(0, "i18x-gulp");
  runNodeCli("dev/tools/build-i18x-gulp.mjs", [], "i18x:gulp");
  ReportProgress(0.6, "i18x-release");
  const extracted = UpdateReleaseI18xSources({ root: rootDir, write: true });
  Log(
    'Release i18x sources: bodies=<bodies format="int"/> missingDe=<missing format="int"/><context="task log"/>',
    {
      bodies: extracted.bodies,
      missing: extracted.missingDe.length,
    }
  );
  for (const body of extracted.missingDe.slice(0, 20)) {
    Warn('missing de-DE release translation: <text/><context="task warning"/>', {
      text: body.slice(0, 80),
    });
  }
  ReportProgress(1, "i18x-gulp");
  PlaySignal(extracted.missingDe.length ? "warning" : "success");
}
_Tag(i18xGulp, {
  gulpName: "i18x:gulp",
  µDisplayName: 'Rebuild i18x Dictionaries V<version/><context="µDisplayName"/>',
  µDescription:
    'Regenerates i18x/gulp dictionaries and extracts release en-US source strings. Reports missing de-DE — does not AI-translate.<context="µDescription"/>',
  µGroup: 'Tools<context="µGroup"/>',
  µIcon: "\uE915",
  µOrder: 40,
  µExecutionConcurrency: false,
});

gulp.task("firmware:env", firmwareEnvTask);
gulp.task("git:status", gitStatus);
gulp.task("git:commit", gitCommit);
gulp.task("git:push", gitPush);
gulp.task("backup:list", backupList);
gulp.task("backup:verify", backupVerify);
gulp.task("test:infra", testInfraTask);
gulp.task("test:mugulp-catalog", testMugulpCatalog);
gulp.task("test:host", testHost);
gulp.task("format:check", formatCheck);
gulp.task("i18x:gulp", i18xGulp);
//===== CATALOG_RECOVERY_END =====

//================================================================
// apple2js / media (developer; network only for :sync)
//================================================================

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
  µDisplayName: 'Sync Online Catalog V<version/><context="µDisplayName"/>',
  µDescription:
    'Clones/updates .cache/apple2js, fetches website index, re-audits with diff (network).<context="µDescription"/>',
  µIcon: "\u2601",
  µGroup: 'Media / Catalog<context="µGroup"/>',
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
  µDisplayName: 'Show Online Catalog V<version/><context="µDisplayName"/>',
  µDescription:
    'Summarize cached apple2js git+website catalog (offline if cache present).<context="µDescription"/>',
  µGroup: 'Media / Catalog<context="µGroup"/>',
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
  µDisplayName: 'Audit Online Catalog V<version/><context="µDisplayName"/>',
  µDescription:
    'Provenance audit + metadata stubs (offline if cache present).<context="µDescription"/>',
  µGroup: 'Media / Catalog<context="µGroup"/>',
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
  µDisplayName: 'Identify Media File V<version/><context="µDisplayName"/>',
  µDescription:
    'Offline SHA-256 + catalog match for a user disk image.<context="µDescription"/>',
  µGroup: 'Media / Catalog<context="µGroup"/>',
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
  µDisplayName: 'Import Local Media V<version/><context="µDisplayName"/>',
  µDescription:
    'Import user-supplied disk(s) into a library path (offline; never downloads).<context="µDescription"/>',
  µGroup: 'Media / Catalog<context="µGroup"/>',
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
  µDisplayName: 'Inspect Media (a2kit) V<version/><context="µDisplayName"/>',
  µDescription:
    'Optional external a2kit oracle if installed; otherwise ESP][ identify only.<context="µDescription"/>',
  µGroup: 'Media / Catalog<context="µGroup"/>',
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
  µDisplayName: 'Prepare SD Library Layout V<version/><context="µDisplayName"/>',
  µDescription:
    'Copy host library into target/apple2 (default dry-run; never formats drives).<context="µDescription"/>',
  µGroup: 'Media / Catalog<context="µGroup"/>',
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
  µDisplayName: 'Download Free Apple II ROMs V<version/><context="µDisplayName"/>',
  µDescription:
    'Download ONLY redistributable ROMs (e.g. AppleIIGo) into gitignored local/apple2/roms/.<context="µDescription"/>',
  µGroup: 'Apple II/Media<context="µGroup"/>',
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
  µDisplayName: 'Download Apple II Media V<version/><context="µDisplayName"/>',
  µDescription:
    'Fetch apple2js title(s) into gitignored local/apple2/cache/ (LOCAL_TEST_ONLY; never stages Git).<context="µDescription"/>',
  µGroup: 'Apple II/Media<context="µGroup"/>',
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
  µDisplayName: 'Prepare Runtime Disk V<version/><context="µDisplayName"/>',
  µDescription:
    'Normalize cached source to local/apple2/disks/ (offline; prefers DSK when lossless).<context="µDescription"/>',
  µGroup: 'Apple II/Media<context="µGroup"/>',
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
  µDisplayName: 'Show Local Media Status V<version/><context="µDisplayName"/>',
  µDescription:
    'Compact table of local titles: downloaded / prepared / provenance.<context="µDescription"/>',
  µGroup: 'Apple II/Media<context="µGroup"/>',
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
  µDisplayName: 'Show Local Media V<version/><context="µDisplayName"/>',
  µDescription: 'JSON list of locally cached titles.<context="µDescription"/>',
  µGroup: 'Apple II/Media<context="µGroup"/>',
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
  µDisplayName: 'Verify Media Provenance V<version/><context="µDisplayName"/>',
  µDescription:
    'Provenance counts for local cache (download never upgrades rights).<context="µDescription"/>',
  µGroup: 'Apple II/Media<context="µGroup"/>',
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
  µDisplayName: 'Clean Generated Disks V<version/><context="µDisplayName"/>',
  µDescription:
    'Default: remove generated disks/. Optional --cache / --user (never deletes user media by default).<context="µDescription"/>',
  µGroup: 'Apple II/Media<context="µGroup"/>',
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
  µDisplayName: 'Sync Game to ESP][ V<version/><context="µDisplayName"/>',
  µDescription:
    'Upload ONLY selected title runtime ROM+disk via serial ESPU (not full catalog).<context="µDescription"/>',
  µGroup: 'Apple II/Device<context="µGroup"/>',
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
  µDisplayName: 'Identify Apple II ROM V<version/><context="µDisplayName"/>',
  µDescription:
    'Hash/identify a user-supplied motherboard ROM (no download). SKIPPED_NO_ROM if absent.<context="µDescription"/>',
  µGroup: 'Apple II/Emulator<context="µGroup"/>',
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
  µDisplayName: 'Test Apple II ROM on Host V<version/><context="µDisplayName"/>',
  µDescription:
    'Optional bounded host run with user ROM; SKIPPED_NO_ROM without local ROM.<context="µDescription"/>',
  µGroup: 'Apple II/Emulator<context="µGroup"/>',
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
  µDisplayName: 'Run Apple II Host Emulator V<version/><context="µDisplayName"/>',
  µDescription:
    'Build and batch-run esp2_host (synthetic ROM unless --rom given).<context="µDescription"/>',
  µGroup: 'Apple II/Emulator<context="µGroup"/>',
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
  µDisplayName: 'Test Apple II Disk Boot on Host V<version/><context="µDisplayName"/>',
  µDescription:
    'Optional bounded Disk II boot/diagnostics; SKIPPED_NO_DISK without --disk. Does not download media.<context="µDescription"/>',
  µGroup: 'Apple II/Emulator<context="µGroup"/>',
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
  µDisplayName: 'Run Compatibility Test V<version/><context="µDisplayName"/>',
  µDescription:
    'Run real-software compatibility test definitions; SKIPPED_* without user assets. No downloads.<context="µDescription"/>',
  µGroup: 'Apple II/Compatibility<context="µGroup"/>',
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

//================================================================
// Apple II media readiness (µAttention / RevealTask)
//================================================================

bindApple2ReadinessAttention({
  "apple2:rom:sync": apple2RomSync,
  "apple2:media:sync": apple2MediaSync,
  "apple2:media:prepare": apple2MediaPrepare,
  "apple2:compat": apple2Compat,
  "apple2:device:sync": apple2DeviceSync,
});

/**
 * Fresh-clone / missing local media: one guided task downloads redistributable
 * ROMs + Galaxian media, prepares the runtime disk, and seeds local device config.
 */
function _FirstRunNeedsAttention() {
  try {
    const media = computeApple2MediaReadiness();
    if (
      media.step === "rom_sync" ||
      media.step === "media_sync" ||
      media.step === "media_prepare"
    ) {
      return true;
    }
    return needsFirstRunSetup(rootDir, media);
  } catch {
    return needsFirstRunSetup(rootDir);
  }
}

export async function setupFirstRun() {
  ReportProgress(0, "setup-first-run");
  const title =
    String(GetParameter("title") || "").trim() ||
    (
      (await RequestForm({
        title: 'ESP][ first-run setup<context="task parameter"/>'.i18xRegister(),
        submitLabel: 'Start setup<context="button text"/>'.i18xRegister(),
        fields: [
          {
            id: "title",
            type: "text",
            default: "galaxian",
            remember: false,
            label: 'Demo title id (apple2js)<context="task parameter"/>'.i18xRegister(),
            description:
              'Downloads redistributable ROMs and this LOCAL_TEST_ONLY title from the network, prepares a runtime disk, and seeds local/device/config from the matching profile when present.<context="task parameter"/>'.i18xRegister(),
          },
        ],
      })) || {}
    ).title ||
    "galaxian";

  Log(
    'First-run setup for title=<title/> (network download of redistributable assets)…<context="task log"/>',
    { title },
  );

  ReportProgress(0.1, "setup-rom");
  runNodeCli("dev/tools/apple2-local/cli.mjs", ["rom-sync"], "setup:first-run/rom");
  ReportProgress(0.35, "setup-media");
  runNodeCli(
    "dev/tools/apple2-local/cli.mjs",
    ["media-sync", "--title", title],
    "setup:first-run/media",
  );
  ReportProgress(0.55, "setup-prepare");
  runNodeCli(
    "dev/tools/apple2-local/cli.mjs",
    ["media-prepare", "--title", title],
    "setup:first-run/prepare",
  );

  ReportProgress(0.75, "setup-config");
  const profileId =
    listProfileIds(rootDir).includes(`${title}-demo`)
      ? `${title}-demo`
      : listProfileIds(rootDir).includes("galaxian-demo")
        ? "galaxian-demo"
        : null;
  if (profileId) {
    const raw = loadProfileSystemJson(rootDir, profileId);
    const normalized = normalizeSystemConfig(raw);
    if (!normalized.ok) {
      throw new Error(`preset ${profileId} invalid: ${normalized.errors.join("; ")}`);
    }
    const saved = saveConfigLocal(rootDir, normalized.config, profileId, profileId);
    Log(
      'Seeded local device config from profile <profile/> → <path/><context="task log"/>',
      { profile: profileId, path: saved.localSystem },
    );
  } else {
    Warn(
      'No matching device profile under config/device/profiles/ — skipped config seed.<context="task warning"/>',
    );
  }

  ReportProgress(1, "setup-first-run");
  Log(
    'First-run host setup finished. Next (optional): Build/Upload firmware, Configure ESP][ → Apply, Sync Game to ESP][.<context="task log"/>',
  );
  PlaySignal("success");
}
_Tag(setupFirstRun, {
  gulpName: "setup:first-run",
  µDisplayName: 'First-run setup (download + config) V<version/><context="µDisplayName"/>',
  µDescription:
    'For a fresh clone: download redistributable Apple II ROMs and a demo title from the network, prepare the runtime disk, and seed local device configuration. Does not flash firmware or upload to the ESP][.<context="µDescription"/>',
  µTooltip:
    'Highlighted after clone when local media/config is missing. Network required for downloads.<context="µTooltip"/>',
  µGroup: 'Tools<context="µGroup"/>',
  µIcon: "\u26A1",
  µOrder: 1,
  µExecutionConcurrency: false,
  µAttention: () => _FirstRunNeedsAttention(),
  µAttentionTooltip:
    'Fresh clone / missing local media — run first-run setup.<context="µAttentionTooltip"/>',
  µAttentionWatch: {
    files: ["local/apple2", "local/device/config", "config/device/profiles"],
    intervalMs: 15_000,
  },
  µParameters: [
    {
      id: "title",
      type: "text",
      default: "galaxian",
      remember: false,
      label: 'Demo title id (apple2js)<context="task parameter"/>'.i18xRegister(),
    },
  ],
});
gulp.task("setup:first-run", setupFirstRun);

if (IsMicroGulp()) {
  try {
    const readiness = computeApple2MediaReadiness();
    const firstRun = _FirstRunNeedsAttention();
    const emphasizeId = firstRun
      ? "setup:first-run"
      : readiness.emphasizeTaskId;
    if (emphasizeId) {
      if (firstRun) {
        SetGroupState("Tools", "open");
      } else {
        SetGroupState("Apple II/Media", "open");
      }
      if (readiness.step === "compat") {
        SetGroupState("Apple II/Compatibility", "open");
      }
      if (readiness.step === "device_sync") {
        SetGroupState("Apple II/Device", "open");
      }
      RevealTask(emphasizeId, {
        expand: true,
        scroll: true,
        highlight: "attention",
      });
      SetTaskEmphasis(emphasizeId, "attention");
    }
  } catch {
    /* readiness is advisory only */
  }
}

//================================================================
// Device media transfer (development)
//================================================================

function _DeviceConfigDisplayName() {
  try {
    const d = formDefaultsFromConfig(rootDir);
    if (d.drive1 && String(d.drive1).toLowerCase().includes("galaxian")) {
      return 'Configure ESP][ — Galaxian demo V<version/><context="µDisplayName"/>'.i18xRegister();
    }
  } catch {
    /* defaults */
  }
  return 'Configure ESP][ V<version/><context="µDisplayName"/>'.i18xRegister();
}

/**
 * Step 1 — pick a preset. µGulp cannot live-update other fields when a select
 * changes, so the edit form is rebuilt after this step with preset defaults.
 */
function _DeviceConfigPresetParameters() {
  const d = formDefaultsFromConfig(rootDir);
  const presets = [
    { value: "custom", label: 'Custom (blank / local fields)<context="task parameter"/>'.i18xRegister() },
    ...listProfileIds(rootDir).map((id) => ({ value: id, label: id })),
  ];
  return {
    title: 'ESP][ device configuration — choose profile<context="task parameter"/>'.i18xRegister(),
    submitLabel: 'Load profile into form<context="button text"/>'.i18xRegister(),
    fields: [
      {
        id: "preset",
        type: "select",
        default: d.preset,
        remember: false,
        label: 'Preset profile<context="task parameter"/>'.i18xRegister(),
        description:
          'Selecting a named preset (e.g. galaxian-demo) fills the next form from that profile. Nothing is uploaded yet.<context="task parameter"/>'.i18xRegister(),
        options: presets,
      },
    ],
  };
}

/**
 * Step 2 — editable values (defaults already taken from the chosen preset).
 * @param {ReturnType<typeof formDefaultsFromPreset>} d
 */
function _DeviceConfigEditForm(d) {
  const macros = (d.macros || ["none"]).map((id) =>
    id === "none"
      ? { value: "none", label: 'none<context="task parameter"/>'.i18xRegister() }
      : { value: id, label: id },
  );
  return {
    title: 'ESP][ device configuration — edit values<context="task parameter"/>'.i18xRegister(),
    submitLabel: 'Continue<context="button text"/>'.i18xRegister(),
    fields: [
      {
        id: "profileName",
        type: "text",
        default: d.profileName,
        remember: false,
        label: 'Profile name (local save)<context="task parameter"/>'.i18xRegister(),
        description:
          'Saved under local/device/config/ and optionally config/device/profiles/<name>/.<context="task parameter"/>'.i18xRegister(),
      },
      {
        id: "rom",
        type: "text",
        default: d.rom,
        required: true,
        remember: false,
        label: 'System ROM (device path)<context="task parameter"/>'.i18xRegister(),
        description:
          'Absolute path on the ESP][ SD, e.g. /esp2/roms/system.rom — not a host file upload.<context="task parameter"/>'.i18xRegister(),
      },
      {
        id: "drive1",
        type: "text",
        default: d.drive1,
        remember: false,
        label: 'Drive 1 image (device path)<context="task parameter"/>'.i18xRegister(),
        description:
          'e.g. /esp2/disks/Galaxian.dsk — path only; disk bytes are not uploaded by this form.<context="task parameter"/>'.i18xRegister(),
      },
      {
        id: "drive2",
        type: "text",
        default: d.drive2,
        remember: false,
        label: 'Drive 2 image (optional)<context="task parameter"/>'.i18xRegister(),
        description:
          'Leave empty when unused. Path under /esp2/ only.<context="task parameter"/>'.i18xRegister(),
      },
      {
        id: "bootFromDisk",
        type: "boolean",
        default: d.bootFromDisk,
        remember: false,
        label: 'Boot from disk (Autostart)<context="task parameter"/>'.i18xRegister(),
      },
      {
        id: "startupMacro",
        type: "select",
        default: d.startupMacro,
        remember: false,
        label: 'Startup macro<context="task parameter"/>'.i18xRegister(),
        options: macros,
      },
      {
        id: "orientation",
        type: "select",
        default: d.orientation,
        remember: false,
        label: 'Screen orientation<context="task parameter"/>'.i18xRegister(),
        options: [
          { value: "classic", label: 'Classic<context="task parameter"/>'.i18xRegister() },
          { value: "landscape", label: 'Landscape<context="task parameter"/>'.i18xRegister() },
        ],
      },
      {
        id: "monitor",
        type: "select",
        default: d.monitor ?? d.color ?? "white",
        remember: false,
        label: 'Monitor appearance<context="task parameter"/>'.i18xRegister(),
        options: [
          { value: "white", label: 'White / Monochrome<context="task parameter"/>'.i18xRegister() },
          { value: "green", label: 'Green phosphor<context="task parameter"/>'.i18xRegister() },
          { value: "amber", label: 'Amber phosphor<context="task parameter"/>'.i18xRegister() },
          {
            value: "artifact",
            label: 'Artifact Color<context="task parameter"/>'.i18xRegister(),
          },
        ],
      },
      {
        id: "effect",
        type: "select",
        default: d.effect ?? "clean",
        remember: false,
        label: 'Display effect<context="task parameter"/>'.i18xRegister(),
        options: [
          { value: "clean", label: 'Clean<context="task parameter"/>'.i18xRegister() },
          { value: "crt", label: 'CRT / TV<context="task parameter"/>'.i18xRegister() },
        ],
      },
      {
        id: "screensaverSeconds",
        type: "number",
        default: d.screensaverSeconds,
        min: 0,
        max: 86400,
        step: 1,
        remember: false,
        label: 'Screensaver timeout (seconds, 0 = disabled)<context="task parameter"/>'.i18xRegister(),
        description:
          'Idle seconds before AMOLED screensaver. 0 disables. Unit: seconds.<context="task parameter"/>'.i18xRegister(),
      },
      {
        id: "action",
        type: "select",
        default: "save_local",
        remember: false,
        label: 'Action<context="task parameter"/>'.i18xRegister(),
        description:
          'Save locally writes JSON on the PC only. Apply to device uploads system.json + macros.json (paths only) — never ROM/disk media.<context="task parameter"/>'.i18xRegister(),
        options: [
          {
            value: "save_local",
            label: 'Save locally (no device)<context="task parameter"/>'.i18xRegister(),
          },
          {
            value: "apply_device",
            label: 'Apply to device (upload config)<context="task parameter"/>'.i18xRegister(),
          },
        ],
      },
      {
        id: "port",
        type: "text",
        default: d.port,
        remember: false,
        label: 'Serial port for Apply (optional)<context="task parameter"/>'.i18xRegister(),
        description:
          'e.g. COM5. Empty → AskPort dialog. Used only when Action is Apply to device.<context="task parameter"/>'.i18xRegister(),
        visibleWhen: { action: "apply_device" },
      },
    ],
  };
}

/**
 * Resolve a concrete COMx for device serial tools (upload/macros).
 * Order: task parameter → ESP2_PORT → AskPort dialog (same as flash/upload).
 */
async function resolveDeviceSerialPort(_title) {
  const fromParam = String(GetParameter("port") || "").trim();
  if (fromParam) {
    return fromParam;
  }
  const asked = await AskPort(rootDir, _title);
  if (asked) {
    return asked;
  }
  throw new Error(
    'No serial port — set ESP2_PORT=COMx, fill the port field, or pick a port in the dialog.<context="task error"/>',
  );
}

export async function deviceConfig() {
  ReportProgress(0, "device-config");
  // Step 1: preset (µParameters in µGulp, or RequestForm on CLI).
  let preset = String(GetParameter("preset") || "").trim();
  if (!preset) {
    const step1 = (await RequestForm(_DeviceConfigPresetParameters())) || {};
    preset = String(step1.preset || "custom");
  }
  const defaults = formDefaultsFromPreset(rootDir, preset);
  Log('Loaded profile defaults from <preset/><context="task log"/>', {
    preset: defaults.preset,
  });

  // Step 2: edit form rebuilt from the chosen preset (fields actually update).
  let values;
  if (process.env.ESP2_DEVICE_CONFIG_JSON) {
    values = JSON.parse(process.env.ESP2_DEVICE_CONFIG_JSON);
  } else {
    values = (await RequestForm(_DeviceConfigEditForm(defaults))) || {};
  }

  const action = String(values.action || "save_local");
  const resolved = configFromFormValues(rootDir, {
    ...values,
    preset: defaults.preset,
    loadPreset: false,
  });
  if (!resolved.ok) {
    throw new Error(`device:config validation failed: ${resolved.errors.join("; ")}`);
  }
  const profileName = String(values.profileName || defaults.profileName || "local").trim();
  const saved = saveConfigLocal(
    rootDir,
    resolved.config,
    profileName,
    defaults.preset,
  );
  Log(
    'Saved device config locally → <path/> (profile=<profile/>). ROM/disk media were NOT uploaded.<context="task log"/>',
    { path: saved.localSystem, profile: profileName || "local" },
  );

  if (action === "apply_device") {
    const port = await resolveDeviceSerialPort("Apply device config — serial port");
    Log(
      'Applying config to device via <port/> (system.json + macros.json only).<context="task log"/>',
      { port },
    );
    applyConfigToDevice(rootDir, port);
    Log(
      'Device config applied. Power-cycle ESP][ to load. Media images were not uploaded.<context="task log"/>',
    );
  } else {
    Log(
      'Action=Save locally — no COM port, no upload, no flash.<context="task log"/>',
    );
  }
  ReportProgress(1, "device-config");
}
_Tag(deviceConfig, {
  gulpName: "device:config",
  µDisplayName: () => _DeviceConfigDisplayName(),
  µDescription:
    'Two-step form: choose a preset (fills fields), then edit/save locally or Apply to device. Opening the form uploads nothing; media bytes are never uploaded here.<context="µDescription"/>',
  µTooltip:
    'Pick galaxian-demo (or another preset) first — the next form shows those values. Apply is explicit.<context="µTooltip"/>',
  µGroup: 'Device<context="µGroup"/>',
  µOrder: 60,
  µParameters: _DeviceConfigPresetParameters(),
});

export async function deviceMacroRun() {
  ReportProgress(0, "device-macro-run");
  const macro = GetParameter("macro") || GetParameter("id") || "galaxian-start";
  const port = await resolveDeviceSerialPort("Device macro serial port");
  Log('Running macro <macro/> on <port/><context="task log"/>…', { macro, port });
  runNodeCli(
    "dev/tools/esp2-macro-run.mjs",
    ["--port", String(port), "--macro", String(macro)],
    "device:macro:run",
  );
  ReportProgress(1, "device-macro-run");
}
_Tag(deviceMacroRun, {
  gulpName: "device:macro:run",
  µDisplayName: 'Run Macro V<version/><context="µDisplayName"/>',
  µDescription:
    'Execute a named input macro on a live ESP][ (#ESP2MACRO RUN). Asks for COM port like flash/upload.<context="µDescription"/>',
  µTooltip:
    'Set ESP2_PORT=COMx to skip the port dialog.<context="µTooltip"/>',
  µGroup: 'Device<context="µGroup"/>',
  µOrder: 65,
  µParameters: [
    {
      name: "macro",
      type: "string",
      optional: true,
      default: "galaxian-start",
      µDisplayName: 'Macro id<context="task parameter"/>',
    },
  ],
});

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
  µDisplayName: 'Transfer File V<version/><context="µDisplayName"/>',
  µDescription:
    'Upload a local file to /esp2/... via framed serial protocol (dev only).<context="µDescription"/>',
  µGroup: 'Device<context="µGroup"/>',
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
  µDisplayName: 'USB Storage Mode V<version/><context="µDisplayName"/>',
  µDescription:
    'Enter or leave USB MSC ownership of the microSD (TinyUSB OTG).<context="µDescription"/>',
  µGroup: 'Device<context="µGroup"/>',
  µOrder: 71,
  µParameters: [
    { name: "port", type: "string", optional: false },
    { name: "leave", type: "boolean", optional: true },
  ],
});

gulp.task("device:config", deviceConfig);
gulp.task("device:macro:run", deviceMacroRun);
gulp.task("device:upload", deviceUpload);
gulp.task("device:usb-storage", deviceUsbStorage);
