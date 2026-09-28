#!/usr/bin/env node
/**
 * ESP][ logical microSD backup / restore (filesystem tree, not raw card image).
 *
 * Backup root (gitignored, NAS-backed): local/sd-backups/<timestamp>/
 *   manifest.json
 *   files/esp2/...
 *   .incomplete  (present until verification passes)
 *
 * CLI:
 *   node dev/tools/esp2-sd-backup.mjs backup --source E:/ --dest-root local/sd-backups
 *   node dev/tools/esp2-sd-backup.mjs restore --backup local/sd-backups/2026-... --dest F:/ [--dry-run]
 *   node dev/tools/esp2-sd-backup.mjs list --dest-root local/sd-backups
 */
import { createHash } from "node:crypto";
import {
  copyFileSync,
  createReadStream,
  existsSync,
  mkdirSync,
  readFileSync,
  readdirSync,
  renameSync,
  rmSync,
  statSync,
  writeFileSync,
  unlinkSync,
} from "node:fs";
import { dirname, join, relative, resolve, sep } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";
import { pipeline } from "node:stream/promises";
import { GetProjectVersionLabel } from "./project-version.mjs";

export const SD_BACKUP_SCHEMA = 1;
export const SD_BACKUP_KIND = "logical-esp2-tree";
export const SD_BACKUP_PRODUCT = "ESP][";
export const SD_BACKUP_DIRNAME = "local/sd-backups";
export const SD_TREE_DIR = "esp2";
export const INCOMPLETE_MARKER = ".incomplete";

const __dirname = dirname(fileURLToPath(import.meta.url));

/**
 * @param {string} projectRoot
 */
export function defaultSdBackupRoot(projectRoot) {
  return join(projectRoot, SD_BACKUP_DIRNAME);
}

/**
 * Filesystem-safe local timestamp directory name.
 * @param {Date} [when]
 */
export function makeBackupStamp(when = new Date()) {
  const p = (n) => String(n).padStart(2, "0");
  return (
    `${when.getFullYear()}-${p(when.getMonth() + 1)}-${p(when.getDate())}` +
    `_${p(when.getHours())}${p(when.getMinutes())}${p(when.getSeconds())}`
  );
}

/**
 * Normalize and reject path traversal / absolute paths in manifest entries.
 * @param {string} rel
 */
export function sanitizeRelativePath(rel) {
  const raw = String(rel ?? "").replace(/\\/g, "/").trim();
  if (!raw) throw new Error("empty relative path");
  if (raw.startsWith("/") || /^[A-Za-z]:/.test(raw)) {
    throw new Error(`absolute path not allowed: ${raw}`);
  }
  const parts = raw.split("/").filter((p) => p && p !== ".");
  if (parts.some((p) => p === "..")) {
    throw new Error(`path traversal rejected: ${raw}`);
  }
  return parts.join("/");
}

/**
 * @param {number} needBytes
 * @param {number | null | undefined} freeBytes
 * @param {number} [reclaimBytes]
 */
export function assertEnoughSpace(needBytes, freeBytes, reclaimBytes = 0) {
  if (!Number.isFinite(freeBytes)) return;
  const usable = Number(freeBytes) + Number(reclaimBytes || 0);
  if (needBytes > usable) {
    throw new Error(
      `insufficient free space: need ${needBytes} bytes, usable ~${usable} bytes`,
    );
  }
}

/**
 * @param {string} filePath
 * @returns {Promise<string>}
 */
export async function sha256File(filePath) {
  const hash = createHash("sha256");
  await pipeline(createReadStream(filePath), hash);
  return hash.digest("hex");
}

/**
 * Walk files under root; returns relative POSIX paths.
 * @param {string} rootAbs
 * @param {string} [prefix]
 */
export function listFilesRecursive(rootAbs, prefix = "") {
  /** @type {{ rel: string, abs: string, size: number }[]} */
  const out = [];
  if (!existsSync(rootAbs)) return out;
  const entries = readdirSync(rootAbs, { withFileTypes: true });
  for (const ent of entries) {
    const name = ent.name;
    if (name === "." || name === "..") continue;
    const abs = join(rootAbs, name);
    const rel = prefix ? `${prefix}/${name}` : name;
    if (ent.isSymbolicLink()) {
      throw new Error(`symlink not supported: ${rel}`);
    }
    if (ent.isDirectory()) {
      out.push(...listFilesRecursive(abs, rel.replace(/\\/g, "/")));
    } else if (ent.isFile()) {
      const st = statSync(abs);
      out.push({ rel: rel.replace(/\\/g, "/"), abs, size: st.size });
    } else {
      throw new Error(`unsupported filesystem entry: ${rel}`);
    }
  }
  return out;
}

/**
 * Count directories under root (including root if it exists).
 * @param {string} rootAbs
 */
export function countDirectories(rootAbs) {
  if (!existsSync(rootAbs)) return 0;
  let n = 1;
  const walk = (dir) => {
    for (const ent of readdirSync(dir, { withFileTypes: true })) {
      if (!ent.isDirectory()) continue;
      n += 1;
      walk(join(dir, ent.name));
    }
  };
  walk(rootAbs);
  return n;
}

/**
 * Probe Windows volume info for a path (best-effort).
 * @param {string} pathOnVolume
 */
export function probeVolumeInfo(pathOnVolume) {
  const abs = resolve(pathOnVolume);
  /** @type {{ path: string, filesystem?: string, capacityBytes?: number, freeBytes?: number, label?: string }} */
  const info = { path: abs };
  try {
    const drive = abs.match(/^([A-Za-z]:)/)?.[1];
    if (!drive) return info;
    const ps = `
$d = Get-PSDrive -Name '${drive[0]}' -ErrorAction SilentlyContinue
if ($d) {
  $free = [int64]$d.Free
  $used = [int64]$d.Used
  Write-Output ("free=$free")
  Write-Output ("used=$used")
  Write-Output ("cap=$($free+$used)")
}
try {
  $v = Get-Volume -DriveLetter '${drive[0]}' -ErrorAction Stop
  Write-Output ("fs=$($v.FileSystem)")
  Write-Output ("label=$($v.FileSystemLabel)")
} catch {}
`;
    const r = spawnSync(
      "powershell.exe",
      ["-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", ps],
      { encoding: "utf8", windowsHide: true },
    );
    for (const line of String(r.stdout || "").split(/\r?\n/)) {
      const t = line.trim();
      if (t.startsWith("free=")) info.freeBytes = Number(t.slice(5));
      else if (t.startsWith("cap=")) info.capacityBytes = Number(t.slice(4));
      else if (t.startsWith("fs=")) info.filesystem = t.slice(3) || undefined;
      else if (t.startsWith("label=")) info.label = t.slice(6) || undefined;
    }
  } catch {
    /* ignore */
  }
  return info;
}

/**
 * Resolve the esp2 tree on a mounted SD (accept D:/esp2 or D:/).
 * For restore destinations without esp2 yet, pass { allowMissingEsp2: true }.
 * @param {string} selectedPath
 * @param {{ allowMissingEsp2?: boolean }} [opts]
 */
export function resolveEsp2Root(selectedPath, opts = {}) {
  const abs = resolve(selectedPath);
  if (!existsSync(abs)) {
    throw new Error(`path does not exist: ${abs}`);
  }
  const asEsp2 = join(abs, SD_TREE_DIR);
  if (existsSync(asEsp2) && statSync(asEsp2).isDirectory()) {
    return { mountRoot: abs, esp2Root: asEsp2 };
  }
  const base = abs.split(/[/\\]/).pop()?.toLowerCase();
  if (base === SD_TREE_DIR) {
    return { mountRoot: dirname(abs), esp2Root: abs };
  }
  if (opts.allowMissingEsp2) {
    return { mountRoot: abs, esp2Root: asEsp2 };
  }
  throw new Error(
    `No /${SD_TREE_DIR}/ tree under ${abs}. Select the SD mount root (containing esp2/) or the esp2 folder.`,
  );
}

/**
 * @param {string} projectRoot
 * @param {{ sourcePath: string, destRoot?: string, stamp?: string }} opts
 */
export async function backupEsp2Sd(projectRoot, opts) {
  const destRoot = opts.destRoot
    ? resolve(opts.destRoot)
    : defaultSdBackupRoot(projectRoot);
  const stamp = opts.stamp || makeBackupStamp();
  const backupDir = join(destRoot, stamp);
  if (existsSync(backupDir)) {
    throw new Error(`backup directory already exists: ${backupDir}`);
  }

  const { mountRoot, esp2Root } = resolveEsp2Root(opts.sourcePath);
  const vol = probeVolumeInfo(mountRoot);
  const filesSrc = listFilesRecursive(esp2Root, SD_TREE_DIR);
  mkdirSync(backupDir, { recursive: true });
  writeFileSync(join(backupDir, INCOMPLETE_MARKER), "incomplete\n", "utf8");

  const filesDir = join(backupDir, "files");
  mkdirSync(filesDir, { recursive: true });

  /** @type {{ path: string, size: number, sha256: string }[]} */
  const fileRecords = [];
  let totalBytes = 0;

  for (const f of filesSrc) {
    const rel = sanitizeRelativePath(f.rel);
    const destAbs = join(filesDir, ...rel.split("/"));
    mkdirSync(dirname(destAbs), { recursive: true });
    copyFileSync(f.abs, destAbs);
    const st = statSync(destAbs);
    if (st.size !== f.size) {
      throw new Error(`size mismatch after copy: ${rel}`);
    }
    const digest = await sha256File(destAbs);
    const srcDigest = await sha256File(f.abs);
    if (digest !== srcDigest) {
      throw new Error(`SHA-256 mismatch vs source: ${rel}`);
    }
    fileRecords.push({ path: rel, size: st.size, sha256: digest });
    totalBytes += st.size;
  }

  let gitCommit = "";
  try {
    const g = spawnSync("git", ["rev-parse", "HEAD"], {
      cwd: projectRoot,
      encoding: "utf8",
      windowsHide: true,
    });
    if (g.status === 0) gitCommit = String(g.stdout || "").trim();
  } catch {
    /* ignore */
  }

  const manifest = {
    schemaVersion: SD_BACKUP_SCHEMA,
    product: SD_BACKUP_PRODUCT,
    kind: SD_BACKUP_KIND,
    complete: true,
    createdAt: new Date().toISOString(),
    projectVersion: GetProjectVersionLabel(projectRoot),
    gitCommit: gitCommit || undefined,
    source: {
      filesystem: vol.filesystem || "FAT",
      capacityBytes: Number.isFinite(vol.capacityBytes) ? vol.capacityBytes : undefined,
      freeBytes: Number.isFinite(vol.freeBytes) ? vol.freeBytes : undefined,
      label: vol.label || undefined,
      // Host path is metadata-only — not required for restore.
      selectedPathNote: "omitted-for-portability",
    },
    totals: {
      files: fileRecords.length,
      directories: countDirectories(esp2Root),
      bytes: totalBytes,
    },
    files: fileRecords,
  };

  writeFileSync(
    join(backupDir, "manifest.json"),
    `${JSON.stringify(manifest, null, 2)}\n`,
    "utf8",
  );
  unlinkSync(join(backupDir, INCOMPLETE_MARKER));

  return {
    backupDir,
    manifest,
    sourceMount: mountRoot,
    sourceEsp2: esp2Root,
    volume: vol,
  };
}

/**
 * @param {string} backupDir
 */
export function loadAndValidateManifest(backupDir) {
  const dir = resolve(backupDir);
  if (!existsSync(dir)) throw new Error(`backup not found: ${dir}`);
  if (existsSync(join(dir, INCOMPLETE_MARKER))) {
    throw new Error(`backup incomplete (.incomplete present): ${dir}`);
  }
  const manPath = join(dir, "manifest.json");
  if (!existsSync(manPath)) throw new Error(`manifest.json missing: ${dir}`);
  const man = JSON.parse(readFileSync(manPath, "utf8"));
  if (Number(man.schemaVersion) !== SD_BACKUP_SCHEMA) {
    throw new Error(`unsupported schemaVersion: ${man.schemaVersion}`);
  }
  if (man.product !== SD_BACKUP_PRODUCT) {
    throw new Error(`unexpected product: ${man.product}`);
  }
  if (man.kind !== SD_BACKUP_KIND) {
    throw new Error(`unexpected kind: ${man.kind}`);
  }
  if (man.complete !== true) {
    throw new Error("manifest.complete is not true");
  }
  if (!Array.isArray(man.files)) throw new Error("manifest.files missing");
  for (const f of man.files) {
    const rel = sanitizeRelativePath(f.path);
    if (!rel.startsWith(`${SD_TREE_DIR}/`) && rel !== SD_TREE_DIR) {
      throw new Error(`file outside esp2 tree: ${rel}`);
    }
    const abs = join(dir, "files", ...rel.split("/"));
    if (!existsSync(abs)) throw new Error(`missing backup file: ${rel}`);
    const st = statSync(abs);
    if (st.size !== Number(f.size)) {
      throw new Error(`size mismatch in backup store: ${rel}`);
    }
  }
  return { dir, manifest: man, manPath };
}

/**
 * Verify all SHA-256 values in an on-disk backup.
 * @param {string} backupDir
 */
export async function verifyBackupHashes(backupDir) {
  const { dir, manifest } = loadAndValidateManifest(backupDir);
  const mismatches = [];
  for (const f of manifest.files) {
    const rel = sanitizeRelativePath(f.path);
    const abs = join(dir, "files", ...rel.split("/"));
    const digest = await sha256File(abs);
    if (digest !== String(f.sha256).toLowerCase() && digest !== f.sha256) {
      mismatches.push(rel);
    }
  }
  if (mismatches.length) {
    throw new Error(`SHA-256 mismatch: ${mismatches.slice(0, 5).join(", ")}`);
  }
  return { ok: true, files: manifest.files.length, bytes: manifest.totals?.bytes ?? 0 };
}

/**
 * List complete backups under destRoot.
 * @param {string} destRoot
 */
export function listSdBackups(destRoot) {
  const root = resolve(destRoot);
  if (!existsSync(root)) return [];
  /** @type {Array<{ id: string, path: string, createdAt?: string, files?: number, bytes?: number, projectVersion?: string }>} */
  const out = [];
  for (const name of readdirSync(root).sort().reverse()) {
    const p = join(root, name);
    try {
      if (!statSync(p).isDirectory()) continue;
      if (existsSync(join(p, INCOMPLETE_MARKER))) continue;
      const manPath = join(p, "manifest.json");
      if (!existsSync(manPath)) continue;
      const man = JSON.parse(readFileSync(manPath, "utf8"));
      if (man.complete !== true) continue;
      out.push({
        id: name,
        path: p,
        createdAt: man.createdAt,
        files: man.totals?.files,
        bytes: man.totals?.bytes,
        projectVersion: man.projectVersion,
      });
    } catch {
      /* skip invalid */
    }
  }
  return out;
}

/**
 * @param {string} projectRoot
 * @param {{ backupDir: string, destPath: string, dryRun?: boolean, replaceEsp2?: boolean }} opts
 */
export async function restoreEsp2Sd(projectRoot, opts) {
  void projectRoot;
  const { dir, manifest } = loadAndValidateManifest(opts.backupDir);
  await verifyBackupHashes(dir);

  const { mountRoot, esp2Root } = resolveEsp2Root(opts.destPath, {
    allowMissingEsp2: true,
  });
  const vol = probeVolumeInfo(mountRoot);
  const needBytes = Number(manifest.totals?.bytes ?? 0);
  const free = vol.freeBytes;
  // If replacing existing esp2, free space may include those bytes — add them.
  let reclaim = 0;
  if (existsSync(esp2Root)) {
    for (const f of listFilesRecursive(esp2Root)) reclaim += f.size;
  }
  assertEnoughSpace(needBytes, free, reclaim);

  const plan = {
    backupDir: dir,
    destMount: mountRoot,
    destEsp2: esp2Root,
    filesToWrite: manifest.files.length,
    bytes: needBytes,
    staleToRemove: [],
    dryRun: Boolean(opts.dryRun),
    volume: vol,
  };

  const backupRelSet = new Set(manifest.files.map((f) => sanitizeRelativePath(f.path)));
  if (existsSync(esp2Root)) {
    for (const f of listFilesRecursive(esp2Root, SD_TREE_DIR)) {
      if (!backupRelSet.has(sanitizeRelativePath(f.rel))) {
        plan.staleToRemove.push(f.rel);
      }
    }
  }

  if (opts.dryRun) {
    return { ...plan, restored: false, verified: false };
  }

  // Stage into temporary tree then swap for clearer failure mode on FAT.
  const staging = join(mountRoot, `.esp2-restore-staging-${Date.now()}`);
  mkdirSync(staging, { recursive: true });
  try {
    for (const f of manifest.files) {
      const rel = sanitizeRelativePath(f.path);
      const from = join(dir, "files", ...rel.split("/"));
      // Write under staging/esp2/...
      const parts = rel.split("/");
      const destAbs = join(staging, ...parts);
      mkdirSync(dirname(destAbs), { recursive: true });
      copyFileSync(from, destAbs);
      const st = statSync(destAbs);
      if (st.size !== Number(f.size)) {
        throw new Error(`restore size mismatch: ${rel}`);
      }
      const digest = await sha256File(destAbs);
      if (digest !== f.sha256) {
        throw new Error(`restore SHA-256 mismatch while writing: ${rel}`);
      }
    }

    const stagedEsp2 = join(staging, SD_TREE_DIR);
    if (existsSync(esp2Root)) {
      const bak = join(mountRoot, `.esp2-old-${Date.now()}`);
      renameSync(esp2Root, bak);
      try {
        renameSync(stagedEsp2, esp2Root);
        rmSync(bak, { recursive: true, force: true });
      } catch (e) {
        // Best-effort rollback
        try {
          if (!existsSync(esp2Root) && existsSync(bak)) renameSync(bak, esp2Root);
        } catch {
          /* ignore */
        }
        throw e;
      }
    } else {
      renameSync(stagedEsp2, esp2Root);
    }
  } finally {
    if (existsSync(staging)) {
      rmSync(staging, { recursive: true, force: true });
    }
  }

  // Final verification against destination
  for (const f of manifest.files) {
    const rel = sanitizeRelativePath(f.path);
    const abs = join(mountRoot, ...rel.split("/"));
    if (!existsSync(abs)) throw new Error(`missing after restore: ${rel}`);
    const st = statSync(abs);
    if (st.size !== Number(f.size)) throw new Error(`size fail after restore: ${rel}`);
    const digest = await sha256File(abs);
    if (digest !== f.sha256) throw new Error(`SHA fail after restore: ${rel}`);
  }

  // Remove stale files left if rename kept nothing (already replaced whole tree).
  // Extra check: any file under esp2 not in manifest should not exist.
  for (const f of listFilesRecursive(esp2Root, SD_TREE_DIR)) {
    if (!backupRelSet.has(sanitizeRelativePath(f.rel))) {
      unlinkSync(f.abs);
    }
  }

  return {
    ...plan,
    restored: true,
    verified: true,
    filesRestored: manifest.files.length,
    bytesRestored: needBytes,
    staleRemoved: plan.staleToRemove.length,
  };
}

function parseArgs(argv) {
  const out = {
    cmd: "",
    source: "",
    destRoot: "",
    backup: "",
    dest: "",
    dryRun: false,
    help: false,
  };
  const rest = [...argv];
  if (rest[0] && !rest[0].startsWith("-")) out.cmd = rest.shift();
  for (let i = 0; i < rest.length; i++) {
    const a = rest[i];
    if (a === "--source") out.source = rest[++i];
    else if (a === "--dest-root") out.destRoot = rest[++i];
    else if (a === "--backup") out.backup = rest[++i];
    else if (a === "--dest") out.dest = rest[++i];
    else if (a === "--dry-run") out.dryRun = true;
    else if (a === "--help" || a === "-h") out.help = true;
  }
  return out;
}

async function main() {
  const args = parseArgs(process.argv.slice(2));
  const projectRoot = resolve(join(__dirname, "../.."));
  if (args.help || !args.cmd) {
    console.log(`Usage:
  node dev/tools/esp2-sd-backup.mjs backup --source <SD-mount> [--dest-root local/sd-backups]
  node dev/tools/esp2-sd-backup.mjs restore --backup <backup-dir> --dest <SD-mount> [--dry-run]
  node dev/tools/esp2-sd-backup.mjs list [--dest-root local/sd-backups]
  node dev/tools/esp2-sd-backup.mjs verify --backup <backup-dir>`);
    process.exit(args.help ? 0 : 2);
  }
  if (args.cmd === "backup") {
    const r = await backupEsp2Sd(projectRoot, {
      sourcePath: args.source,
      destRoot: args.destRoot || defaultSdBackupRoot(projectRoot),
    });
    console.log(
      JSON.stringify(
        {
          ok: true,
          backupDir: r.backupDir,
          files: r.manifest.totals.files,
          bytes: r.manifest.totals.bytes,
          filesystem: r.volume.filesystem,
          capacityBytes: r.volume.capacityBytes,
        },
        null,
        2,
      ),
    );
    return;
  }
  if (args.cmd === "list") {
    const root = args.destRoot || defaultSdBackupRoot(projectRoot);
    console.log(JSON.stringify(listSdBackups(root), null, 2));
    return;
  }
  if (args.cmd === "verify") {
    const r = await verifyBackupHashes(args.backup);
    console.log(JSON.stringify({ ok: true, ...r }, null, 2));
    return;
  }
  if (args.cmd === "restore") {
    const r = await restoreEsp2Sd(projectRoot, {
      backupDir: args.backup,
      destPath: args.dest,
      dryRun: args.dryRun,
    });
    console.log(JSON.stringify({ ok: true, ...r }, null, 2));
    return;
  }
  throw new Error(`unknown command: ${args.cmd}`);
}

const isMain =
  process.argv[1] && resolve(process.argv[1]).endsWith("esp2-sd-backup.mjs");
if (isMain) {
  main().catch((e) => {
    console.error("FAIL", e.message || e);
    process.exit(1);
  });
}
