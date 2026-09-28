import assert from "node:assert/strict";
import test from "node:test";
import {
  copyFileSync,
  mkdirSync,
  mkdtempSync,
  readFileSync,
  rmSync,
  writeFileSync,
  existsSync,
} from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";
import {
  backupEsp2Sd,
  restoreEsp2Sd,
  sanitizeRelativePath,
  verifyBackupHashes,
  loadAndValidateManifest,
  defaultSdBackupRoot,
  SD_BACKUP_DIRNAME,
} from "./esp2-sd-backup.mjs";

function makeFixtureSource() {
  const root = mkdtempSync(join(tmpdir(), "esp2-sd-src-"));
  const esp2 = join(root, "esp2");
  mkdirSync(join(esp2, "config"), { recursive: true });
  mkdirSync(join(esp2, "roms"), { recursive: true });
  mkdirSync(join(esp2, "disks"), { recursive: true });
  writeFileSync(join(esp2, "config", "system.json"), '{"schemaVersion":1}\n');
  writeFileSync(join(esp2, "config", "macros.json"), '{"schemaVersion":1,"macros":[]}\n');
  writeFileSync(join(esp2, "roms", "system.rom"), Buffer.from([0x01, 0x02, 0xaa, 0x00]));
  writeFileSync(join(esp2, "disks", "Demo.dsk"), Buffer.alloc(64, 0x5a));
  writeFileSync(join(esp2, "extra-user.bin"), Buffer.from("user-extra"));
  return root;
}

test("sanitizeRelativePath rejects traversal and absolutes", () => {
  assert.equal(sanitizeRelativePath("esp2/a.bin"), "esp2/a.bin");
  assert.throws(() => sanitizeRelativePath("../etc/passwd"));
  assert.throws(() => sanitizeRelativePath("C:/windows"));
  assert.throws(() => sanitizeRelativePath("/esp2/x"));
});

test("backup creates manifest + verified hashes", async () => {
  const project = mkdtempSync(join(tmpdir(), "esp2-proj-"));
  const src = makeFixtureSource();
  const destRoot = join(project, SD_BACKUP_DIRNAME);
  const r = await backupEsp2Sd(project, { sourcePath: src, destRoot, stamp: "test-backup-1" });
  assert.ok(existsSync(join(r.backupDir, "manifest.json")));
  assert.equal(existsSync(join(r.backupDir, ".incomplete")), false);
  assert.equal(r.manifest.totals.files, 5);
  assert.ok(r.manifest.totals.bytes > 0);
  await verifyBackupHashes(r.backupDir);
  rmSync(project, { recursive: true, force: true });
  rmSync(src, { recursive: true, force: true });
});

test("restore to smaller fixture with enough space", async () => {
  const project = mkdtempSync(join(tmpdir(), "esp2-proj-"));
  const src = makeFixtureSource();
  const destRoot = join(project, SD_BACKUP_DIRNAME);
  const bak = await backupEsp2Sd(project, { sourcePath: src, destRoot, stamp: "test-backup-2" });

  const dest = mkdtempSync(join(tmpdir(), "esp2-sd-dst-"));
  // Simulate stale destination content
  mkdirSync(join(dest, "esp2", "config"), { recursive: true });
  writeFileSync(join(dest, "esp2", "config", "stale.json"), "OLD\n");
  writeFileSync(join(dest, "unrelated-on-card.txt"), "keep-me\n");

  const res = await restoreEsp2Sd(project, {
    backupDir: bak.backupDir,
    destPath: dest,
  });
  assert.equal(res.verified, true);
  assert.equal(existsSync(join(dest, "esp2", "config", "stale.json")), false);
  assert.equal(existsSync(join(dest, "unrelated-on-card.txt")), true);
  assert.equal(
    readFileSync(join(dest, "esp2", "roms", "system.rom")).equals(Buffer.from([0x01, 0x02, 0xaa, 0x00])),
    true,
  );
  rmSync(project, { recursive: true, force: true });
  rmSync(src, { recursive: true, force: true });
  rmSync(dest, { recursive: true, force: true });
});

test("corrupt backup refused before restore", async () => {
  const project = mkdtempSync(join(tmpdir(), "esp2-proj-"));
  const src = makeFixtureSource();
  const destRoot = join(project, SD_BACKUP_DIRNAME);
  const bak = await backupEsp2Sd(project, { sourcePath: src, destRoot, stamp: "test-backup-3" });
  const victim = join(bak.backupDir, "files", "esp2", "roms", "system.rom");
  writeFileSync(victim, Buffer.from([0xff]));
  await assert.rejects(() => verifyBackupHashes(bak.backupDir));
  const dest = mkdtempSync(join(tmpdir(), "esp2-sd-dst-"));
  await assert.rejects(() =>
    restoreEsp2Sd(project, { backupDir: bak.backupDir, destPath: dest }),
  );
  rmSync(project, { recursive: true, force: true });
  rmSync(src, { recursive: true, force: true });
  rmSync(dest, { recursive: true, force: true });
});

test("incomplete backup rejected", async () => {
  const project = mkdtempSync(join(tmpdir(), "esp2-proj-"));
  const src = makeFixtureSource();
  const destRoot = join(project, SD_BACKUP_DIRNAME);
  const bak = await backupEsp2Sd(project, { sourcePath: src, destRoot, stamp: "test-backup-4" });
  writeFileSync(join(bak.backupDir, ".incomplete"), "x\n");
  assert.throws(() => loadAndValidateManifest(bak.backupDir));
  rmSync(project, { recursive: true, force: true });
  rmSync(src, { recursive: true, force: true });
});

test("defaultSdBackupRoot points at local/sd-backups", () => {
  assert.ok(defaultSdBackupRoot("/proj").replace(/\\/g, "/").endsWith("local/sd-backups"));
});

test("assertEnoughSpace rejects when destination too small", async () => {
  const { assertEnoughSpace } = await import("./esp2-sd-backup.mjs");
  assert.throws(() => assertEnoughSpace(1000, 100, 0));
  assert.doesNotThrow(() => assertEnoughSpace(1000, 100, 950));
  assert.doesNotThrow(() => assertEnoughSpace(1000, undefined, 0));
});

test("local/sd-backups payload is gitignored", () => {
  const projectRoot = join(dirname(fileURLToPath(import.meta.url)), "../..");
  const probeDir = join(projectRoot, "local", "sd-backups", "_git_probe");
  mkdirSync(probeDir, { recursive: true });
  const probe = join(probeDir, "probe.bin");
  writeFileSync(probe, Buffer.from([1, 2, 3]));
  const r = spawnSync(
    "git",
    ["check-ignore", "-v", "--", "local/sd-backups/_git_probe/probe.bin"],
    { cwd: projectRoot, encoding: "utf8", windowsHide: true },
  );
  assert.equal(r.status, 0, "sd-backups payload must be gitignored");
  rmSync(probeDir, { recursive: true, force: true });
});

test("NAS policy lists local/sd-backups as local asset", async () => {
  const { NAS_BACKUP_LOCAL_ASSET_DIRS, NAS_BACKUP_INCLUDE_DIRS } = await import(
    "./nas-backup.mjs"
  );
  assert.ok(NAS_BACKUP_LOCAL_ASSET_DIRS.includes("local/sd-backups"));
  assert.ok(NAS_BACKUP_INCLUDE_DIRS.includes("local/sd-backups"));
});
