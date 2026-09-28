// ===========================================
// git-backup.mjs — explicit checkpoint commit for ESP][
// ===========================================

import { spawn } from "node:child_process";
import { existsSync } from "node:fs";
import { join } from "node:path";

/** Paths that must be considered for a project checkpoint. */
export const GIT_BACKUP_ENSURE_PATHS = Object.freeze([
  "CLAUDE.md",
  "README.md",
  "platformio.ini",
  "package.json",
  "gulpfile.mjs",
  ".gitignore",
  "src",
  "include",
  "docs",
  "dev",
  "config/nas.targets.example",
]);

/** Never stage these even if somehow tracked. */
export const GIT_BACKUP_NEVER_STAGE = Object.freeze([
  "config/nas.targets.local",
  ".env",
  "node_modules",
  ".pio",
  "compile_commands.json",
  // Downloaded / runtime Apple II media (also gitignored under /local/apple2/**)
  "local/apple2",
  "local/roms",
  "local/sd-backups",
  "library/user",
]);

/**
 * @param {string} _cwd
 * @param {string[]} _args
 * @param {{ allowFail?: boolean }} [_opts]
 * @returns {Promise<{ code: number, stdout: string, stderr: string }>}
 */
export function Git(_cwd, _args, _opts = {}) {
  return new Promise((_resolve, _reject) => {
    const child = spawn("git", _args, {
      cwd: _cwd,
      windowsHide: true,
      stdio: ["ignore", "pipe", "pipe"],
    });
    let stdout = "";
    let stderr = "";
    child.stdout.on("data", (c) => {
      stdout += c.toString();
    });
    child.stderr.on("data", (c) => {
      stderr += c.toString();
    });
    child.on("error", _reject);
    child.on("close", (_code) => {
      const code = _code ?? 1;
      if (code !== 0 && !_opts.allowFail) {
        _reject(
          new Error(
            `git ${_args.join(" ")} failed (${code}): ${stderr.trim() || stdout.trim()}`
          )
        );
      } else {
        _resolve({ code, stdout, stderr });
      }
    });
  });
}

/**
 * @param {Date} [_date]
 */
export function BackupCommitMessage(_date = new Date()) {
  const pad = (n) => String(n).padStart(2, "0");
  const y = _date.getFullYear();
  const m = pad(_date.getMonth() + 1);
  const d = pad(_date.getDate());
  const hh = pad(_date.getHours());
  const mm = pad(_date.getMinutes());
  return `backup: ESP][ ${y}-${m}-${d} ${hh}:${mm}`;
}

/**
 * @param {string} _root
 */
export function IsGitRepo(_root) {
  return existsSync(join(_root, ".git"));
}

/**
 * Inspect + optionally create a checkpoint commit (never force / hard reset).
 * @param {string} _root
 * @param {{
 *   dryRun?: boolean,
 *   push?: boolean,
 *   log?: (msg: string) => void,
 *   warn?: (msg: string) => void,
 * }} [_opts]
 */
export async function RunGitBackup(_root, _opts = {}) {
  const log = _opts.log ?? console.log;
  const warn = _opts.warn ?? console.warn;
  const dryRun =
    _opts.dryRun === true ||
    process.env.ESP2_BACKUP_GIT_DRY_RUN === "1" ||
    process.env.ESP2_GIT_BACKUP_DRY_RUN === "1";
  const doPush =
    _opts.push !== false && process.env.ESP2_BACKUP_GIT_NO_PUSH !== "1";

  log("[GIT] Inspecting repository state…");

  if (!IsGitRepo(_root)) {
    warn(
      "[GIT] No Git repository (.git missing). Initialize with `git init` when ready, then re-run backup:git."
    );
    warn(
      "[GIT] No commit or push was attempted (non-destructive). CLAUDE.md and sources remain on disk only."
    );
    return {
      ok: false,
      reason: "not-a-repo",
      dryRun,
      message: BackupCommitMessage(),
    };
  }

  const status = await Git(_root, ["status", "--porcelain"], { allowFail: true });
  const branch = await Git(_root, ["rev-parse", "--abbrev-ref", "HEAD"], {
    allowFail: true,
  });
  const remote = await Git(_root, ["remote", "-v"], { allowFail: true });

  log(`[GIT] Branch: ${(branch.stdout || "").trim() || "(unknown)"}`);
  if (!(remote.stdout || "").trim()) {
    warn("[GIT] No remote configured — commit can still be local; push will be skipped.");
  } else {
    log("[GIT] Remotes:\n" + remote.stdout.trim());
  }

  // Ensure important paths are staged (respects .gitignore)
  const ensure = [...GIT_BACKUP_ENSURE_PATHS].filter((p) =>
    existsSync(join(_root, p))
  );
  if (!ensure.includes("CLAUDE.md") && !existsSync(join(_root, "CLAUDE.md"))) {
    throw new Error(
      "[GIT] CLAUDE.md is missing from the project root — refusing backup (required documentation)."
    );
  }

  log("[GIT] Ensuring project paths (gitignore still applies):");
  for (const p of ensure) log(`  + ${p}`);

  if (dryRun) {
    log("[GIT] DRY-RUN — showing status; no add/commit/push.");
    log(
      status.stdout.trim()
        ? "[GIT] Working tree:\n" + status.stdout.trim()
        : "[GIT] Working tree clean (nothing unstaged)."
    );
    const msg = BackupCommitMessage();
    log(`[GIT] Proposed commit message: ${msg}`);
    log("[GIT] CLAUDE.md would be included via ensure-path list.");
    return {
      ok: true,
      reason: "dry-run",
      dryRun: true,
      message: msg,
      status: status.stdout,
    };
  }

  await Git(_root, ["add", "-A", "--", ...ensure]);

  // Double-check CLAUDE.md is not ignored
  const checkClaude = await Git(_root, ["check-ignore", "-v", "CLAUDE.md"], {
    allowFail: true,
  });
  if (checkClaude.code === 0) {
    throw new Error(
      "[GIT] CLAUDE.md is ignored by .gitignore — fix .gitignore before backup:git."
    );
  }

  // Unstage anything that must never be committed
  for (const bad of GIT_BACKUP_NEVER_STAGE) {
    if (existsSync(join(_root, bad))) {
      await Git(_root, ["reset", "-q", "HEAD", "--", bad], { allowFail: true });
    }
  }

  const staged = await Git(_root, ["diff", "--cached", "--name-status"], {
    allowFail: true,
  });
  const stagedList = (staged.stdout || "").trim();
  if (!stagedList) {
    const dirty = await Git(_root, ["status", "--porcelain"], { allowFail: true });
    if (!(dirty.stdout || "").trim()) {
      log("[GIT] Nothing to commit — working tree already clean.");
      return {
        ok: true,
        reason: "clean",
        dryRun: false,
        message: BackupCommitMessage(),
      };
    }
    warn(
      "[GIT] Changes exist but nothing staged after filters. Review .gitignore / secrets exclusions."
    );
    log(dirty.stdout.trim());
    return {
      ok: false,
      reason: "nothing-staged",
      dryRun: false,
      message: BackupCommitMessage(),
    };
  }

  log("[GIT] Staged for commit:\n" + stagedList);

  // CLAUDE.md must appear if it differs or was never committed
  const claudeTracked = await Git(_root, ["ls-files", "--", "CLAUDE.md"], {
    allowFail: true,
  });
  const claudeInCommit =
    stagedList.split(/\r?\n/).some((l) => l.includes("CLAUDE.md")) ||
    Boolean((claudeTracked.stdout || "").trim());
  if (!claudeInCommit) {
    await Git(_root, ["add", "-f", "--", "CLAUDE.md"]);
    log("[GIT] Force-added CLAUDE.md to guarantee inclusion.");
  }

  const message = BackupCommitMessage();
  log(`[GIT] Creating checkpoint: ${message}`);
  await Git(_root, ["commit", "-m", message]);

  let pushed = false;
  if (doPush) {
    const remotes = await Git(_root, ["remote"], { allowFail: true });
    const names = (remotes.stdout || "")
      .split(/\r?\n/)
      .map((s) => s.trim())
      .filter(Boolean);
    if (names.length === 0) {
      warn("[GIT] Push skipped — no remote.");
    } else {
      const primary = names.includes("origin") ? "origin" : names[0];
      log(`[GIT] Pushing to ${primary}…`);
      try {
        await Git(_root, ["push", "-u", primary, "HEAD"]);
        pushed = true;
        log(`[GIT] Push OK (${primary}).`);
      } catch (err) {
        warn(`[GIT] Push failed (commit kept locally): ${err.message}`);
      }
    }
  } else {
    log("[GIT] Push disabled (ESP2_BACKUP_GIT_NO_PUSH=1).");
  }

  return {
    ok: true,
    reason: "committed",
    dryRun: false,
    message,
    pushed,
  };
}
