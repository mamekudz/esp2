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
import { existsSync, mkdirSync, writeFileSync } from "node:fs";
import { spawn, spawnSync } from "node:child_process";
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
  LogTable,
  LogCallout,
  LogKeyValue,
  LogBadges,
} from "gulp-mu-gulp-api";
import {
  PLATFORM_DIYMORE,
  PLATFORM_RLCD42,
  buildFilterPlatformOptions,
  buildPlatformProjectControl,
  defaultPioEnvForPlatform,
  pioEnvsForPlatform,
  platformMeta,
  resolveActivePlatform,
  visibleOnDiymore,
  visibleOnRlcd42,
  visibleOnWaveshare,
} from "./dev/tools/esp2-platforms.mjs";
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
import {
  buildBleSetupForm,
  defaultControlBindings,
  formatGamepadChord,
  formatKeyboardBinding,
  openBleSession,
  parseGamepadChordText,
  parseKeyboardBindingText,
} from "./dev/tools/esp2-ble-setup.mjs";
import { RunGitBackup } from "./dev/tools/git-backup.mjs";
import {
  AskPort,
  ListPorts,
  MonitorArgs,
  Pio,
  PIO_ENV,
  PIO_ENV_DEFAULT,
  PIO_ENVS,
  ResolvePioEnv,
  UploadArgs,
} from "./dev/tools/pio.mjs";
import { parseMediaUploadProgressLine } from "./dev/tools/esp2-upload.mjs";
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
import {
  generateProfilesFromApple2jsAudit,
} from "./dev/tools/device-profiles-from-apple2js.mjs";
import {
  activateProfiles,
  resolveProfileSelection,
  DEMO_PROFILE_IDS,
} from "./dev/tools/device-profiles-activate.mjs";
import { writeProfileCatalogMarkdown } from "./dev/tools/device-profile-catalog-doc.mjs";
import {
  buildGamepadSelectOptions,
  listXInputGamepads,
  pulseXInputRumble,
  waitForXInputButtonPress,
} from "./dev/tools/esp2-input-bridge.mjs";

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
    // <platformLabel/> filled from Project Control Target (platform option label).
    'Device (<platformLabel/>)<context="µGroup"/>': "collapsed",
    'Device (<platformLabel/>)/SD Card<context="µGroup"/>': "collapsed",
    'Device<context="µGroup"/>': "collapsed",
    'diymore Hardware<context="µGroup"/>': "collapsed",
    'RLCD 4.2 Hardware<context="µGroup"/>': "collapsed",
  },
};

/** Project Control bar (Target radio). Same schema as microgulp.config.json. */
export const µControls = [buildPlatformProjectControl()];

const rootDir = dirname(fileURLToPath(import.meta.url));

/**
 * @param {Function} _task
 * @param {object} _meta
 */
/**
 * Default task visibility from µGroup when µVisible is omitted.
 * Waveshare production Device / Apple II Device → waveshare only.
 * diymore Hardware → diymore only. Firmware/Tools/Docs/… stay common.
 * @param {string} [_group]
 */
function _DefaultVisibleForGroup(_group) {
  const g = String(_group ?? "");
  if (g.startsWith("diymore Hardware")) return visibleOnDiymore;
  if (g.startsWith("RLCD 4.2 Hardware")) return visibleOnRlcd42;
  if (g.startsWith("Device") || g.startsWith("Apple II/Device")) {
    return visibleOnWaveshare;
  }
  return undefined;
}

/** Device group — dynamic Target label from Project Control (`platform`). */
function _DeviceGroup() {
  return 'Device (<platformLabel/>)<context="µGroup"/>';
}

/** Nested SD Card subgroup under Device. */
function _DeviceSdGroup() {
  return 'Device (<platformLabel/>)/SD Card<context="µGroup"/>';
}

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
  if (_meta.µEnabled != null) _task.µEnabled = _meta.µEnabled;
  if (_meta.µDisabledTooltip) {
    _task.µDisabledTooltip = _meta.µDisabledTooltip.i18xRegister();
  }
  const visible =
    _meta.µVisible != null
      ? _meta.µVisible
      : _DefaultVisibleForGroup(_meta.µGroup);
  if (visible != null) _task.µVisible = visible;
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
  const platformId = resolveActivePlatform();
  const meta = platformMeta(platformId);
  const allowed = [...meta.pioEnvs];
  try {
    const p = GetParameter("env");
    if (p) {
      return ResolvePioEnv(String(p), {
        platformId,
        allowedEnvs: allowed,
        defaultEnv: meta.defaultPioEnv,
      });
    }
  } catch (err) {
    if (err && /Unknown (PlatformIO env|ESP2_PIO_ENV)/.test(String(err.message || err))) {
      throw err;
    }
  }
  return ResolvePioEnv(undefined, {
    platformId,
    allowedEnvs: allowed,
    defaultEnv: meta.defaultPioEnv,
  });
}

function _FirmwareEnvParameters() {
  const platformId = resolveActivePlatform();
  const allowed = pioEnvsForPlatform(platformId);
  const def = defaultPioEnvForPlatform(platformId);
  const labelFor = (id) => {
    if (id === "apple2_text") {
      return 'apple2_text (production Apple II)<context="task parameter"/>'.i18xRegister();
    }
    if (id === "bringup") {
      return 'bringup (Waveshare HW diagnostics)<context="task parameter"/>'.i18xRegister();
    }
    if (id === "core_smoke") {
      return 'core_smoke (compile-only)<context="task parameter"/>'.i18xRegister();
    }
    if (id === "diymore_bringup") {
      return 'diymore_bringup (identity probe)<context="task parameter"/>'.i18xRegister();
    }
    if (id === "diymore_display_probe") {
      return 'diymore_display_probe (ILI9341 color bars)<context="task parameter"/>'.i18xRegister();
    }
    if (id === "diymore_apple2") {
      return 'diymore_apple2 (Apple II + ILI9341)<context="task parameter"/>'.i18xRegister();
    }
    if (id === "rlcd42_bringup") {
      return 'rlcd42_bringup (identity probe)<context="task parameter"/>'.i18xRegister();
    }
    if (id === "rlcd42_display_probe") {
      return 'rlcd42_display_probe (ST7305 marks)<context="task parameter"/>'.i18xRegister();
    }
    if (id === "rlcd42_apple2") {
      return 'rlcd42_apple2 (Apple II + ST7305)<context="task parameter"/>'.i18xRegister();
    }
    return id;
  };
  return [
    {
      id: "env",
      type: "select",
      optional: true,
      default: def,
      remember: true,
      label: 'PlatformIO environment<context="task parameter"/>'.i18xRegister(),
      description:
        'Env list follows Project Control Target (platform). Waveshare: apple2_text/bringup/core_smoke. diymore: diymore_bringup.<context="task parameter"/>'.i18xRegister(),
      options: allowed.map((id) => ({
        value: id,
        label: labelFor(id),
      })),
    },
  ];
}

function _FirmwareDisplayName(kind) {
  // <platformLabel/> is filled by µGulp from Project Control id `platform`.
  if (kind === "flash") {
    return 'Build & Upload – <platformLabel/> V<version/><context="µDisplayName"/>'.i18xRegister();
  }
  if (kind === "upload") {
    return 'Upload – <platformLabel/> V<version/><context="µDisplayName"/>'.i18xRegister();
  }
  if (kind === "monitor") {
    return 'Serial Monitor – <platformLabel/> V<version/><context="µDisplayName"/>'.i18xRegister();
  }
  return 'Build – <platformLabel/> V<version/><context="µDisplayName"/>'.i18xRegister();
}


export async function build() {
  ReportProgress(0, "build");
  const platformId = resolveActivePlatform();
  const env = firmwareEnv();
  Log('Building ESP][ (platform <platform/>, env <env/>)…<context="task log"/>', {
    platform: platformId,
    env,
  });
  await Pio(rootDir, ["run", "-e", env], "pio build");
  ReportProgress(1, "build");
  Log('Build OK.<context="task log"/>');
  PlaySignal("success");
}
_Tag(build, {
  gulpName: "build",
  µDisplayName: _FirmwareDisplayName("build"),
  µDescription:
    'Compiles ESP][ with PlatformIO. Env follows Project Control Target (waveshare → apple2_text; diymore → diymore_bringup).<context="µDescription"/>',
  µIcon: "\u2692",
  µGroup: 'Firmware<context="µGroup"/>',
  µOrder: 10,
  µExecutionConcurrency: false,
  µParameters: _FirmwareEnvParameters,
});

export async function flash() {
  ReportProgress(0, "flash-build");
  const platformId = resolveActivePlatform();
  const env = firmwareEnv();
  const device = await resolveEsp2DeviceForTask("Flash — select ESP][ device", {
    operation: "flash",
    allowUnidentifiedPick: true,
  });
  const port = device.port;
  const { assertFlashSdSafe } = await import("./dev/tools/esp2-sd-flash-guard.mjs");
  const { runUsbStorageCommand } = await import("./dev/tools/esp2-sd-ownership-session.mjs");
  assertFlashSdSafe(rootDir, port, {
    log: (msg) => Log(`${msg}<context="task log"/>`),
    tryReturn: (p) => {
      try {
        runUsbStorageCommand(rootDir, p, "leave");
        return true;
      } catch {
        return false;
      }
    },
  });
  Log(
    'Build + upload platform=<platform/> env=<env/> → <port/><context="task log"/>…',
    {
      platform: platformId,
      env,
      port,
    },
  );
  await Pio(rootDir, ["run", "-e", env], "pio build");
  ReportProgress(0.5, "flash-upload");
  await Pio(rootDir, UploadArgs(port, env), "pio upload", {
    onProgress: ({ fraction }) => {
      // Build already done — map esptool 0..1 into the second half of the bar.
      ReportProgress(0.5 + 0.5 * fraction, "flash-upload");
    },
  });
  ReportProgress(1, "flash-upload");
  Log('Flash finished (platform <platform/>, env <env/>).<context="task log"/>', {
    platform: platformId,
    env,
  });
  PlaySignal("success");
}
_Tag(flash, {
  gulpName: "flash",
  µDisplayName: _FirmwareDisplayName("flash"),
  µDescription:
    'Rebuilds firmware and flashes the currently connected ESP][ for the Project Control Target. COM is resolved live (USB + identity), not remembered.<context="µDescription"/>',
  µTooltip:
    'Target (Waveshare/diymore) selects which board to find. COM numbers are not device identity.<context="µTooltip"/>',
  µIcon: "\u26A1",
  µGroup: 'Firmware<context="µGroup"/>',
  µOrder: 40,
  µExecutionConcurrency: false,
  µExecutionRestrictions: { deny: ["upload", "build", "backup"] },
  µParameters: _FirmwareEnvParameters,
});

export async function upload() {
  ReportProgress(0, "upload");
  const platformId = resolveActivePlatform();
  const env = firmwareEnv();
  const device = await resolveEsp2DeviceForTask("Upload — select ESP][ device", {
    operation: "flash",
    allowUnidentifiedPick: true,
  });
  const port = device.port;
  const { assertFlashSdSafe } = await import("./dev/tools/esp2-sd-flash-guard.mjs");
  const { runUsbStorageCommand } = await import("./dev/tools/esp2-sd-ownership-session.mjs");
  assertFlashSdSafe(rootDir, port, {
    log: (msg) => Log(`${msg}<context="task log"/>`),
    tryReturn: (p) => {
      try {
        runUsbStorageCommand(rootDir, p, "leave");
        return true;
      } catch {
        return false;
      }
    },
  });
  Log(
    'Uploading platform=<platform/> env=<env/> → <port/><context="task log"/>…',
    {
      platform: platformId,
      env,
      port,
    },
  );
  await Pio(rootDir, UploadArgs(port, env), "pio upload", {
    onProgress: ({ fraction }) => {
      ReportProgress(fraction, "upload");
    },
  });
  ReportProgress(1, "upload");
  Log('Upload finished (platform <platform/>, env <env/>).<context="task log"/>', {
    platform: platformId,
    env,
  });
  PlaySignal("success");
}
_Tag(upload, {
  gulpName: "upload",
  µDisplayName: _FirmwareDisplayName("upload"),
  µDescription:
    'Flashes the last build for the PlatformIO env of the selected Target onto the live ESP][ device.<context="µDescription"/>',
  µTooltip:
    'Resolves the physical ESP][ via USB + firmware identity. COM is runtime state only.<context="µTooltip"/>',
  µIcon: "\u2191",
  µGroup: 'Firmware<context="µGroup"/>',
  µOrder: 30,
  µExecutionConcurrency: false,
  µExecutionRestrictions: { deny: ["flash", "monitor", "backup"] },
  µParameters: _FirmwareEnvParameters,
});

export async function size() {
  ReportProgress(0, "size");
  await Pio(rootDir, ["run", "-e", firmwareEnv(), "-t", "size"], "pio size");
  ReportProgress(1, "size");
}
_Tag(size, {
  gulpName: "size",
  µDisplayName: 'Firmware Memory Usage – <platformLabel/> V<version/><context="µDisplayName"/>',
  µDescription:
    'Shows RAM / Flash usage of the last firmware build for the selected Target.<context="µDescription"/>',
  µIcon: "\u25A6",
  µGroup: 'Firmware<context="µGroup"/>',
  µOrder: 50,
  µExecutionConcurrency: true,
});

export async function clean() {
  ReportProgress(0, "clean");
  const platformId = resolveActivePlatform();
  const env = firmwareEnv();
  Log('Cleaning platform=<platform/> env=<env/>…<context="task log"/>', {
    platform: platformId,
    env,
  });
  await Pio(rootDir, ["run", "-e", env, "-t", "clean"], "pio clean");
  ReportProgress(1, "clean");
  Log('Build artefacts removed.<context="task log"/>');
}
_Tag(clean, {
  gulpName: "clean",
  µDisplayName: 'Clean – <platformLabel/> V<version/><context="µDisplayName"/>',
  µDescription:
    'Removes .pio/build artefacts for the PlatformIO env of the selected Target.<context="µDescription"/>',
  µIcon: "\u239A",
  µGroup: 'Firmware<context="µGroup"/>',
  µOrder: 60,
  µExecutionConcurrency: true,
});

export const rebuild = gulp.series(clean, build);
_Tag(rebuild, {
  gulpName: "rebuild",
  µDisplayName: 'Rebuild – <platformLabel/> V<version/><context="µDisplayName"/>',
  µDescription: 'clean → build for the PlatformIO env of the selected Target.<context="µDescription"/>',
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
  const platformId = resolveActivePlatform();
  const device = await resolveEsp2DeviceForTask("Serial monitor — select ESP][ device", {
    operation: "monitor",
    allowUnidentifiedPick: true,
  });
  const port = device.port;
  Log(
    'Opening serial monitor (platform <platform/>)… Ctrl+C to stop.<context="task log"/>',
    { platform: platformId },
  );
  await Pio(rootDir, MonitorArgs(port), "pio monitor");
}
_Tag(monitor, {
  gulpName: "monitor",
  µDisplayName: _FirmwareDisplayName("monitor"),
  µDescription:
    'Opens the PlatformIO serial monitor at 115200 baud for the selected Target’s live ESP][.<context="µDescription"/>',
  µTooltip:
    'Device is resolved live. Use device:port:release if Access denied.<context="µTooltip"/>',
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
  try {
    const catalog = writeProfileCatalogMarkdown({ root: rootDir });
    Log(
      'Refreshed profile catalog (<count format="int"/> profiles) → docs/device/generated/profile-catalog.md<context="task log"/>',
      { count: catalog.count },
    );
  } catch (e) {
    Warn(`profile catalog refresh skipped: ${e?.message || e}`);
  }
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

/**
 * Like runNodeCli, but streams stdout/stderr and maps PROGRESS / esptool lines
 * into µGulp ReportProgress (0..1 within [progressLo, progressHi]).
 * @param {string} relScript
 * @param {string[]} args
 * @param {string} label
 * @param {{ progressLo?: number, progressHi?: number, progressId?: string }} [opts]
 */
function runNodeCliWithProgress(relScript, args, label, opts = {}) {
  const script = join(rootDir, relScript);
  const lo = Number.isFinite(opts.progressLo) ? opts.progressLo : 0;
  const hi = Number.isFinite(opts.progressHi) ? opts.progressHi : 1;
  const progressId = opts.progressId || label;
  return new Promise((resolve, reject) => {
    const child = spawn(process.execPath, [script, ...args], {
      cwd: rootDir,
      shell: false,
      windowsHide: true,
      stdio: ["ignore", "pipe", "pipe"],
    });
    let carry = "";
    const feed = (chunk, stream) => {
      const text = chunk.toString();
      stream.write(text);
      carry += text;
      const parts = carry.split(/\r?\n/);
      carry = parts.pop() ?? "";
      for (const line of parts) {
        const media = parseMediaUploadProgressLine(line);
        if (media != null) {
          ReportProgress(lo + (hi - lo) * media, progressId);
        }
      }
    };
    child.stdout.on("data", (c) => feed(c, process.stdout));
    child.stderr.on("data", (c) => feed(c, process.stderr));
    child.on("error", reject);
    child.on("close", (code) => {
      if (code === 0) resolve();
      else reject(new Error(`${label} failed (exit ${code})`));
    });
  });
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
  const platformId = resolveActivePlatform();
  const meta = platformMeta(platformId);
  const env = firmwareEnv();
  Log('Active platform: <platform/> (<label/>)<context="task log"/>', {
    platform: platformId,
    label: meta.label,
  });
  Log('Active PlatformIO env: <env/><context="task log"/>', { env });
  Log('Envs for this Target: <list/><context="task log"/>', {
    list: meta.pioEnvs.join(", "),
  });
  Log('All known envs: <list/><context="task log"/>', {
    list: PIO_ENVS.join(", "),
  });
  Log(
    'Override via Project Control Target, ESP2_PIO_ENV, or µGulp parameter env.<context="task log"/>',
  );
  ReportProgress(1, "firmware-env");
  PlaySignal("success");
  return { platform: platformId, env, available: [...meta.pioEnvs] };
}
_Tag(firmwareEnvTask, {
  gulpName: "firmware:env",
  µDisplayName: 'Show Active Firmware Env – <platformLabel/> V<version/><context="µDisplayName"/>',
  µDescription:
    'Read-only: prints the Project Control Target, active PlatformIO environment, and envs allowed for that Target.<context="µDescription"/>',
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
  const { loadGitCatalog, loadWebCatalog, mergeCatalogs } = await import(
    "./dev/tools/apple2js/catalog.mjs"
  );
  const { auditCatalog, summarizeAudit } = await import(
    "./dev/tools/apple2js/audit.mjs"
  );
  const { catalogTableRows } = await import("./dev/tools/apple2js/catalog-view.mjs");

  let gitEntries = [];
  let webEntries = [];
  try {
    gitEntries = loadGitCatalog().entries;
  } catch (e) {
    Warn(String(e.message || e));
  }
  try {
    webEntries = loadWebCatalog().entries;
  } catch (e) {
    Warn(String(e.message || e));
  }
  if (!gitEntries.length && !webEntries.length) {
    throw new Error(
      "No apple2js catalog cache. Run: gulp apple2js:sync",
    );
  }
  const merged = mergeCatalogs(gitEntries, webEntries);
  const audited = auditCatalog(merged);
  const summary = summarizeAudit(audited);
  const rows = catalogTableRows(audited);

  LogCallout({
    variant: "info",
    title: "Apple ][js online catalog",
    message:
      "Cached git + website index (offline). Path metadata only — disk images are not shown and are not redistributed by ESP][. Activate media with setup:profiles (LOCAL_TEST_ONLY).",
  });
  LogKeyValue({
    title: "Counts",
    items: [
      { key: "merged entries", value: String(merged.length) },
      { key: "git", value: String(gitEntries.length) },
      { key: "website", value: String(webEntries.length) },
      { key: "REDISTRIBUTABLE", value: String(summary.REDISTRIBUTABLE || 0) },
      { key: "USER_SUPPLIED_ONLY", value: String(summary.USER_SUPPLIED_ONLY || 0) },
      { key: "UNKNOWN", value: String(summary.UNKNOWN || 0) },
      { key: "DO_NOT_DISTRIBUTE", value: String(summary.DO_NOT_DISTRIBUTE || 0) },
    ],
  });
  LogBadges({
    title: "Rights mix",
    items: [
      { label: `USER_SUPPLIED_ONLY ${summary.USER_SUPPLIED_ONLY || 0}`, variant: "warning" },
      { label: `UNKNOWN ${summary.UNKNOWN || 0}`, variant: "neutral" },
      { label: `DO_NOT_DISTRIBUTE ${summary.DO_NOT_DISTRIBUTE || 0}`, variant: "error" },
      { label: `REDISTRIBUTABLE ${summary.REDISTRIBUTABLE || 0}`, variant: "success" },
    ],
  });
  LogTable({
    columns: [
      { id: "Title", label: "Title" },
      { id: "Category", label: "Category" },
      { id: "Id", label: "Profile / id" },
      { id: "Source", label: "Source" },
      { id: "Rights", label: "Rights" },
    ],
    rows,
    hyphenate: false,
  });
  ReportProgress(1, "apple2js-catalog");
  PlaySignal("success");
}
_Tag(apple2jsCatalog, {
  gulpName: "apple2js:catalog",
  µDisplayName: 'Show Online Catalog V<version/><context="µDisplayName"/>',
  µDescription:
    'Shows the cached apple2js git+website catalog as a readable dashboard table (title, category, id, source, rights). Offline if cache present — not a raw JSON dump. Use apple2js:sync first when empty.<context="µDescription"/>',
  µGroup: 'Media / Catalog<context="µGroup"/>',
  µOrder: 61,
  µExecutionConcurrency: false,
});

export async function apple2jsAudit() {
  ReportProgress(0, "apple2js-audit");
  const { loadGitCatalog, loadWebCatalog, mergeCatalogs } = await import(
    "./dev/tools/apple2js/catalog.mjs"
  );
  const { auditCatalog, summarizeAudit } = await import(
    "./dev/tools/apple2js/audit.mjs"
  );
  const { catalogTableRows } = await import("./dev/tools/apple2js/catalog-view.mjs");
  const { normalizeToGameMetadata } = await import(
    "./dev/tools/apple2js/normalize.mjs"
  );
  const { existsSync } = await import("node:fs");
  const { join } = await import("node:path");
  const paths = await import("./dev/tools/apple2js/paths.mjs");

  let gitEntries = [];
  let webEntries = [];
  let pin = null;
  try {
    const g = loadGitCatalog();
    gitEntries = g.entries;
    pin = g.pin;
  } catch (e) {
    Warn(String(e.message || e));
  }
  try {
    webEntries = loadWebCatalog().entries;
  } catch (e) {
    Warn(String(e.message || e));
  }
  if (!gitEntries.length && !webEntries.length) {
    throw new Error(
      "No apple2js catalog cache. Run: gulp apple2js:sync",
    );
  }

  const merged = mergeCatalogs(gitEntries, webEntries);
  const audited = auditCatalog(merged);
  const summary = summarizeAudit(audited);

  const outDir = join(rootDir, "docs", "media", "generated");
  paths.ensureDir(outDir);
  const auditPath = join(outDir, "apple2js-catalog-audit.json");
  const prevPath = join(outDir, "apple2js-catalog-audit.prev.json");
  let previous = null;
  if (existsSync(auditPath)) {
    previous = paths.readJson(auditPath);
    paths.writeJson(prevPath, previous);
  }
  const nextAudit = {
    schemaVersion: 1,
    generatedAt: new Date().toISOString(),
    upstreamPin: pin || (existsSync(paths.PIN_FILE) ? paths.readJson(paths.PIN_FILE) : null),
    webIndexCachePresent: existsSync(paths.WEB_INDEX_CACHE),
    gitEntryCount: gitEntries.length,
    webEntryCount: webEntries.length,
    mergedEntryCount: merged.length,
    summary,
    entries: audited,
  };
  paths.writeJson(auditPath, nextAudit);

  let diff = null;
  if (previous) {
    const { diffApple2jsAudits } = await import("./dev/tools/apple2js/sync_diff.mjs");
    diff = diffApple2jsAudits(previous, nextAudit);
    paths.writeJson(join(outDir, "apple2js-catalog-diff.json"), {
      generatedAt: new Date().toISOString(),
      ...diff,
    });
  }

  const stubDir = join(rootDir, "library", "metadata");
  paths.ensureDir(stubDir);
  const interesting = audited.filter((a) =>
    ["choplifter", "nightmission", "audit"].includes(a.basename),
  );
  for (const a of interesting) {
    const meta = normalizeToGameMetadata(a, { upstreamCommit: pin?.commit });
    paths.writeJson(join(stubDir, `${meta.id}.game.json`), meta);
  }

  LogCallout({
    variant: "info",
    title: "Apple ][js catalog audit",
    message:
      "Provenance classification for the cached online catalog. Website listing is not a redistribution grant. Machine audit JSON is written under docs/media/generated/ for tooling — this view is the human summary.",
  });
  LogKeyValue({
    title: "Audit summary",
    items: [
      { key: "merged entries", value: String(merged.length) },
      { key: "wrote", value: "docs/media/generated/apple2js-catalog-audit.json" },
      { key: "stubs", value: interesting.map((a) => a.id || a.basename).join(", ") || "(none)" },
      {
        key: "diff (added/removed/changed)",
        value: diff?.counts
          ? `${diff.counts.added ?? 0}/${diff.counts.removed ?? 0}/${diff.counts.changed ?? 0}`
          : "(first run / no previous)",
      },
      {
        key: "provenance blocked",
        value: String((diff?.provenanceBlocked || []).length),
      },
    ],
  });
  LogBadges({
    title: "Redistribution status counts",
    items: [
      { label: `USER_SUPPLIED_ONLY ${summary.USER_SUPPLIED_ONLY || 0}`, variant: "warning" },
      { label: `UNKNOWN ${summary.UNKNOWN || 0}`, variant: "neutral" },
      { label: `DO_NOT_DISTRIBUTE ${summary.DO_NOT_DISTRIBUTE || 0}`, variant: "error" },
      { label: `REDISTRIBUTABLE ${summary.REDISTRIBUTABLE || 0}`, variant: "success" },
    ],
  });

  // Compact rights-focused table (same columns as catalog show).
  LogTable({
    columns: [
      { id: "Title", label: "Title" },
      { id: "Category", label: "Category" },
      { id: "Id", label: "Id" },
      { id: "Source", label: "Source" },
      { id: "Rights", label: "Rights" },
    ],
    rows: catalogTableRows(audited),
    hyphenate: false,
  });

  if ((diff?.provenanceBlocked || []).length) {
    LogCallout({
      variant: "warning",
      title: "Provenance blocked",
      message: (diff.provenanceBlocked || [])
        .slice(0, 20)
        .map((x) => (typeof x === "string" ? x : JSON.stringify(x)))
        .join("\n"),
    });
    PlaySignal("warning");
  } else {
    PlaySignal("success");
  }
  ReportProgress(1, "apple2js-audit");
}
_Tag(apple2jsAudit, {
  gulpName: "apple2js:audit",
  µDisplayName: 'Audit Online Catalog V<version/><context="µDisplayName"/>',
  µDescription:
    'Human-readable provenance audit of the cached apple2js catalog (table + summary). Also writes docs/media/generated audit JSON for tooling — not shown as raw JSON in the dashboard.<context="µDescription"/>',
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
  if (!title) throw new Error("apple2:device:sync requires --title <id>");
  const hinted = String(cliArg("port") || "").trim();
  const device = await resolveEsp2DeviceForTask("Sync game — ESP][ device", {
    formPort: hinted,
    operation: "media",
  });
  const port = device.port;
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
    listProfileIds(rootDir).includes(title)
      ? title
      : listProfileIds(rootDir).includes(`${title}-demo`)
        ? `${title}-demo`
        : title === "starblazer" && listProfileIds(rootDir).includes("star-blazer")
          ? "star-blazer"
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
  µVisible: visibleOnWaveshare,
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

/**
 * Regenerate path-only device profiles from the apple2js catalog audit.
 * Does not download disks. Skips hand-maintained demo profiles unless --force.
 */
export async function deviceProfilesGenerate() {
  ReportProgress(0, "device-profiles-generate");
  const force = cliFlag("force");
  const category = String(cliArg("category") || "").trim();
  const categories = category
    ? category.split(",").map((s) => s.trim()).filter(Boolean)
    : null;
  Log(
    'Generating device profiles from apple2js audit (path-only; no media download)…<context="task log"/>',
  );
  const result = generateProfilesFromApple2jsAudit({ force, categories });
  const catalog = writeProfileCatalogMarkdown();
  Log(
    'Wrote <count/> profiles (skipped protected: <protected/>). Catalog → <catalog/>.<context="task log"/>',
    {
      count: String(result.written.length),
      protected: result.skippedProtected.join(",") || "(none)",
      catalog: catalog.outPath,
    },
  );
  ReportProgress(1, "device-profiles-generate");
  PlaySignal("success");
}
_Tag(deviceProfilesGenerate, {
  gulpName: "device:profiles:generate",
  µDisplayName: 'Generate Apple ][js device profiles V<version/><context="µDisplayName"/>',
  µDescription:
    'Create path-only config/device/profiles/* from the apple2js catalog audit. No disk download. Hand demos (galaxian-demo, little-brick-out, star-blazer) are preserved unless --force.<context="µDescription"/>',
  µGroup: 'Tools<context="µGroup"/>',
  µIcon: "\uD83D\uDCCB",
  µOrder: 2,
  µExecutionConcurrency: false,
  µVisible: visibleOnWaveshare,
  µParameters: [
    {
      id: "category",
      type: "text",
      optional: true,
      label: 'Optional category filter (e.g. Game)<context="task parameter"/>'.i18xRegister(),
    },
    {
      id: "force",
      type: "boolean",
      optional: true,
      label: 'Overwrite hand-maintained demos<context="task parameter"/>'.i18xRegister(),
    },
  ],
});
gulp.task("device:profiles:generate", deviceProfilesGenerate);

function _SetupProfilesGroupForm() {
  return {
    title: 'Activate device profiles — choose group<context="task parameter"/>'.i18xRegister(),
    submitLabel: 'Continue<context="button text"/>'.i18xRegister(),
    fields: [
      {
        id: "selection",
        type: "select",
        default: "demos",
        remember: false,
        label: 'Which profiles?<context="task parameter"/>'.i18xRegister(),
        description:
          'Downloads LOCAL_TEST_ONLY apple2js media into gitignored local/apple2/. Pick curated demos, a whole category, or continue to pick one title.<context="task parameter"/>'.i18xRegister(),
        options: [
          {
            value: "demos",
            label: 'Curated demos (Galaxian, Little Brick Out, Star Blazer)<context="task parameter"/>'.i18xRegister(),
          },
          {
            value: "__pick__",
            label: 'Pick one profile (next step)<context="task parameter"/>'.i18xRegister(),
          },
          ...DEVICE_CONFIG_CATEGORY_OPTIONS.map((o) => ({
            value: o.value,
            label: `${o.label} (all in category)`,
          })),
          {
            value: "all-safe",
            label: 'All profiles except DO_NOT_DISTRIBUTE (large)<context="task parameter"/>'.i18xRegister(),
          },
        ],
      },
      {
        id: "syncRoms",
        type: "select",
        default: "yes",
        remember: false,
        label: 'Also download redistributable ROMs?<context="task parameter"/>'.i18xRegister(),
        options: [
          {
            value: "yes",
            label: 'Yes (AppleIIGo etc.)<context="task parameter"/>'.i18xRegister(),
          },
          {
            value: "no",
            label: 'No — disks only<context="task parameter"/>'.i18xRegister(),
          },
        ],
      },
    ],
  };
}

/**
 * Step for "__pick__": choose a browse group (keeps selects under ~200 options).
 */
function _SetupProfilesPickGroupForm() {
  return {
    title: 'Activate device profiles — pick category<context="task parameter"/>'.i18xRegister(),
    submitLabel: 'Continue<context="button text"/>'.i18xRegister(),
    fields: [
      {
        id: "group",
        type: "select",
        default: "demos",
        remember: false,
        label: 'Browse group<context="task parameter"/>'.i18xRegister(),
        options: [
          {
            value: "demos",
            label: 'Curated demos (Galaxian, Little Brick Out, Star Blazer)<context="task parameter"/>'.i18xRegister(),
          },
          ...DEVICE_CONFIG_CATEGORY_OPTIONS,
        ],
      },
    ],
  };
}

/**
 * @param {string} group demos | category:Game | …
 */
function _SetupProfilesPickForm(group) {
  const ids = _profileIdsInGroup(group);
  const options = ids.map((id) => ({ value: id, label: _profilePresetLabel(id) }));
  const defaultId = options.some((o) => o.value === "star-blazer")
    ? "star-blazer"
    : options[0]?.value || "galaxian-demo";
  return {
    title: 'Activate device profiles — pick title<context="task parameter"/>'.i18xRegister(),
    submitLabel: 'Activate selection<context="button text"/>'.i18xRegister(),
    fields: [
      {
        id: "profile",
        type: "select",
        default: defaultId,
        remember: false,
        label: 'Profile<context="task parameter"/>'.i18xRegister(),
        options,
      },
      {
        id: "seedLocal",
        type: "select",
        default: "yes",
        remember: false,
        label: 'Seed local/device/config from this profile?<context="task parameter"/>'.i18xRegister(),
        options: [
          {
            value: "yes",
            label: 'Yes — seed local config<context="task parameter"/>'.i18xRegister(),
          },
          {
            value: "no",
            label: 'No — download/prepare only<context="task parameter"/>'.i18xRegister(),
          },
        ],
      },
    ],
  };
}

function _SetupProfilesSeedForm(selection) {
  const all = listProfileIds(rootDir);
  const seedOptions = [
    { value: "", label: 'Do not seed local config<context="task parameter"/>'.i18xRegister() },
    ...DEMO_PROFILE_IDS.filter((id) => all.includes(id)).map((id) => ({
      value: id,
      label: _profilePresetLabel(id),
    })),
  ];
  let defaultSeed = "";
  if (selection === "demos" || selection === "star-blazer") defaultSeed = "star-blazer";
  else if (DEMO_PROFILE_IDS.includes(selection)) defaultSeed = selection;
  return {
    title: 'Activate device profiles — seed config?<context="task parameter"/>'.i18xRegister(),
    submitLabel: 'Start download<context="button text"/>'.i18xRegister(),
    fields: [
      {
        id: "seedProfile",
        type: "select",
        default: defaultSeed,
        remember: false,
        label: 'Seed local/device/config from<context="task parameter"/>'.i18xRegister(),
        options: seedOptions,
      },
    ],
  };
}

/**
 * Fresh-clone helper: selectively download/prepare media for device profiles.
 */
export async function setupProfiles() {
  ReportProgress(0, "setup-profiles");
  const cliSelection =
    String(cliArg("profile") || cliArg("profiles") || cliArg("selection") || "").trim();
  const cliSeed = String(cliArg("seed") || "").trim();

  let selection = cliSelection;
  let seedProfile = cliSeed;
  let syncRoms = true;

  if (!selection) {
    // Selects only — no µParameters text fields (µGulp would show those instead).
    const step1 = (await RequestForm(_SetupProfilesGroupForm())) || {};
    selection = String(step1.selection || "demos").trim();
    syncRoms = String(step1.syncRoms || "yes").toLowerCase() !== "no";

    if (selection === "__pick__") {
      const pickGroup =
        String(((await RequestForm(_SetupProfilesPickGroupForm())) || {}).group || "demos").trim() ||
        "demos";
      const stepPick = (await RequestForm(_SetupProfilesPickForm(pickGroup))) || {};
      selection = String(stepPick.profile || "").trim();
      if (!selection) {
        throw new Error("setup:profiles: no profile selected.");
      }
      if (String(stepPick.seedLocal || "yes").toLowerCase() !== "no") {
        seedProfile = selection;
      } else {
        seedProfile = "";
      }
    } else {
      const stepSeed = (await RequestForm(_SetupProfilesSeedForm(selection))) || {};
      seedProfile = String(stepSeed.seedProfile || "").trim();
    }
  } else {
    if (process.argv.includes("--no-roms")) syncRoms = false;
    else if (cliFlag("roms") || cliFlag("sync-roms")) syncRoms = true;
  }

  const ids = resolveProfileSelection(rootDir, selection);
  if (!ids.length) {
    throw new Error(
      `setup:profiles: no profiles matched selection=${selection}. Run device:profiles:generate first.`,
    );
  }

  Log(
    'Activating <count/> profile(s): <ids/>…<context="task log"/>',
    { count: String(ids.length), ids: ids.join(", ") },
  );

  const result = await activateProfiles(rootDir, ids, {
    download: true,
    prepare: true,
    syncRoms,
    seedProfileId: seedProfile || null,
    log: (msg) => Log(String(msg)),
  });

  const failed = result.results.filter((r) => !r.ok);
  const deferred = result.results.filter((r) => r.prepareError);
  Log(
    'Activation finished. ok=<ok/> failed=<failed/> prepare_deferred=<deferred/> seeded=<seeded/><context="task log"/>',
    {
      ok: String(result.results.length - failed.length),
      failed: String(failed.length),
      deferred: String(deferred.length),
      seeded: result.seeded || "(none)",
    },
  );
  if (failed.length) {
    Warn(
      'Some profiles failed: <ids/>. Media is LOCAL_TEST_ONLY / user-supplied.<context="task warning"/>',
      {
        ids: failed.map((r) => `${r.id}${r.error ? ` (${r.error})` : ""}`).join("; "),
      },
    );
  }
  ReportProgress(1, "setup-profiles");
  PlaySignal(failed.length ? "warning" : "success");
}
_Tag(setupProfiles, {
  gulpName: "setup:profiles",
  µDisplayName: 'Activate device profiles V<version/><context="µDisplayName"/>',
  µDescription:
    'After clone: choose curated demos, a category, or a single Apple ][js profile; download LOCAL_TEST_ONLY media, prepare runtime disks, optionally seed local/device/config. Does not flash firmware.<context="µDescription"/>',
  µTooltip:
    'Selective alternative to first-run when you want Star Blazer or other catalog titles.<context="µTooltip"/>',
  µGroup: 'Tools<context="µGroup"/>',
  µIcon: "\u2B07",
  µOrder: 1.5,
  µExecutionConcurrency: false,
  µVisible: visibleOnWaveshare,
});
gulp.task("setup:profiles", setupProfiles);

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
        // Do not steal scroll when the user starts another task (gulpfile may reload).
        scroll: false,
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
 * Static category options for device:config (avoid scanning ~250 JSONs on every open).
 * Counts are approximate; regenerate profiles if the catalog changes a lot.
 */
const DEVICE_CONFIG_CATEGORY_OPTIONS = Object.freeze([
  { value: "category:Game", label: "Game" },
  { value: "category:Interactive Fiction", label: "Interactive Fiction" },
  { value: "category:Demo", label: "Demo" },
  { value: "category:Educational", label: "Educational" },
  { value: "category:Utility", label: "Utility" },
  { value: "category:Programming", label: "Programming" },
  { value: "category:Productivity", label: "Productivity" },
  { value: "category:Graphics", label: "Graphics" },
  { value: "category:Diagnostics", label: "Diagnostics" },
  { value: "category:System", label: "System" },
  { value: "category:13 Sector", label: "13 Sector" },
  { value: "category:Personal", label: "Personal" },
  { value: "category:Music Video", label: "Music Video" },
]);

/** Cached category → profile ids (filled lazily). */
let _profileCategoryIndex = null;

function _getProfileCategoryIndex() {
  if (_profileCategoryIndex) return _profileCategoryIndex;
  const all = listProfileIds(rootDir);
  const byCat = new Map();
  for (const id of all) {
    const raw = loadProfileSystemJson(rootDir, id);
    const c = String(raw?.meta?.category || "").trim();
    if (!c) continue;
    if (!byCat.has(c)) byCat.set(c, []);
    byCat.get(c).push(id);
  }
  _profileCategoryIndex = byCat;
  return byCat;
}

/**
 * Human-readable label for a device profile select option.
 * @param {string} id
 */
function _profilePresetLabel(id) {
  const known = {
    "galaxian-demo": "Galaxian (galaxian-demo)",
    "little-brick-out": "Little Brick Out (little-brick-out)",
    "star-blazer": "Star Blazer (star-blazer)",
  };
  if (known[id]) return known[id];
  const raw = loadProfileSystemJson(rootDir, id);
  const title = String(raw?.meta?.title || "").trim();
  if (title && title.toLowerCase() !== id.toLowerCase()) {
    return `${title} (${id})`;
  }
  return id;
}

/**
 * Profile ids in a browse group. Groups stay under µGulp's ~200 select-option cap.
 * @param {string} group
 */
function _profileIdsInGroup(group) {
  const all = listProfileIds(rootDir);
  const g = String(group || "demos").trim();
  if (g === "custom") return [];
  if (g === "demos") {
    return DEMO_PROFILE_IDS.filter((id) => all.includes(id));
  }
  if (g.startsWith("category:")) {
    const cat = g.slice("category:".length);
    const indexed = _getProfileCategoryIndex().get(cat);
    if (indexed) return indexed.slice();
    return all.filter((id) => {
      const raw = loadProfileSystemJson(rootDir, id);
      return String(raw?.meta?.category || "") === cat;
    });
  }
  return all;
}

/**
 * Step 1a — pick a profile group (µGulp select options are capped ~200).
 */
function _DeviceConfigGroupParameters() {
  return {
    title: 'ESP][ device configuration — choose profile group<context="task parameter"/>'.i18xRegister(),
    submitLabel: 'Continue<context="button text"/>'.i18xRegister(),
    fields: [
      {
        id: "group",
        type: "select",
        default: "demos",
        remember: false,
        label: 'Profile group<context="task parameter"/>'.i18xRegister(),
        description:
          'Start with curated demos (fast). Catalog categories list path-only profiles — media stays LOCAL_TEST_ONLY via setup:profiles.<context="task parameter"/>'.i18xRegister(),
        options: [
          {
            value: "custom",
            label: 'Custom (blank / local fields)<context="task parameter"/>'.i18xRegister(),
          },
          {
            value: "demos",
            label: 'Curated demos (Galaxian, Little Brick Out, Star Blazer)<context="task parameter"/>'.i18xRegister(),
          },
          ...DEVICE_CONFIG_CATEGORY_OPTIONS,
        ],
      },
    ],
  };
}

/**
 * Step 1b — pick a preset within the chosen group.
 * @param {string} group
 */
function _DeviceConfigPresetParameters(group) {
  const d = formDefaultsFromConfig(rootDir);
  const ids = _profileIdsInGroup(group);
  const defaultPreset =
    ids.includes(d.preset) ? d.preset : ids.includes("star-blazer") ? "star-blazer" : ids[0] || "custom";
  return {
    title: 'ESP][ device configuration — choose profile<context="task parameter"/>'.i18xRegister(),
    submitLabel: 'Load profile into form<context="button text"/>'.i18xRegister(),
    fields: [
      {
        id: "preset",
        type: "select",
        default: defaultPreset,
        remember: false,
        label: 'Preset profile<context="task parameter"/>'.i18xRegister(),
        description:
          'Selecting a preset fills the next form — nothing is uploaded yet.<context="task parameter"/>'.i18xRegister(),
        options: ids.map((id) => ({ value: id, label: _profilePresetLabel(id) })),
      },
    ],
  };
}

/**
 * Shared gamepad option list for host-bridge step.
 * @param {ReturnType<typeof formDefaultsFromPreset>} d
 * @param {{ gamepadOptions?: { value: string, label: string }[], preferPress?: boolean }} [extra]
 */
function _deviceConfigGamepadDefaults(d, extra = {}) {
  const gamepadOptions = (extra.gamepadOptions || []).map((o) => ({
    value: o.value,
    label:
      o.value === "none"
        ? 'None<context="task parameter"/>'.i18xRegister()
        : o.value === "auto"
          ? 'Auto (first connected)<context="task parameter"/>'.i18xRegister()
          : o.value === "press"
            ? '▶ Press any button on the desired pad…<context="task parameter"/>'.i18xRegister()
            : o.label,
  }));
  if (!gamepadOptions.length) {
    gamepadOptions.push(
      { value: "none", label: 'None<context="task parameter"/>'.i18xRegister() },
      {
        value: "auto",
        label: 'Auto (first connected)<context="task parameter"/>'.i18xRegister(),
      },
      {
        value: "press",
        label: '▶ Press any button on the desired pad…<context="task parameter"/>'.i18xRegister(),
      },
      { value: "0", label: 'XInput #0<context="task parameter"/>'.i18xRegister() },
      { value: "1", label: 'XInput #1<context="task parameter"/>'.i18xRegister() },
      { value: "2", label: 'XInput #2<context="task parameter"/>'.i18xRegister() },
      { value: "3", label: 'XInput #3<context="task parameter"/>'.i18xRegister() },
    );
  }
  const gamepadDefault =
    d.gamepad === "press" || (d.gamepad === "auto" && extra.preferPress)
      ? "press"
      : d.gamepad ?? "none";
  return { gamepadOptions, gamepadDefault };
}

/**
 * Editable device config values (single RequestForm).
 * µGulp panels must always scroll — field count is unknown; do not split
 * forms merely because the host viewport is short.
 * @param {ReturnType<typeof formDefaultsFromPreset>} d
 * @param {{ gamepadOptions?: { value: string, label: string }[], preferPress?: boolean }} [extra]
 */
function _DeviceConfigEditForm(d, extra = {}) {
  const macros = (d.macros || ["none"]).map((id) =>
    id === "none"
      ? { value: "none", label: 'none<context="task parameter"/>'.i18xRegister() }
      : { value: id, label: id },
  );
  const { gamepadOptions, gamepadDefault } = _deviceConfigGamepadDefaults(d, extra);
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
      },
      {
        id: "rom",
        type: "text",
        default: d.rom,
        required: true,
        remember: false,
        label: 'System ROM (device path)<context="task parameter"/>'.i18xRegister(),
      },
      {
        id: "drive1",
        type: "text",
        default: d.drive1,
        remember: false,
        label: 'Drive 1 image (device path)<context="task parameter"/>'.i18xRegister(),
      },
      {
        id: "drive2",
        type: "text",
        default: d.drive2,
        remember: false,
        label: 'Drive 2 image (optional)<context="task parameter"/>'.i18xRegister(),
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
          { value: "artifact", label: 'Artifact Color<context="task parameter"/>'.i18xRegister() },
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
      },
      {
        id: "usbStorageMode",
        type: "select",
        default: d.usbStorageMode ?? "normal",
        remember: false,
        label: 'USB connection behavior<context="task parameter"/>'.i18xRegister(),
        options: [
          {
            value: "normal",
            label: 'Normal — ESP][ keeps SD<context="task parameter"/>'.i18xRegister(),
          },
          {
            value: "auto",
            label: 'Auto-mount SD on computer<context="task parameter"/>'.i18xRegister(),
          },
        ],
      },
      {
        id: "gamepad",
        type: "select",
        default: gamepadDefault,
        remember: false,
        label: 'Host gamepad (Windows bridge)<context="task parameter"/>'.i18xRegister(),
        options: gamepadOptions,
      },
      {
        id: "pdl0",
        type: "select",
        default: d.pdl0 ?? "auto",
        remember: false,
        label: 'PDL0 source (host bridge)<context="task parameter"/>'.i18xRegister(),
        options: [
          { value: "auto", label: 'Auto<context="task parameter"/>'.i18xRegister() },
          {
            value: "gamepadX",
            label: 'Gamepad left stick X<context="task parameter"/>'.i18xRegister(),
          },
          { value: "dial", label: 'Logitech Dial<context="task parameter"/>'.i18xRegister() },
          { value: "none", label: 'None<context="task parameter"/>'.i18xRegister() },
        ],
      },
      {
        id: "pdl1",
        type: "select",
        default: d.pdl1 ?? "auto",
        remember: false,
        label: 'PDL1 source (host bridge)<context="task parameter"/>'.i18xRegister(),
        options: [
          { value: "auto", label: 'Auto<context="task parameter"/>'.i18xRegister() },
          {
            value: "gamepadY",
            label: 'Gamepad left stick Y<context="task parameter"/>'.i18xRegister(),
          },
          { value: "none", label: 'None<context="task parameter"/>'.i18xRegister() },
        ],
      },
      {
        id: "pb0",
        type: "select",
        default: d.pb0 ?? "auto",
        remember: false,
        label: 'PB0 button mapping<context="task parameter"/>'.i18xRegister(),
        options: [
          { value: "auto", label: 'Auto<context="task parameter"/>'.i18xRegister() },
          { value: "gamepadA", label: 'A / Cross button<context="task parameter"/>'.i18xRegister() },
          { value: "none", label: 'None<context="task parameter"/>'.i18xRegister() },
        ],
      },
      {
        id: "dial",
        type: "boolean",
        default: Boolean(d.dial),
        remember: false,
        label: 'Enable Logitech MX Dial (host bridge)<context="task parameter"/>'.i18xRegister(),
      },
      {
        id: "action",
        type: "select",
        default: "save_local",
        remember: false,
        label: 'Action<context="task parameter"/>'.i18xRegister(),
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
        visibleWhen: { action: "apply_device" },
      },
    ],
  };
}

/**
 * Canonical physical ESP][ resolution for µGulp device operations.
 * COM is logged as runtime state only — never used as identity.
 */
async function resolveEsp2DeviceForTask(_title, extra = {}) {
  const {
    resolveEsp2Device,
    formatDeviceStatus,
    hintPortFromEnvAndForm,
  } = await import("./dev/tools/esp2-device-resolver.mjs");
  let formPort = extra.formPort;
  if (formPort === undefined) {
    try {
      formPort = GetParameter("port");
    } catch {
      formPort = "";
    }
  }
  const hint = hintPortFromEnvAndForm({
    formPort,
    envPort: extra.useEnvHint === false ? "" : process.env.ESP2_PORT,
  });
  const requestPick = async (candidates) => {
    const values = await RequestForm({
      title: String(_title).i18xRegister(),
      submitLabel: 'Use this device<context="button text"/>'.i18xRegister(),
      fields: [
        {
          id: "port",
          type: "select",
          label: 'ESP][ device<context="task parameter"/>'.i18xRegister(),
          description:
            'COM is current USB address, not board identity.<context="task parameter"/>'.i18xRegister(),
          options: candidates.map((c) => ({
            value: c.port,
            label: c.label || c.port,
          })),
        },
      ],
    });
    return String(values?.port || "").trim();
  };
  const device = await resolveEsp2Device({
    expectedPlatform: extra.expectedPlatform ?? resolveActivePlatform(),
    hintPort: hint,
    requireIdent: extra.requireIdent !== false,
    allowUnidentifiedPick: extra.allowUnidentifiedPick === true,
    operation: extra.operation || "generic",
    requestPick,
  });
  const st = formatDeviceStatus(device);
  Log('Gerät: <device/>  Port: <port/><context="task log"/>', {
    device: st.device,
    port: st.port,
  });
  return device;
}

async function reResolveEsp2AfterTransition(previous, _title) {
  const { reResolveEsp2DeviceAfterTransition, formatDeviceStatus } = await import(
    "./dev/tools/esp2-device-resolver.mjs"
  );
  const device = await reResolveEsp2DeviceAfterTransition({
    previous,
    expectedPlatform: resolveActivePlatform(),
    requireIdent: true,
    requestPick: async (candidates) => {
      const values = await RequestForm({
        title: String(_title).i18xRegister(),
        submitLabel: 'Use this device<context="button text"/>'.i18xRegister(),
        fields: [
          {
            id: "port",
            type: "select",
            label: 'ESP][ device after USB change<context="task parameter"/>'.i18xRegister(),
            options: candidates.map((c) => ({
              value: c.port,
              label: c.label || c.port,
            })),
          },
        ],
      });
      return String(values?.port || "").trim();
    },
  });
  const st = formatDeviceStatus(device);
  Log('Gerät: <device/>  Port: <port/> (nach USB-Wechsel)<context="task log"/>', {
    device: st.device,
    port: st.port,
  });
  return device;
}

/**
 * Resolve a concrete COMx for device serial tools.
 * Live USB enumeration + `#ESP2IDENT` — not a cached COM number.
 */
async function resolveDeviceSerialPort(_title, extra = {}) {
  const device = await resolveEsp2DeviceForTask(_title, extra);
  return device.port;
}

export async function deviceConfig() {
  ReportProgress(0, "device-config");
  // Always browse via selects: group → profile (µGulp ~200 option cap).
  // Optional CLI/env shortcut: --preset <id> or GetParameter("preset").
  let preset = String(GetParameter("preset") || cliArg("preset") || "").trim();
  if (!preset) {
    const stepGroup = (await RequestForm(_DeviceConfigGroupParameters())) || {};
    const group = String(stepGroup.group || "demos").trim();
    if (group === "custom") {
      preset = "custom";
    } else {
      const ids = _profileIdsInGroup(group);
      if (!ids.length) {
        throw new Error(`No profiles in group=${group}`);
      }
      const stepPreset = (await RequestForm(_DeviceConfigPresetParameters(group))) || {};
      preset = String(stepPreset.preset || ids[0]).trim();
    }
  }
  const defaults = formDefaultsFromPreset(rootDir, preset);
  Log('Loaded profile defaults from <preset/><context="task log"/>', {
    preset: defaults.preset,
  });

  let pads = [];
  try {
    // enrichNames unused; keep list fast (host already returns product names).
    pads = await listXInputGamepads();
  } catch (e) {
    Warn(`Gamepad enumerate failed: ${e?.message || e}`);
  }
  const connectedCount = pads.filter((p) => p.connected).length;
  for (const p of pads) {
    Log('Host gamepad <index/>: <label/><context="task log"/>', {
      index: String(p.index),
      label: p.label || p.name,
    });
  }
  for (const name of pads.pnpNames || []) {
    Log('PnP gamepad-like device: <name/><context="task log"/>', { name });
  }
  for (const r of pads.rawPads || []) {
    Log('RawGameController <index/>: <name/><context="task log"/>', {
      index: String(r.index),
      name: r.name || "(unnamed)",
    });
  }
  if (connectedCount === 0) {
    Warn(
      "No XInput pads awake yet — Bluetooth controllers often show asleep until a button is pressed. 8BitDo must be in X-input mode (often Start+B / Start+X).",
    );
  }

  const editGamepadOptions = buildGamepadSelectOptions(pads, { includePress: true });
  // Only nudge toward press-to-identify when several pads are awake (ambiguous).
  const preferPress = connectedCount >= 2;
  let values;
  if (process.env.ESP2_DEVICE_CONFIG_JSON) {
    values = JSON.parse(process.env.ESP2_DEVICE_CONFIG_JSON);
  } else {
    values =
      (await RequestForm(
        _DeviceConfigEditForm(defaults, {
          gamepadOptions: editGamepadOptions,
          preferPress,
        }),
      )) || {};
  }

  // Resolve press-to-identify after the single edit form (no extra modal).
  if (String(values.gamepad || "") === "press") {
    Log(
      'Press any button NOW on the desired pad (25s). Wake Bluetooth pads first. Dial press is not used here.<context="task log"/>',
    );
    const hit = await waitForXInputButtonPress({ timeoutMs: 25000 });
    if (!hit) {
      throw new Error(
        'No gamepad button press detected — wake the pad, use X-input mode (8BitDo), or pick slot #0/#1/#2/#3 manually.<context="task error"/>',
      );
    }
    try {
      pads = await listXInputGamepads();
    } catch {
      /* keep prior list */
    }
    const pad = pads.find((p) => p.index === hit.index);
    values.gamepad = String(hit.index);
    values.gamepadId =
      (hit.name && !/^XInput#/.test(hit.name) ? hit.name : null) ||
      (pad?.name && !/^XInput#/.test(pad.name) ? String(pad.name) : "");
    await pulseXInputRumble(hit.index, 400);
    Log('Detected gamepad slot <index/> (<label/>, api=<api/>) — rumble pulsed.<context="task log"/>', {
      index: String(hit.index),
      label: pad?.label || hit.name || `XInput#${hit.index}`,
      api: hit.api || "xinput",
    });
  } else {
    const pad = pads.find((p) => String(p.index) === String(values.gamepad));
    if (pad?.name && !/^XInput#/.test(pad.name)) {
      values.gamepadId = String(pad.name);
    }
  }

  const action = String(values.action || "save_local");
  const resolved = configFromFormValues(rootDir, {
    ...values,
    gamepadId: values.gamepadId ?? defaults.gamepadId ?? "",
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
    applyConfigToDevice(rootDir, port, { nodeBin: resolveNodeExecutable() });
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
    'Preset → edit profile (host gamepad: pick slot or press a button to identify) → save. Opening uploads nothing; media bytes are never uploaded here.<context="µDescription"/>',
  µTooltip:
    'Host gamepad is in the edit form. “Press any button…” rumble-identifies the controller after Continue.<context="µTooltip"/>',
  µGroup: 'Device<context="µGroup"/>',
  µOrder: 60,
});

/**
 * Absolute Node binary for child consoles. Under µGulp/Cursor `process.execPath`
 * is the IDE (Electron) — must not be used to run .mjs bridge scripts.
 * @returns {string}
 */
function resolveNodeExecutable() {
  const exe = String(process.execPath || "");
  const norm = exe.replace(/\\/g, "/").toLowerCase();
  if (
    (norm.endsWith("/node.exe") || norm.endsWith("/node")) &&
    existsSync(exe)
  ) {
    return exe;
  }
  for (const key of ["NODE_BINARY", "MICROGULP_NODE", "NODE"]) {
    const v = String(process.env[key] || "").trim();
    if (v && existsSync(v)) return v;
  }
  const winDefault = "C:\\Program Files\\nodejs\\node.exe";
  if (process.platform === "win32" && existsSync(winDefault)) return winDefault;
  try {
    const r = spawnSync(
      process.platform === "win32" ? "where.exe" : "which",
      ["node"],
      { encoding: "utf8", windowsHide: true },
    );
    const first = String(r.stdout || "")
      .split(/\r?\n/)
      .map((s) => s.trim())
      .find(Boolean);
    if (first && existsSync(first)) return first;
  } catch {
    /* PATH fallback */
  }
  return "node";
}

/**
 * Launch the CDC input bridge in its own visible Windows console.
 * µGulp/Electron Job objects kill detached spawn children when the task
 * ends — so the launcher must finish (spawnSync) before we return.
 * WScript.Shell.Run(…, 1) forces a normal visible window out of the Job.
 * @param {string} script absolute path to esp2-input-bridge.mjs
 * @param {string[]} args
 */
function startInputBridgeConsole(script, args) {
  const nodeBin = resolveNodeExecutable();
  if (process.platform === "win32") {
    const microDir = join(rootDir, ".microgulp");
    mkdirSync(microDir, { recursive: true });
    const bat = join(microDir, "run-input-bridge.cmd");
    const winQuote = (s) => `"${String(s).replace(/"/g, "")}"`;
    const argLine = [script, ...args.map(String)]
      .map((a) => (/\s/.test(a) ? winQuote(a) : a))
      .join(" ");
    const batBody = [
      "@echo off",
      "title ESP2 Input Bridge",
      `cd /d ${winQuote(rootDir)}`,
      `${winQuote(nodeBin)} ${argLine}`,
      "echo.",
      "echo Bridge exited — press any key to close this window.",
      "pause >nul",
      "",
    ].join("\r\n");
    writeFileSync(bat, batBody, "utf8");
    Log(
      'Bridge launcher node=<node/> bat=<bat/><context="task log"/>',
      { node: nodeBin, bat },
    );
    // Run style 1 = normal focus; false = don't wait for bridge exit.
    const cmdLine = `cmd /k call ${winQuote(bat)}`;
    const ps = [
      `$w = New-Object -ComObject WScript.Shell`,
      `$code = $w.Run(${psLiteral(cmdLine)}, 1, $false)`,
      `if ($code -lt 0) { throw "WScript.Shell.Run failed: $code" }`,
      `Write-Output "launched=$code"`,
    ].join("; ");
    const r = spawnSync(
      "powershell.exe",
      ["-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", ps],
      { cwd: rootDir, encoding: "utf8", windowsHide: true, timeout: 20000 },
    );
    if (r.status !== 0) {
      throw new Error(
        `Input Bridge console launch failed: ${(r.stderr || r.stdout || r.error || "unknown").toString().trim()}`,
      );
    }
    Log('Bridge console launch ok (<out/>).<context="task log"/>', {
      out: String(r.stdout || "").trim() || "ok",
    });
    return;
  }
  const child = spawn(nodeBin, [script, ...args.map(String)], {
    cwd: rootDir,
    detached: true,
    stdio: "ignore",
  });
  child.unref();
}

/** PowerShell single-quoted string literal. */
function psLiteral(s) {
  return `'${String(s).replace(/'/g, "''")}'`;
}

export async function deviceInputBridge() {
  ReportProgress(0, "device-input-bridge");
  // µGulp may remember an empty profile from a prior run and override the
  // µParameters default — never start without a profile when local config
  // had gamepad:"none".
  let profile = String(GetParameter("profile", "galaxian-demo") ?? "").trim();
  if (!profile) profile = "galaxian-demo";
  const diag = GetParameter("diag") === true || GetParameter("diag") === "true";
  const port = await resolveDeviceSerialPort("Input bridge — serial port");
  const args = ["--port", String(port), "--profile", profile];
  if (diag) args.push("--diag");
  Log(
    'Opening Input Bridge console on <port/> (profile=<profile/>, node=<node/>). Focus that window for keyboard; gamepads need XInput (not D-input).<context="task log"/>',
    { port, profile, node: resolveNodeExecutable() },
  );
  const script = join(rootDir, "dev/tools/esp2-input-bridge.mjs");
  startInputBridgeConsole(script, args);
  Log(
    'Bridge runs in a separate console. Close that window (or device:port:release) to stop.<context="task log"/>',
  );
  ReportProgress(1, "device-input-bridge");
}
_Tag(deviceInputBridge, {
  gulpName: "device:input-bridge",
  µDisplayName: 'Input Bridge V<version/><context="µDisplayName"/>',
  µDescription:
    'Opens a Windows console for the CDC input bridge (keyboard + XInput gamepad + optional Dial). Reads input.hostBridge from the chosen profile or local/device/config.<context="µDescription"/>',
  µTooltip:
    'Set ESP2_PORT only as a session hint (must still match a live ESP][). Pad must be in X-input mode. Default profile galaxian-demo.<context="µTooltip"/>',
  µGroup: 'Device<context="µGroup"/>',
  µOrder: 62,
  µParameters: [
    {
      id: "profile",
      name: "profile",
      type: "string",
      optional: true,
      default: "galaxian-demo",
      µDisplayName: 'Profile id (hostBridge)<context="task parameter"/>',
      µDescription:
        'config/device/profiles/<id>/ — e.g. galaxian-demo or little-brick-out.<context="task parameter"/>',
    },
    {
      id: "diag",
      name: "diag",
      type: "boolean",
      optional: true,
      default: false,
      µDisplayName: 'Diagnostic pad/key log<context="task parameter"/>',
    },
  ],
});

export async function devicePortRelease() {
  ReportProgress(0, "device-port-release");
  const dry =
    GetParameter("dryRun") === true || GetParameter("dryRun") === "true";
  let port = String(GetParameter("port") || "").trim();
  if (!port && !dry) {
    try {
      port = (await AskPort(rootDir, "Release serial port — which COM?")) || "";
    } catch {
      port = "";
    }
  }
  const { releaseComPort } = await import("./dev/tools/esp2-release-com.mjs");
  Log(
    'Releasing COM holders (input-bridge / PIO monitor / upload)… port=<port/><context="task log"/>',
    { port: port || "(any known holder)" },
  );
  const result = await releaseComPort({
    port,
    dryRun: dry,
    log: (s) => Log(String(s)),
  });
  if (result.killed.length) {
    Log('Stopped <count/> holder process(es).<context="task log"/>', {
      count: String(result.killed.length),
    });
  } else {
    Log('No matching holder processes were running.<context="task log"/>');
  }
  if (port && result.probe) {
    if (result.probe.ok) {
      Log('Port <port/> is free.<context="task log"/>', { port });
    } else {
      Warn(`Port ${port} still busy: ${result.probe.reason}`);
      throw new Error(
        `Port ${port} still busy after release — close other serial apps manually.`,
      );
    }
  }
  ReportProgress(1, "device-port-release");
}
_Tag(devicePortRelease, {
  gulpName: "device:port:release",
  µDisplayName: 'Release COM Port V<version/><context="µDisplayName"/>',
  µDescription:
    'Stops ESP][ host processes that lock the USB CDC port (input bridge, PlatformIO monitor, upload helpers) and probes that the port opens again.<context="µDescription"/>',
  µTooltip:
    'Use when flash/monitor/input-bridge says Access denied. Pick the current COM of the stuck session — numbers change after replug.<context="µTooltip"/>',
  µGroup: 'Tools<context="µGroup"/>',
  µOrder: 25,
  µVisible: () => true,
  µParameters: [
    {
      name: "port",
      type: "string",
      optional: true,
      default: "",
      µDisplayName: 'Serial port (optional)<context="task parameter"/>',
      µDescription:
        'e.g. COM5 if you already know the stuck session. Empty → pick from currently enumerated ports.<context="task parameter"/>',
    },
    {
      name: "dryRun",
      type: "boolean",
      optional: true,
      default: false,
      µDisplayName: 'Dry run (list only)<context="task parameter"/>',
    },
  ],
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
    'Execute a named input macro on a live ESP][ (#ESP2MACRO RUN). Device is resolved via USB + identity.<context="µDescription"/>',
  µTooltip:
    'Requires a connected ESP][ that answers #ESP2IDENT. COM is not stored as identity.<context="µTooltip"/>',
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

function _DeviceBleDisplayName() {
  return 'Set up BLE devices V<version/><context="µDisplayName"/>'.i18xRegister();
}

export async function deviceBle() {
  ReportProgress(0, "device-ble");
  let discovered = [];
  let known = [];
  const control = defaultControlBindings();
  const resolved = await resolveEsp2DeviceForTask("ESP][ BLE setup — device", {
    operation: "ble",
  });
  let defaultsPort = resolved.port;

  // First form may be scan-only; after scan we re-open with discovered list.
  for (let round = 0; round < 4; ++round) {
    const portOptions = [{ value: defaultsPort, label: `ESP][ · ${defaultsPort}` }];

    const form = buildBleSetupForm({
      port: defaultsPort,
      portOptions,
      discovered,
      known,
      control,
    });
    // RequestForm expects i18x-registered strings on title — register here.
    form.title = String(form.title).i18xRegister();
    form.submitLabel = String(form.submitLabel).i18xRegister();
    for (const f of form.fields) {
      if (f.label) f.label = String(f.label).i18xRegister();
      if (f.description) f.description = String(f.description).i18xRegister();
      if (Array.isArray(f.options)) {
        f.options = f.options.map((o) => ({
          ...o,
          label: String(o.label).i18xRegister(),
        }));
      }
    }
    const values = await RequestForm(form);
    const action = String(values.action || "scan");
    const port = String(values.port || "").trim();
    if (!port) {
      throw new Error(
        'Select an ESP][ COM port from the list.<context="task error"/>'.i18xRegister(),
      );
    }
    defaultsPort = port;

    const session = await openBleSession(port);
    try {
      Log('Probing ESP][ on <port/>…<context="task log"/>', { port });
      await session.init();
      Log('BLE host ready on <port/>.<context="task log"/>', { port });
      if (action === "scan") {
        const sec = Math.max(3, Math.min(30, Number(values.scanSeconds) || 8));
        Log('Scanning BLE for <sec/> s…<context="task log"/>', { sec });
        const r = await session.scan(sec);
        discovered = r.devices || [];
        Log('Scan done: <count/> device(s).<context="task log"/>', {
          count: discovered.length,
        });
        for (const d of discovered) {
          Log('  · <name/> (<type/>) rssi=<rssi/> addr=<addr/><context="task log"/>', {
            name: d.name,
            type: d.type,
            rssi: d.rssi,
            addr: d.address,
          });
        }
        const k = await session.listKnown();
        known = k.devices || [];
        // Loop again so user can pair from populated list.
        continue;
      }
      if (action === "pair") {
        const addr = String(values.discoveredAddress || "").trim();
        if (!addr) throw new Error("Select a discovered device to pair");
        Log('Pairing <addr/> …<context="task log"/>', { addr });
        const line = await session.pair(addr);
        Log('<line/><context="task log"/>', { line });
        const k = await session.listKnown();
        known = k.devices || [];
        break;
      }
      if (action === "connect") {
        const addr = String(values.knownAddress || "").trim();
        if (!addr) throw new Error("Select a known device to connect");
        Log('Connecting <addr/> …<context="task log"/>', { addr });
        const line = await session.connect(addr);
        Log('<line/><context="task log"/>', { line });
        break;
      }
      if (action === "disconnect") {
        const line = await session.disconnect();
        Log('<line/><context="task log"/>', { line });
        break;
      }
      if (action === "forget") {
        const addr = String(values.knownAddress || "").trim();
        if (!addr) throw new Error("Select a known device to forget");
        Log('Forgetting <addr/> …<context="task log"/>', { addr });
        const line = await session.forget(addr);
        Log('<line/><context="task log"/>', { line });
        break;
      }
      if (action === "status") {
        const st = await session.status();
        const bd = await session.bindings();
        Log('<st/><context="task log"/>', { st });
        Log('<bd/><context="task log"/>', { bd });
        Log(
          'Defaults: Reset <rk/> / <rg/>; Settings <sk/> / <sg/><context="task log"/>',
          {
            rk: formatKeyboardBinding(control.resetAppleIi.keyboard),
            rg: formatGamepadChord(control.resetAppleIi.gamepad),
            sk: formatKeyboardBinding(control.openSettings.keyboard),
            sg: formatGamepadChord(control.openSettings.gamepad),
          },
        );
        break;
      }
      if (action === "save_hotkeys") {
        // V1: local validate + instruct — firmware rewrite of control section via
        // bluetooth.json upload path is preferred through device:config apply later.
        const kbR = parseKeyboardBindingText(values.resetKeyboard);
        const kbS = parseKeyboardBindingText(values.settingsKeyboard);
        const hold = Number(values.holdMs) || 1500;
        const gpR = parseGamepadChordText(values.resetGamepad, hold);
        const gpS = parseGamepadChordText(values.settingsGamepad, hold);
        if (!kbR || !kbS || !gpR || !gpS) {
          throw new Error("Invalid hotkey text — use Ctrl+Alt+R / start+select (hold 1.5 s)");
        }
        Log(
          'Parsed bindings OK. Persist via /esp2/config/bluetooth.json on next pair/save (firmware). Binding capture UI is future work.<context="task log"/>',
        );
        Log('Reset KB=<kb/> GP=<gp/><context="task log"/>', {
          kb: formatKeyboardBinding(kbR),
          gp: formatGamepadChord(gpR),
        });
        Log('Settings KB=<kb/> GP=<gp/><context="task log"/>', {
          kb: formatKeyboardBinding(kbS),
          gp: formatGamepadChord(gpS),
        });
        break;
      }
      throw new Error(`Unknown BLE action: ${action}`);
    } finally {
      await session.close();
    }
  }
  ReportProgress(1, "device-ble");
}
_Tag(deviceBle, {
  gulpName: "device:ble",
  µDisplayName: () => _DeviceBleDisplayName(),
  µDescription:
    'USB µGulp workflow: BLE scan / pair / reconnect / forget + hotkey labels. Uses the live ESP][ (identity handshake), not a remembered COM port.<context="µDescription"/>',
  µTooltip:
    'Do not enter USB Storage during BLE setup. Device is resolved from Target + USB + #ESP2IDENT.<context="µTooltip"/>',
  µGroup: 'Device<context="µGroup"/>',
  µOrder: 62,
});
gulp.task("device:ble", deviceBle);

export async function deviceUpload() {
  ReportProgress(0, "device-upload");
  const hinted = String(GetParameter("port") || "").trim();
  const device = await resolveEsp2DeviceForTask("Transfer file — ESP][ device", {
    formPort: hinted,
    operation: "media",
  });
  const port = device.port;
  const file = GetParameter("file") || "";
  const target = GetParameter("target") || "";
  const verify = GetParameter("verify") || "";
  const args = ["--port", port];
  if (verify) {
    args.push("--verify", verify);
  } else {
    if (!file || !target) {
      throw new Error("device:upload requires --file and --target (or --verify)");
    }
    args.push("--file", file, "--target", target);
  }
  await runNodeCliWithProgress("dev/tools/esp2-upload.mjs", args, "device:upload", {
    progressLo: 0.05,
    progressHi: 0.98,
    progressId: "device-upload",
  });
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
    { name: "port", type: "string", optional: true },
    { name: "file", type: "string", optional: true },
    { name: "target", type: "string", optional: true },
    { name: "verify", type: "string", optional: true },
  ],
});

/** CLI-only (not dashboard): prefer device:sd:computer. Kept for scripts. */
async function deviceUsbStorage() {
  ReportProgress(0, "device-usb-storage");
  const hinted = String(GetParameter("port") || "").trim();
  const device = await resolveEsp2DeviceForTask("USB storage — ESP][ device", {
    formPort: hinted,
    operation: "sd-ownership",
  });
  const port = device.port;
  const leave = GetParameter("leave") === true || GetParameter("leave") === "true";
  const args = leave
    ? ["--port", port, "--leave-usb-storage"]
    : ["--port", port, "--enter-usb-storage"];
  runNodeCli("dev/tools/esp2-upload.mjs", args, "device:usb-storage");
  ReportProgress(1, "device-usb-storage");
}
gulp.task("device:usb-storage", deviceUsbStorage);

export async function deviceSdComputer() {
  ReportProgress(0, "device-sd-computer");
  const { buildSdComputerForm, registerFormI18x } = await loadSdUx();
  const { openHostSdSession, runUsbStorageCommand } = await import(
    "./dev/tools/esp2-sd-ownership-session.mjs"
  );
  const values =
    (await RequestForm(
      registerFormI18x(buildSdComputerForm({ defaultPort: "" }), (s) => s.i18xRegister()),
    )) || {};
  const action = String(values.action || "make_available").trim();
  const device = await resolveEsp2DeviceForTask("ESP][ SD card — device", {
    formPort: values.port,
    operation: "sd-ownership",
  });
  const port = device.port;
  const openExplorer =
    values.openExplorer === true ||
    values.openExplorer === "true" ||
    values.openExplorer === undefined;

  if (action === "return_esp") {
    Log('Returning SD to ESP][…<context="task log"/>');
    Log(
      'If the SD is still visible in Explorer/Finder, eject it first.<context="task log"/>',
    );
    const next = await reResolveEsp2AfterTransition(
      device,
      "ESP][ after USB Storage — return SD",
    );
    runUsbStorageCommand(rootDir, next.port, "leave");
    Log('SD returned to ESP][.<context="task log"/>');
    ReportProgress(1, "device-sd-computer");
    return;
  }

  const session = await openHostSdSession({
    mode: "browse",
    port,
    projectRoot: rootDir,
    openExplorer: Boolean(openExplorer),
    resolveLeavePort: async () =>
      (await reResolveEsp2AfterTransition(device, "ESP][ after USB Storage")).port,
    log: (msg, vars) => Log(msg, vars),
    requestAmbiguousPick: async (candidates) => {
      const pick = await RequestForm({
        title: 'Select ESP][ SD volume<context="task parameter"/>'.i18xRegister(),
        submitLabel: 'Use this volume<context="button text"/>'.i18xRegister(),
        fields: [
          {
            id: "root",
            type: "select",
            required: true,
            label: 'Detected volumes<context="task parameter"/>'.i18xRegister(),
            options: candidates.map((c) => ({
              value: c.root,
              label: `${c.root} · ${c.filesystem || "?"} · ${c.label || ""}`,
            })),
          },
        ],
      });
      return pick?.root || null;
    },
  });

  Log(
    'SD is on the computer at <path/>. Disk II/runtime SD access is unavailable until returned.<context="task log"/>',
    { path: session.mountRoot },
  );
  Log(
    'When finished: eject in Explorer/Finder, then run this task with “Return to ESP][”.<context="task log"/>',
  );
  // Keep MSC active for manual browsing — do not auto-leave.
  ReportProgress(1, "device-sd-computer");
}
_Tag(deviceSdComputer, {
  gulpName: "device:sd:computer",
  µDisplayName: 'SD card on computer V<version/><context="µDisplayName"/>',
  µDescription:
    'Make the ESP][ microSD available to Windows/macOS (USB Storage), or return it to ESP][. While on the computer, Apple II Disk II cannot use the SD.<context="µDescription"/>',
  µTooltip:
    'Uses the same USB Storage ownership path as SD backup/restore. Requires TinyUSB MSC on device (USB_MODE=0). Resolves ESP][ by USB + #ESP2IDENT, never a stale COM.<context="µTooltip"/>',
  µGroup: 'Device<context="µGroup"/>',
  µOrder: 71,
});

export async function deviceSdInfo() {
  ReportProgress(0, "device-sd-info");
  const { buildSdInfoForm, registerFormI18x } = await loadSdUx();
  const { parseSdInfoAck } = await import("./dev/tools/esp2-device-resolver.mjs");
  const values =
    (await RequestForm(
      registerFormI18x(buildSdInfoForm({ defaultPort: "" }), (s) => s.i18xRegister()),
    )) || {};
  const device = await resolveEsp2DeviceForTask("ESP][ SD information — device", {
    formPort: values.port,
    operation: "sd-info",
  });
  const r = spawnSync(
    resolveNodeExecutable(),
    [
      join(rootDir, "dev/tools/esp2-upload.mjs"),
      "--port",
      device.port,
      "--sd-info",
    ],
    { encoding: "utf8", windowsHide: true, cwd: rootDir },
  );
  const out = `${r.stdout || ""}\n${r.stderr || ""}`;
  if (r.status !== 0) {
    throw new Error(out.trim() || "SDINFO failed");
  }
  let info = null;
  for (const line of out.split(/\r?\n/)) {
    info = parseSdInfoAck(line);
    if (info) break;
  }
  if (!info) {
    throw new Error("No #ACK SDINFO from device");
  }
  const present =
    info.present === "1" ? "Present" : info.present === "0" ? "Missing" : "Unknown";
  const fs =
    info.fs === "fat" ? "FAT" : info.fs === "none" ? "None" : info.fs || "Unknown";
  const structure = info.esp2 === "1" ? "Present" : "Missing";
  Log('Card: <v/><context="task log"/>', { v: present });
  Log('Capacity: <v/> MB<context="task log"/>', { v: info.sizeMb || "unknown" });
  Log('Filesystem: <v/><context="task log"/>', { v: fs });
  Log('ESP][ structure (/esp2): <v/><context="task log"/>', { v: structure });
  Log('Owner: <v/><context="task log"/>', { v: info.owner || "?" });
  ReportProgress(1, "device-sd-info");
}
_Tag(deviceSdInfo, {
  gulpName: "device:sd:info",
  µDisplayName: 'SD card information V<version/><context="µDisplayName"/>',
  µDescription:
    'Read-only query of the SD in the live ESP][ (presence, capacity if mounted, FAT, /esp2). Does not format or enter USB Storage.<context="µDescription"/>',
  µGroup: 'Device<context="µGroup"/>',
  µOrder: 70,
});

/** Lazy import cache for SD backup/restore UX helpers. */
let _sdUxMod = null;
async function loadSdUx() {
  if (!_sdUxMod) {
    _sdUxMod = await import("./dev/tools/esp2-sd-backup-ux.mjs");
  }
  return _sdUxMod;
}

export async function deviceSdBackup() {
  ReportProgress(0, "device-sd-backup");
  const {
    buildSdBackupForm,
    registerFormI18x,
    resolveSdMountViaUx,
    SD_UX_ACCESS_DEVICE,
  } = await loadSdUx();
  const defaultRoot = join(rootDir, "local/sd-backups");
  const values =
    (await RequestForm(
      registerFormI18x(
        buildSdBackupForm({ defaultBackupRoot: defaultRoot, defaultPort: "" }),
        (s) => s.i18xRegister(),
      ),
    )) || {};
  // Opening the form must not enter MSC — device work starts only here.
  const destRoot = String(values.destRoot || defaultRoot).trim();
  const accessMode = String(values.accessMode || SD_UX_ACCESS_DEVICE).trim();
  let port = "";
  let device = null;
  if (accessMode === SD_UX_ACCESS_DEVICE) {
    device = await resolveEsp2DeviceForTask("ESP][ SD backup — device", {
      formPort: values.port,
      operation: "sd-ownership",
    });
    port = device.port;
  }

  const session = await resolveSdMountViaUx({
    mode: "backup",
    accessMode,
    advancedPath: String(values.advancedSource || "").trim(),
    port,
    projectRoot: rootDir,
    resolveLeavePort: device
      ? async () =>
          (await reResolveEsp2AfterTransition(device, "ESP][ after USB Storage")).port
      : undefined,
    log: (msg, vars) => Log(msg, vars),
    requestAmbiguousPick: async (candidates) => {
      const pick = await RequestForm({
        title: 'Select ESP][ SD volume<context="task parameter"/>'.i18xRegister(),
        submitLabel: 'Use this volume<context="button text"/>'.i18xRegister(),
        fields: [
          {
            id: "root",
            type: "select",
            required: true,
            label: 'Detected volumes<context="task parameter"/>'.i18xRegister(),
            options: candidates.map((c) => ({
              value: c.root,
              label: `${c.root} · ${c.filesystem || "?"} · ${c.label || ""}`,
            })),
          },
        ],
      });
      return pick?.root || null;
    },
  });

  try {
    const { backupEsp2Sd, probeVolumeInfo, resolveEsp2Root } = await import(
      "./dev/tools/esp2-sd-backup.mjs"
    );
    const resolved = resolveEsp2Root(session.mountRoot);
    const vol = probeVolumeInfo(resolved.mountRoot);
    Log(
      'SD backup (logical /esp2 tree, not raw image). source=<source/> fs=<fs/> capacity=<cap/><context="task log"/>',
      {
        source: resolved.mountRoot,
        fs: vol.filesystem || "unknown",
        cap: vol.capacityBytes != null ? String(vol.capacityBytes) : "unknown",
      },
    );
    Log('Copying…<context="task log"/>');
    const t0 = Date.now();
    const result = await backupEsp2Sd(rootDir, {
      sourcePath: session.mountRoot,
      destRoot,
    });
    const ms = Date.now() - t0;
    const mbs =
      result.manifest.totals.bytes > 0
        ? (result.manifest.totals.bytes / (1024 * 1024) / (ms / 1000)).toFixed(2)
        : "0";
    Log('Verifying…<context="task log"/>');
    Log(
      'SD backup PASS → <path/> files=<files/> bytes=<bytes/> verified SHA-256 (~<mbs/> MB/s)<context="task log"/>',
      {
        path: result.backupDir,
        files: String(result.manifest.totals.files),
        bytes: String(result.manifest.totals.bytes),
        mbs,
      },
    );
    Log('Finished.<context="task log"/>');
  } finally {
    await session.release();
  }
  ReportProgress(1, "device-sd-backup");
}
_Tag(deviceSdBackup, {
  gulpName: "device:sd:backup",
  µDisplayName: 'Back up SD card V<version/><context="µDisplayName"/>',
  µDescription:
    'Read-only logical backup of the SD currently in the connected ESP][ (USB Storage → hash-verified copy under local/sd-backups). Not a raw card image.<context="µDescription"/>',
  µTooltip:
    'Select the live ESP][ (not a remembered COM). Destination defaults to local/sd-backups/. Advanced card-reader path available if MSC auto-detect fails.<context="µTooltip"/>',
  µGroup: 'Device<context="µGroup"/>',
  µOrder: 72,
});

export async function deviceSdRestore() {
  ReportProgress(0, "device-sd-restore");
  const {
    buildSdRestoreForm,
    buildSdRestoreConfirmForm,
    registerFormI18x,
    resolveSdMountViaUx,
    resolveBackupDirFromForm,
    SD_UX_ACCESS_DEVICE,
  } = await loadSdUx();
  const {
    listSdBackups,
    restoreEsp2Sd,
    loadAndValidateManifest,
    probeVolumeInfo,
    resolveEsp2Root,
    defaultSdBackupRoot,
  } = await import("./dev/tools/esp2-sd-backup.mjs");

  const defaultRoot = defaultSdBackupRoot(rootDir);
  const backups = listSdBackups(defaultRoot);
  const values =
    (await RequestForm(
      registerFormI18x(
        buildSdRestoreForm({
          defaultBackupRoot: defaultRoot,
          defaultPort: "",
          backups,
        }),
        (s) => s.i18xRegister(),
      ),
    )) || {};

  const backupDir = resolveBackupDirFromForm(values, defaultRoot);
  const dryRun = values.dryRun === true || values.dryRun === "true";
  const accessMode = String(values.accessMode || SD_UX_ACCESS_DEVICE).trim();
  if (!backupDir) {
    throw new Error('Select a backup.<context="task error"/>');
  }
  const { manifest } = loadAndValidateManifest(backupDir);

  let port = "";
  let device = null;
  if (accessMode === SD_UX_ACCESS_DEVICE) {
    device = await resolveEsp2DeviceForTask("ESP][ SD restore — device", {
      formPort: values.port,
      operation: "sd-ownership",
    });
    port = device.port;
  }

  const session = await resolveSdMountViaUx({
    mode: "restore",
    accessMode,
    advancedPath: String(values.advancedDest || "").trim(),
    port,
    projectRoot: rootDir,
    resolveLeavePort: device
      ? async () =>
          (await reResolveEsp2AfterTransition(device, "ESP][ after USB Storage")).port
      : undefined,
    log: (msg, vars) => Log(msg, vars),
    requestAmbiguousPick: async (candidates) => {
      const pick = await RequestForm({
        title: 'Select destination SD volume<context="task parameter"/>'.i18xRegister(),
        submitLabel: 'Use this volume<context="button text"/>'.i18xRegister(),
        fields: [
          {
            id: "root",
            type: "select",
            required: true,
            label: 'Detected volumes<context="task parameter"/>'.i18xRegister(),
            options: candidates.map((c) => ({
              value: c.root,
              label: `${c.root} · ${c.filesystem || "?"} · ${c.label || ""}`,
            })),
          },
        ],
      });
      return pick?.root || null;
    },
  });

  try {
    const destResolved = resolveEsp2Root(session.mountRoot, { allowMissingEsp2: true });
    const destMount = destResolved.mountRoot;
    const destEsp2 = destResolved.esp2Root;
    const vol = probeVolumeInfo(destMount);
    const summary =
      session.summary ||
      {
        text: [
          `path=${destMount}`,
          `fs=${vol.filesystem || "?"}`,
          `capacity=${vol.capacityBytes ?? "?"}`,
          `free=${vol.freeBytes ?? "?"}`,
        ].join(" · "),
      };

    Log(
      'RESTORE plan backup=<backup/> → dest=<dest/> files=<files/> bytes=<bytes/> info=<info/><context="task log"/>',
      {
        backup: backupDir,
        dest: destMount,
        files: String(manifest.totals?.files ?? 0),
        bytes: String(manifest.totals?.bytes ?? 0),
        info: summary.text,
      },
    );

    if (!dryRun) {
      const conf =
        (await RequestForm(
          registerFormI18x(
            buildSdRestoreConfirmForm({
              summaryText: summary.text,
              backupLabel: backupDir,
            }),
            (s) => s.i18xRegister(),
          ),
        )) || {};
      const confirm = conf.confirm === true || conf.confirm === "true";
      if (!confirm) {
        throw new Error(
          'Confirmation required after reviewing the detected destination SD.<context="task error"/>',
        );
      }
    }

    Log(dryRun ? 'Dry run…<context="task log"/>' : 'Copying…<context="task log"/>');
    const t0 = Date.now();
    const result = await restoreEsp2Sd(rootDir, {
      backupDir,
      destPath: destMount,
      dryRun,
    });
    const ms = Date.now() - t0;
    if (dryRun) {
      Log(
        'Dry run only — no writes. Would restore <files/> files (<bytes/> bytes); stale remove=<stale/>.<context="task log"/>',
        {
          files: String(result.filesToWrite),
          bytes: String(result.bytes),
          stale: String(result.staleToRemove?.length ?? 0),
        },
      );
    } else {
      Log('Verifying…<context="task log"/>');
      const mbs =
        result.bytesRestored > 0
          ? (result.bytesRestored / (1024 * 1024) / (ms / 1000)).toFixed(2)
          : "0";
      Log(
        'SD restore PASS → <dest/> files=<files/> bytes=<bytes/> SHA-256 verified (~<mbs/> MB/s)<context="task log"/>',
        {
          dest: destEsp2,
          files: String(result.filesRestored),
          bytes: String(result.bytesRestored),
          mbs,
        },
      );
    }
    Log('Finished.<context="task log"/>');
  } finally {
    await session.release();
  }
  ReportProgress(1, "device-sd-restore");
}
_Tag(deviceSdRestore, {
  gulpName: "device:sd:restore",
  µDisplayName: 'Restore SD card V<version/><context="µDisplayName"/>',
  µDescription:
    'Restores a verified backup onto the SD currently in the connected ESP][ (USB Storage). Card sizes may differ if data fits. Requires confirmation after the destination is detected.<context="µDescription"/>',
  µTooltip:
    'Pick a backup under local/sd-backups/. Destination mount is auto-detected via USB Storage — never guessed by card size alone.<context="µTooltip"/>',
  µGroup: 'Device<context="µGroup"/>',
  µOrder: 73,
});

gulp.task("device:config", deviceConfig);
gulp.task("device:port:release", devicePortRelease);
gulp.task("device:input-bridge", deviceInputBridge);
gulp.task("device:macro:run", deviceMacroRun);
gulp.task("device:upload", deviceUpload);
gulp.task("device:sd:computer", deviceSdComputer);
gulp.task("device:sd:info", deviceSdInfo);
gulp.task("device:sd:backup", deviceSdBackup);
gulp.task("device:sd:restore", deviceSdRestore);

//================================================================
// diymore Hardware — bring-up only (platform: diymore-es3c28p)
//================================================================

export async function diymoreIdentify() {
  ReportProgress(0, "diymore-identify");
  const platformId = resolveActivePlatform();
  if (platformId !== PLATFORM_DIYMORE) {
    throw new Error(
      `diymore identify requires Project Control Target = diymore-es3c28p (got ${platformId})`,
    );
  }
  const env = firmwareEnv();
  const device = await resolveEsp2DeviceForTask("diymore flash — device", {
    expectedPlatform: PLATFORM_DIYMORE,
    operation: "flash",
    allowUnidentifiedPick: true,
  });
  const port = device.port;
  Log(
    'diymore identity: build+upload env=<env/> → <port/><context="task log"/>…',
    { env, port },
  );
  await Pio(rootDir, ["run", "-e", env], "pio build");
  ReportProgress(0.45, "diymore-identify-upload");
  await Pio(rootDir, UploadArgs(port, env), "pio upload", {
    onProgress: ({ fraction }) => {
      ReportProgress(0.45 + 0.5 * fraction, "diymore-identify-upload");
    },
  });
  ReportProgress(0.95, "diymore-identify");
  Log(
    'Flash done. Open Serial Monitor – look for [CHIP]/[FLASH]/[PSRAM]/[SELFTEST]. Next: Display Probe.<context="task log"/>',
  );
  ReportProgress(1, "diymore-identify");
  PlaySignal("success");
}
_Tag(diymoreIdentify, {
  gulpName: "diymore:identify",
  µDisplayName:
    'Identify Chip / Flash / PSRAM V<version/><context="µDisplayName"/>',
  µDescription:
    'Build and flash diymore_bringup identity probe. Reports chip/flash/PSRAM over serial. No display/touch init.<context="µDescription"/>',
  µTooltip:
    'diymore ES3C28P · AM03-FBBA0125-AAA — first bring-up step.<context="µTooltip"/>',
  µIcon: "\u26A1",
  µGroup: 'diymore Hardware<context="µGroup"/>',
  µOrder: 10,
  µExecutionConcurrency: false,
  µExecutionRestrictions: { deny: ["flash", "upload", "build"] },
});
gulp.task("diymore:identify", diymoreIdentify);

export async function diymoreDocs() {
  ReportProgress(0, "diymore-docs");
  Log('Platform ID: diymore-es3c28p<context="task log"/>');
  Log('Part number: AM03-FBBA0125-AAA (metadata only)<context="task log"/>');
  Log('Docs: docs/hardware/diymore-es3c28p.md + docs/hardware/targets.md<context="task log"/>');
  Log(
    'UNKNOWN until probed: display IC, touch IC, pins, flash size, PSRAM, SD, audio.<context="task log"/>',
  );
  ReportProgress(1, "diymore-docs");
  PlaySignal("success");
}
_Tag(diymoreDocs, {
  gulpName: "diymore:docs",
  µDisplayName: 'diymore Board Notes V<version/><context="µDisplayName"/>',
  µDescription:
    'Prints known vs unknown hardware facts for the diymore ES3C28P board. No flash.<context="µDescription"/>',
  µGroup: 'diymore Hardware<context="µGroup"/>',
  µOrder: 5,
  µExecutionConcurrency: true,
});
gulp.task("diymore:docs", diymoreDocs);

/** LCD color-bar probe using LCDWIKI ES3C28P pin map (needs visual confirm). */
export async function diymoreDisplayProbe() {
  ReportProgress(0, "diymore-display");
  const platformId = resolveActivePlatform();
  if (platformId !== PLATFORM_DIYMORE) {
    throw new Error(
      `diymore display probe requires Project Control Target = diymore-es3c28p (got ${platformId})`,
    );
  }
  const env = "diymore_display_probe";
  const device = await resolveEsp2DeviceForTask("diymore display probe — device", {
    expectedPlatform: PLATFORM_DIYMORE,
    operation: "flash",
    allowUnidentifiedPick: true,
  });
  const port = device.port;
  Log(
    'diymore display probe: build+upload env=<env/> → <port/><context="task log"/>…',
    { env, port },
  );
  Log(
    'Pins from LCDWIKI ES3C28P (ILI9341V SPI). Confirm color bars / TOP BOTTOM LEFT RIGHT on the panel.<context="task log"/>',
  );
  await Pio(rootDir, ["run", "-e", env], "pio build");
  ReportProgress(0.45, "diymore-display-upload");
  await Pio(rootDir, UploadArgs(port, env), "pio upload", {
    onProgress: ({ fraction }) => {
      ReportProgress(0.45 + 0.5 * fraction, "diymore-display-upload");
    },
  });
  ReportProgress(0.95, "diymore-display");
  Log(
    'Flash done. Serial: [DISPLAY] / [SELFTEST]. Visual PASS only if you see labeled color bars.<context="task log"/>',
  );
  ReportProgress(1, "diymore-display");
  PlaySignal("success");
}
_Tag(diymoreDisplayProbe, {
  gulpName: "diymore:display",
  µDisplayName: 'Display Probe (ILI9341 color bars) V<version/><context="µDisplayName"/>',
  µDescription:
    'Build/flash diymore_display_probe: ILI9341V SPI color bars from LCDWIKI ES3C28P pin map. Requires your visual confirmation — begin() OK is not enough.<context="µDescription"/>',
  µTooltip:
    'Vendor pins: CS10 DC46 SCK12 MOSI11 MISO13 BL45. Not Apple II / Galaxian.<context="µTooltip"/>',
  µIcon: "\u25A3",
  µGroup: 'diymore Hardware<context="µGroup"/>',
  µOrder: 20,
  µExecutionConcurrency: false,
  µExecutionRestrictions: { deny: ["flash", "upload", "build", "diymore:identify"] },
});
gulp.task("diymore:display", diymoreDisplayProbe);

/** Placeholder until touch IC + pins are verified — stays disabled. */
export async function diymoreTouchTodo() {
  throw new Error(
    "diymore touch test is not implemented — touch controller/pins UNKNOWN",
  );
}
_Tag(diymoreTouchTodo, {
  gulpName: "diymore:touch",
  µDisplayName: 'Touch Test (TODO) V<version/><context="µDisplayName"/>',
  µDescription:
    'Reserved. Enable only after the touch controller and pins are verified.<context="µDescription"/>',
  µGroup: 'diymore Hardware<context="µGroup"/>',
  µOrder: 30,
  µEnabled: false,
  µDisabledTooltip:
    'Blocked: touch controller and pins UNKNOWN — see docs/hardware/diymore-es3c28p.md.<context="µDisabledTooltip"/>',
});
gulp.task("diymore:touch", diymoreTouchTodo);

/**
 * Demonstrates gulp-mu-build-filter platform against board header stubs.
 * Does not rewrite the working tree — dry log of filtered include/boards/*.h.
 */
export async function sourcesBuildFilterDemo() {
  ReportProgress(0, "build-filter");
  const { createRequire } = await import("node:module");
  const require = createRequire(import.meta.url);
  let muBuildFilter;
  try {
    muBuildFilter = require("gulp-mu-build-filter");
  } catch {
    throw new Error(
      "gulp-mu-build-filter is not installed — run npm install gulp-mu-build-filter",
    );
  }
  const { readFileSync: readText } = await import("node:fs");
  const platformId = resolveActivePlatform();
  const opts = buildFilterPlatformOptions(platformId);
  Log(
    'gulp-mu-build-filter platform=<platform/> mode=<mode/><context="task log"/>',
    { platform: opts.platform, mode: opts.mode },
  );
  for (const rel of [
    "include/boards/waveshare_amoled_s3.h",
    "include/boards/diymore_es3c28p.h",
  ]) {
    const raw = readText(join(rootDir, rel), "utf8");
    const out = muBuildFilter({ text: raw, ...opts });
    const keptWaveshare = /ESP2_PLATFORM_TAG_WAVESHARE/.test(out);
    const keptDiymore = /ESP2_PLATFORM_TAG_DIYMORE/.test(out);
    Log(
      '<file/> waveshareTag=<w/> diymoreTag=<d/><context="task log"/>',
      {
        file: rel,
        w: String(keptWaveshare),
        d: String(keptDiymore),
      },
    );
  }
  ReportProgress(1, "build-filter");
  PlaySignal("success");
}
_Tag(sourcesBuildFilterDemo, {
  gulpName: "sources:build-filter",
  µDisplayName:
    'Build-Filter Demo – <platformLabel/> V<version/><context="µDisplayName"/>',
  µDescription:
    'Runs gulp-mu-build-filter with the active Project Control platform against board header stubs (read-only demo).<context="µDescription"/>',
  µGroup: 'Tools<context="µGroup"/>',
  µOrder: 90,
  µExecutionConcurrency: true,
});
gulp.task("sources:build-filter", sourcesBuildFilterDemo);


//================================================================
// RLCD 4.2 Hardware — bring-up (platform: waveshare-rlcd42)
//================================================================

export async function rlcd42Identify() {
  ReportProgress(0, "rlcd42-identify");
  const platformId = resolveActivePlatform();
  if (platformId !== PLATFORM_RLCD42) {
    throw new Error(
      `rlcd42 identify requires Project Control Target = waveshare-rlcd42 (got ${platformId})`,
    );
  }
  const env = "rlcd42_bringup";
  const device = await resolveEsp2DeviceForTask("rlcd42 flash — device", {
    expectedPlatform: PLATFORM_RLCD42,
    operation: "flash",
    allowUnidentifiedPick: true,
  });
  const port = device.port;
  Log(
    'rlcd42 identity: build+upload env=<env/> → <port/><context="task log"/>…',
    { env, port },
  );
  await Pio(rootDir, ["run", "-e", env], "pio build");
  ReportProgress(0.45, "rlcd42-identify-upload");
  await Pio(rootDir, UploadArgs(port, env), "pio upload", {
    onProgress: ({ fraction }) => {
      ReportProgress(0.45 + 0.5 * fraction, "rlcd42-identify-upload");
    },
  });
  ReportProgress(0.95, "rlcd42-identify");
  Log('rlcd42_bringup uploaded — open serial 115200 for [BOOT]/[FLASH]/[PSRAM]/[BTN]<context="task log"/>');
  ReportProgress(1, "rlcd42-identify");
}
_Tag(rlcd42Identify, {
  gulpName: "rlcd42:identify",
  µDisplayName: 'Identify Chip / Flash / PSRAM (RLCD)<context="µDisplayName"/>',
  µDescription:
    'Build and flash rlcd42_bringup identity probe. Reports chip/flash/PSRAM/buttons over serial. No display/SD init.<context="µDescription"/>',
  µTooltip: 'ESP32-S3-RLCD-4.2 — first bring-up step.<context="µTooltip"/>',
  µGroup: 'RLCD 4.2 Hardware<context="µGroup"/>',
  µVisible: visibleOnRlcd42,
});
gulp.task("rlcd42:identify", rlcd42Identify);

export async function rlcd42Docs() {
  ReportProgress(0, "rlcd42-docs");
  Log('Platform ID: waveshare-rlcd42<context="task log"/>');
  Log('Docs: docs/hardware/waveshare-rlcd42.md + docs/hardware/targets.md<context="task log"/>');
  ReportProgress(1, "rlcd42-docs");
}
_Tag(rlcd42Docs, {
  gulpName: "rlcd42:docs",
  µDisplayName: 'RLCD 4.2 Board Notes V<version/><context="µDisplayName"/>',
  µDescription:
    'Prints known vs unknown hardware facts for the RLCD 4.2 board. No flash.<context="µDescription"/>',
  µGroup: 'RLCD 4.2 Hardware<context="µGroup"/>',
  µVisible: visibleOnRlcd42,
});
gulp.task("rlcd42:docs", rlcd42Docs);

export async function rlcd42DisplayProbe() {
  ReportProgress(0, "rlcd42-display");
  const platformId = resolveActivePlatform();
  if (platformId !== PLATFORM_RLCD42) {
    throw new Error(
      `rlcd42 display probe requires Project Control Target = waveshare-rlcd42 (got ${platformId})`,
    );
  }
  const env = "rlcd42_display_probe";
  const device = await resolveEsp2DeviceForTask("rlcd42 display probe — device", {
    expectedPlatform: PLATFORM_RLCD42,
    operation: "flash",
    allowUnidentifiedPick: true,
  });
  const port = device.port;
  Log(
    'rlcd42 display probe: build+upload env=<env/> → <port/><context="task log"/>…',
    { env, port },
  );
  await Pio(rootDir, ["run", "-e", env], "pio build");
  ReportProgress(0.45, "rlcd42-display-upload");
  await Pio(rootDir, UploadArgs(port, env), "pio upload", {
    onProgress: ({ fraction }) => {
      ReportProgress(0.45 + 0.5 * fraction, "rlcd42-display-upload");
    },
  });
  ReportProgress(0.95, "rlcd42-display");
  Log('ST7305 probe uploaded — confirm TOP/BOTTOM/L/R in ambient light<context="task log"/>');
  ReportProgress(1, "rlcd42-display");
}
_Tag(rlcd42DisplayProbe, {
  gulpName: "rlcd42:display",
  µDisplayName: 'Display Probe (ST7305 marks)<context="µDisplayName"/>',
  µDescription:
    'Build/flash rlcd42_display_probe: ST7305 orientation + greyscale bars. Requires visual confirmation.<context="µDescription"/>',
  µGroup: 'RLCD 4.2 Hardware<context="µGroup"/>',
  µVisible: visibleOnRlcd42,
  µExecutionRestrictions: { deny: ["flash", "upload", "build", "rlcd42:identify"] },
});
gulp.task("rlcd42:display", rlcd42DisplayProbe);
