// ===========================================
// nas-backup.mjs — up to 3 NAS destinations for ESP][
// ===========================================

import { spawn } from "node:child_process";
import {
  copyFileSync,
  existsSync,
  mkdirSync,
  readFileSync,
  readdirSync,
  statSync,
  writeFileSync,
} from "node:fs";
import { dirname, join, resolve } from "node:path";

export const NAS_BACKUP_MAX_DESTINATIONS = 3;

/** Non-reproducible trees (µGulp / Watchy selective-backup convention). */
export const NAS_BACKUP_INCLUDE_DIRS = Object.freeze([
  "src",
  "include",
  "docs",
  "dev",
  "config",
  ".git",
]);

export const NAS_BACKUP_INCLUDE_FILES = Object.freeze([
  "CLAUDE.md",
  "README.md",
  "platformio.ini",
  "package.json",
  "package-lock.json",
  "gulpfile.mjs",
  ".gitignore",
  "LICENSE",
]);

export const NAS_BACKUP_EXCLUDE_DIRS = Object.freeze([
  "node_modules",
  ".pio",
  ".cache",
  ".microgulp",
  "_refs",
  "tmp",
  "temp",
  // Third-party / user-supplied Apple II media — never silently NAS-backup.
  // Opt-in archival is a separate future task (not part of backup / backup:all).
  "local",
]);

export const NAS_BACKUP_EXCLUDE_FILES = Object.freeze([
  "Thumbs.db",
  ".DS_Store",
  "desktop.ini",
  "compile_commands.json",
  "nas.targets.local",
  ".env",
]);

export const NAS_BACKUP_REQUIRED_RELATIVE = Object.freeze([
  "CLAUDE.md",
  "platformio.ini",
  "package.json",
  "gulpfile.mjs",
  "src/main.cpp",
]);

/**
 * @param {Iterable<string>} _paths
 * @returns {string[]}
 */
export function UniqueNasTargets(_paths) {
  const seen = new Set();
  const list = [];
  let rejectedExtra = 0;
  for (const raw of _paths) {
    const path = String(raw ?? "").trim();
    if (!path) continue;
    const key = path.toLowerCase();
    if (seen.has(key)) continue;
    seen.add(key);
    if (list.length >= NAS_BACKUP_MAX_DESTINATIONS) {
      rejectedExtra += 1;
      continue;
    }
    list.push(path);
  }
  return { destinations: list, rejectedExtra };
}

/**
 * Parse KEY=VALUE lines (nas.targets.local / example).
 * @param {string} _text
 */
function _ParseEnvFile(_text) {
  /** @type {Record<string, string>} */
  const map = {};
  for (const line of String(_text).split(/\r?\n/)) {
    const trimmed = line.trim();
    if (!trimmed || trimmed.startsWith("#")) continue;
    const eq = trimmed.indexOf("=");
    if (eq <= 0) continue;
    const key = trimmed.slice(0, eq).trim();
    let value = trimmed.slice(eq + 1).trim();
    if (
      (value.startsWith('"') && value.endsWith('"')) ||
      (value.startsWith("'") && value.endsWith("'"))
    ) {
      value = value.slice(1, -1);
    }
    map[key] = value;
  }
  return map;
}

/**
 * Optional remembered defaults from gitignored config/nas.targets.local.
 * Never hard-codes private NAS paths into source.
 * @param {string} _root
 * @returns {{ t1: string, t2: string, t3: string, dryRun: boolean }}
 */
export function LoadNasLocalDefaults(_root) {
  const localPath = join(_root, "config", "nas.targets.local");
  if (!existsSync(localPath)) {
    return { t1: "", t2: "", t3: "", dryRun: false };
  }
  const map = _ParseEnvFile(readFileSync(localPath, "utf8"));
  return {
    t1: String(map.NAS_TARGET_1 ?? "").trim(),
    t2: String(map.NAS_TARGET_2 ?? "").trim(),
    t3: String(map.NAS_TARGET_3 ?? "").trim(),
    dryRun: map.ESP2_NAS_DRY_RUN === "1",
  };
}

/**
 * Resolve 0–3 NAS targets.
 * Precedence (µGulp / Watchy style):
 *   1. env NAS_TARGET_1..3
 *   2. env ESP2_NAS_BACKUP_PATHS (|/;)
 *   3. µParameters / form slots destination1..3 (MICROGULP_PARAMS)
 *   4. config/nas.targets.local
 * Empty result is valid (0 destinations).
 *
 * @param {string} _root
 * @param {{
 *   destination1?: string,
 *   destination2?: string,
 *   destination3?: string,
 *   dryRun?: boolean,
 * } | null} [_form]
 * @returns {{ destinations: string[], dryRun: boolean, rejectedExtra: number, source: string }}
 */
export function ResolveNasTargets(_root, _form = null) {
  const envDry =
    process.env.ESP2_NAS_DRY_RUN === "1" ||
    process.env.ESP2_NAS_BACKUP_DRY_RUN === "1";

  const fromEnv = [
    process.env.NAS_TARGET_1,
    process.env.NAS_TARGET_2,
    process.env.NAS_TARGET_3,
  ].map((v) => String(v ?? "").trim());

  if (fromEnv.some(Boolean)) {
    const { destinations, rejectedExtra } = UniqueNasTargets(fromEnv);
    return { destinations, dryRun: envDry, rejectedExtra, source: "env" };
  }

  const pathsEnv = String(process.env.ESP2_NAS_BACKUP_PATHS ?? "").trim();
  if (pathsEnv) {
    const { destinations, rejectedExtra } = UniqueNasTargets(
      pathsEnv.split(/[|;]/)
    );
    return {
      destinations,
      dryRun: envDry,
      rejectedExtra,
      source: "ESP2_NAS_BACKUP_PATHS",
    };
  }

  if (_form) {
    const { destinations, rejectedExtra } = UniqueNasTargets([
      _form.destination1,
      _form.destination2,
      _form.destination3,
    ]);
    return {
      destinations,
      dryRun: envDry || _form.dryRun === true,
      rejectedExtra,
      source: "form",
    };
  }

  const local = LoadNasLocalDefaults(_root);
  if (local.t1 || local.t2 || local.t3) {
    const { destinations, rejectedExtra } = UniqueNasTargets([
      local.t1,
      local.t2,
      local.t3,
    ]);
    return {
      destinations,
      dryRun: envDry || local.dryRun,
      rejectedExtra,
      source: "config/nas.targets.local",
    };
  }

  return {
    destinations: [],
    dryRun: envDry,
    rejectedExtra: 0,
    source: "none",
  };
}

/**
 * @param {string} _destination
 * @param {string} _sourceResolved
 */
export function AssertNasBackupTarget(_destination, _sourceResolved) {
  if (!existsSync(_destination)) {
    throw new Error(
      `destination missing: "${_destination}" (create the folder first; no silent local fallback)`
    );
  }
  const destResolved = resolve(_destination);
  const src = _sourceResolved.toLowerCase();
  const dest = destResolved.toLowerCase();
  const srcPrefix = src.replace(/[\\/]+$/, "") + "\\";
  const destPrefix = dest.replace(/[\\/]+$/, "") + "\\";
  if (
    src === dest ||
    src.startsWith(destPrefix) ||
    dest.startsWith(srcPrefix) ||
    /^[a-z]:[\\/]?$/i.test(destResolved) ||
    /^\\\\[^\\]+\\[^\\]+[\\]?$/.test(destResolved)
  ) {
    throw new Error(
      "destination must be a dedicated, non-overlapping backup folder, not a drive or share root"
    );
  }
  return destResolved;
}

/**
 * @param {string} _cmd
 * @param {string[]} _args
 * @returns {Promise<number>}
 */
function _RunRobocopy(_cmd, _args) {
  return new Promise((_resolve, _reject) => {
    const child = spawn(_cmd, _args, {
      // no shell — avoids DEP0190; robocopy.exe is on PATH under Windows
      windowsHide: true,
      stdio: ["ignore", "pipe", "pipe"],
    });
    let err = "";
    child.stderr.on("data", (c) => {
      err += c.toString();
    });
    child.on("error", _reject);
    child.on("close", (_code) => {
      // robocopy: 0–7 success-ish, >=8 failure
      const code = _code ?? 1;
      if (code >= 8) {
        _reject(new Error(`robocopy failed (${code}): ${err || _args.join(" ")}`));
      } else {
        _resolve(code);
      }
    });
  });
}

/**
 * Recursive copy fallback (non-Windows / dry-run listing helper).
 * @param {string} _src
 * @param {string} _dest
 * @param {{ dryRun?: boolean, excludeDirs?: Set<string> }} _opts
 */
function _CopyTree(_src, _dest, _opts) {
  const excludeDirs = _opts.excludeDirs ?? new Set(NAS_BACKUP_EXCLUDE_DIRS);
  if (!existsSync(_src)) return 0;
  const st = statSync(_src);
  if (st.isFile()) {
    if (!_opts.dryRun) {
      mkdirSync(dirname(_dest), { recursive: true });
      copyFileSync(_src, _dest);
    }
    return 1;
  }
  let count = 0;
  if (!_opts.dryRun) mkdirSync(_dest, { recursive: true });
  for (const name of readdirSync(_src)) {
    if (excludeDirs.has(name)) continue;
    if (NAS_BACKUP_EXCLUDE_FILES.includes(name)) continue;
    count += _CopyTree(join(_src, name), join(_dest, name), _opts);
  }
  return count;
}

/**
 * Selective backup into one destination.
 * @param {string} _sourceResolved
 * @param {string} _destResolved
 * @param {boolean} _dryRun
 */
export async function MirrorNonReproducible(
  _sourceResolved,
  _destResolved,
  _dryRun
) {
  if (process.platform === "win32") {
    let lastExit = 0;
    const common = [
      "/FFT",
      "/DST",
      "/R:1",
      "/W:1",
      "/MT:4",
      "/NFL",
      "/NDL",
      "/NP",
      "/XD",
      ...NAS_BACKUP_EXCLUDE_DIRS,
      "/XF",
      ...NAS_BACKUP_EXCLUDE_FILES,
    ];
    if (_dryRun) common.push("/L");

    for (const rel of NAS_BACKUP_INCLUDE_DIRS) {
      const src = join(_sourceResolved, rel);
      if (!existsSync(src)) continue;
      const dest = join(_destResolved, rel);
      lastExit = await _RunRobocopy("robocopy.exe", [src, dest, "/E", ...common]);
    }

    const rootFiles = NAS_BACKUP_INCLUDE_FILES.filter(
      (rel) =>
        !rel.includes("/") &&
        !rel.includes("\\") &&
        existsSync(join(_sourceResolved, rel))
    );
    if (rootFiles.length > 0) {
      lastExit = await _RunRobocopy("robocopy.exe", [
        _sourceResolved,
        _destResolved,
        ...rootFiles,
        "/XO",
        ...common.filter((a) => a !== "/MIR"),
      ]);
    }

    if (!_dryRun) {
      const manifest = {
        project: "ESP][",
        package: "esp2",
        createdAt: new Date().toISOString(),
        source: _sourceResolved,
        destination: _destResolved,
        includeDirs: [...NAS_BACKUP_INCLUDE_DIRS],
        includeFiles: [...NAS_BACKUP_INCLUDE_FILES],
        excludeDirs: [...NAS_BACKUP_EXCLUDE_DIRS],
      };
      writeFileSync(
        join(_destResolved, "BACKUP_MANIFEST.json"),
        JSON.stringify(manifest, null, 2) + "\n",
        "utf8"
      );
    }
    return lastExit;
  }

  // Non-Windows fallback
  let files = 0;
  for (const rel of NAS_BACKUP_INCLUDE_DIRS) {
    const src = join(_sourceResolved, rel);
    if (!existsSync(src)) continue;
    files += _CopyTree(src, join(_destResolved, rel), {
      dryRun: _dryRun,
      excludeDirs: new Set(NAS_BACKUP_EXCLUDE_DIRS),
    });
  }
  for (const rel of NAS_BACKUP_INCLUDE_FILES) {
    const src = join(_sourceResolved, rel);
    if (!existsSync(src)) continue;
    files += _CopyTree(src, join(_destResolved, rel), { dryRun: _dryRun });
  }
  return files;
}

/**
 * @param {string} _backupRoot
 */
export function VerifyBackupContents(_backupRoot) {
  const missing = [];
  for (const rel of NAS_BACKUP_REQUIRED_RELATIVE) {
    if (!existsSync(join(_backupRoot, rel))) missing.push(rel);
  }
  return { ok: missing.length === 0, missing };
}
