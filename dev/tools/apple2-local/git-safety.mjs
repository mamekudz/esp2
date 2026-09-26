/**
 * Gitignore safety for local Apple II media destinations.
 * Downloads MUST abort if the destination is not ignored by Git.
 */

import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, writeFileSync, unlinkSync } from "node:fs";
import { dirname, join, relative, resolve } from "node:path";
import { ESP2_ROOT, LOCAL_APPLE2_ROOT } from "./paths.mjs";

/**
 * @param {string} absPath
 * @param {{ cwd?: string }} [opts]
 * @returns {{ ignored: boolean, detail: string }}
 */
export function checkGitIgnored(absPath, opts = {}) {
  const cwd = opts.cwd || ESP2_ROOT;
  const rel = relative(cwd, resolve(absPath)).replace(/\\/g, "/");
  const r = spawnSync("git", ["check-ignore", "-v", "--", rel], {
    cwd,
    encoding: "utf8",
    windowsHide: true,
  });
  if (r.status === 0 && (r.stdout || "").trim()) {
    return { ignored: true, detail: (r.stdout || "").trim(), relative: rel };
  }
  // Also try absolute path (some git versions prefer it)
  const r2 = spawnSync("git", ["check-ignore", "-v", "--", absPath], {
    cwd,
    encoding: "utf8",
    windowsHide: true,
  });
  if (r2.status === 0 && (r2.stdout || "").trim()) {
    return { ignored: true, detail: (r2.stdout || "").trim(), relative: rel };
  }
  return {
    ignored: false,
    detail: (r.stderr || r2.stderr || "").trim() || "not ignored",
    relative: rel,
  };
}

/**
 * Ensure destination directory (and a probe file path) is gitignored.
 * Creates the directory if needed. ABORTs with Error if not ignored.
 * @param {string} destDir absolute directory under local/apple2
 * @param {{ probeName?: string, cwd?: string }} [opts]
 */
export function assertDestinationIgnored(destDir, opts = {}) {
  const cwd = opts.cwd || ESP2_ROOT;
  const absDir = resolve(destDir);
  const root = resolve(LOCAL_APPLE2_ROOT);

  if (!absDir.startsWith(root) && absDir !== root) {
    throw new Error(
      `[APPLE2-LOCAL][ABORT] destination outside local/apple2/: ${absDir}`,
    );
  }

  if (!existsSync(absDir)) {
    mkdirSync(absDir, { recursive: true });
  }

  const probeName = opts.probeName || ".esp2-gitignore-probe";
  const probePath = join(absDir, probeName);
  const created = !existsSync(probePath);
  if (created) {
    writeFileSync(probePath, "probe\n", "utf8");
  }

  try {
    const check = checkGitIgnored(probePath, { cwd });
    if (!check.ignored) {
      throw new Error(
        `[APPLE2-LOCAL][ABORT] destination is NOT gitignored — refusing download/write.\n` +
          `  path: ${check.relative}\n` +
          `  expected rule: /local/apple2/**\n` +
          `  git check-ignore: ${check.detail}`,
      );
    }
    return check;
  } finally {
    if (created && existsSync(probePath)) {
      try {
        unlinkSync(probePath);
      } catch {
        /* ignore */
      }
    }
  }
}

/**
 * Paths that backup:git must never stage (relative to repo root).
 */
export function backupNeverStageMediaPaths() {
  return ["local/apple2", "local/roms", "library/user"];
}
