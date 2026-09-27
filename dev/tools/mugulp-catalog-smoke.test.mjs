/**
 * Functional smoke tests for recovered µGulp workflows (non-destructive).
 */
import test from "node:test";
import assert from "node:assert/strict";
import { existsSync, readFileSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { ResolvePioEnv, PIO_ENVS, PIO_ENV_DEFAULT } from "./pio.mjs";
import {
  ResolveNasTargets,
  NAS_BACKUP_EXCLUDE_DIRS,
  NAS_BACKUP_INCLUDE_DIRS,
  NAS_BACKUP_LOCAL_ASSET_DIRS,
  VerifyBackupContents,
} from "./nas-backup.mjs";
import {
  GIT_BACKUP_NEVER_STAGE,
  RunGitBackup,
  IsGitRepo,
} from "./git-backup.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "../..");

test("firmware env resolves valid PlatformIO environment", () => {
  const prev = process.env.ESP2_PIO_ENV;
  try {
    delete process.env.ESP2_PIO_ENV;
    assert.equal(ResolvePioEnv(), PIO_ENV_DEFAULT);
    assert.ok(PIO_ENVS.includes(ResolvePioEnv()));
    process.env.ESP2_PIO_ENV = "core_smoke";
    assert.equal(ResolvePioEnv(), "core_smoke");
    assert.equal(ResolvePioEnv("apple2_text"), "apple2_text");
  } finally {
    if (prev === undefined) delete process.env.ESP2_PIO_ENV;
    else process.env.ESP2_PIO_ENV = prev;
  }
  const ini = readFileSync(join(ROOT, "platformio.ini"), "utf8");
  for (const env of PIO_ENVS) {
    assert.match(ini, new RegExp(`\\[env:${env}\\]`));
  }
});

test("Git dry-run constructs checkpoint without committing", async () => {
  assert.ok(IsGitRepo(ROOT));
  assert.ok(GIT_BACKUP_NEVER_STAGE.includes("local/apple2"));
  const result = await RunGitBackup(ROOT, {
    dryRun: true,
    push: false,
    log: () => {},
    warn: () => {},
  });
  assert.equal(result.dryRun, true);
  assert.ok(result.ok);
  assert.match(result.message, /^backup: ESP\]\[ /);
});

test("NAS backup policy includes local/apple2 tree", () => {
  assert.ok(NAS_BACKUP_EXCLUDE_DIRS.includes("node_modules"));
  assert.ok(NAS_BACKUP_EXCLUDE_DIRS.includes(".pio"));
  assert.ok(!NAS_BACKUP_EXCLUDE_DIRS.includes("local"));
  assert.ok(NAS_BACKUP_INCLUDE_DIRS.includes("local/apple2"));
  assert.ok(NAS_BACKUP_LOCAL_ASSET_DIRS.includes("local/apple2"));
  assert.ok(NAS_BACKUP_LOCAL_ASSET_DIRS.includes("local/roms"));
  const prev1 = process.env.NAS_TARGET_1;
  const prev2 = process.env.NAS_TARGET_2;
  const prev3 = process.env.NAS_TARGET_3;
  delete process.env.NAS_TARGET_1;
  delete process.env.NAS_TARGET_2;
  delete process.env.NAS_TARGET_3;
  delete process.env.ESP2_NAS_BACKUP_PATHS;
  try {
    const resolved = ResolveNasTargets(ROOT);
    assert.ok(Array.isArray(resolved.destinations));
  } finally {
    if (prev1 !== undefined) process.env.NAS_TARGET_1 = prev1;
    else delete process.env.NAS_TARGET_1;
    if (prev2 !== undefined) process.env.NAS_TARGET_2 = prev2;
    else delete process.env.NAS_TARGET_2;
    if (prev3 !== undefined) process.env.NAS_TARGET_3 = prev3;
    else delete process.env.NAS_TARGET_3;
  }
});

test("docs task and Apple II media task resolve from gulp module", async () => {
  const mod = await import(
    pathToFileURL(join(ROOT, "gulpfile.mjs")).href + `?smoke=${Date.now()}`
  );
  assert.equal(typeof mod.docs, "function");
  assert.equal(mod.docs.displayName, "docs");
  assert.equal(typeof mod.apple2MediaStatus, "function");
  assert.equal(mod.apple2MediaStatus.displayName, "apple2:media:status");
  assert.equal(typeof mod.gitStatus, "function");
  assert.equal(typeof mod.firmwareEnvTask, "function");
  assert.equal(typeof mod.backupList, "function");
});

test("VerifyBackupContents API is callable (temp empty → missing)", () => {
  const check = VerifyBackupContents(join(ROOT, "docs", "tooling"));
  // docs/tooling is not a NAS backup root — expect missing essentials
  assert.equal(check.ok, false);
  assert.ok(Array.isArray(check.missing) && check.missing.length > 0);
});

test("canonical manifest path exists for smoke consumers", () => {
  assert.ok(
    existsSync(join(ROOT, "docs", "tooling", "gulp-task-manifest.json")),
  );
  assert.ok(existsSync(join(ROOT, "docs", "tooling", "gulp-task-history.md")));
});
