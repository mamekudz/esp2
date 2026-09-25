/**
 * Offline media identify / import helpers (host / developer).
 * Never downloads commercial software.
 */

import { createHash } from "node:crypto";
import {
  copyFileSync,
  existsSync,
  mkdirSync,
  readFileSync,
  writeFileSync,
} from "node:fs";
import { basename, dirname, extname, join } from "node:path";

export const MEDIA_FORMAT = Object.freeze({
  Unknown: "unknown",
  Dsk: "dsk",
  Po: "po",
  Nib: "nib",
  Woz: "woz",
});

const DOS33 = 143360;
const NIB_35 = 35 * 0x1a00;

/**
 * @param {string} path
 * @param {Buffer} [buf]
 */
export function detectMediaFormat(path, buf) {
  const ext = extname(path).toLowerCase();
  if (ext === ".dsk") return MEDIA_FORMAT.Dsk;
  if (ext === ".po") return MEDIA_FORMAT.Po;
  if (ext === ".nib") return MEDIA_FORMAT.Nib;
  if (ext === ".woz") return MEDIA_FORMAT.Woz;
  if (buf) {
    if (buf.length === DOS33) return MEDIA_FORMAT.Dsk;
    if (buf.length === NIB_35) return MEDIA_FORMAT.Nib;
    if (buf.length >= 12 && buf.subarray(0, 4).toString("ascii") === "WOZ2") {
      return MEDIA_FORMAT.Woz;
    }
    if (buf.length >= 12 && buf.subarray(0, 4).toString("ascii") === "WOZ1") {
      return MEDIA_FORMAT.Woz;
    }
  }
  return MEDIA_FORMAT.Unknown;
}

/**
 * @param {Buffer|Uint8Array} buf
 */
export function validateBasicMedia(format, buf) {
  const errors = [];
  if (!buf || buf.length === 0) {
    errors.push("empty");
    return { ok: false, errors };
  }
  switch (format) {
    case MEDIA_FORMAT.Dsk:
    case MEDIA_FORMAT.Po:
      if (buf.length !== DOS33) {
        errors.push(`expected ${DOS33} bytes for dsk/po, got ${buf.length}`);
      }
      break;
    case MEDIA_FORMAT.Nib:
      if (buf.length !== NIB_35) {
        errors.push(`expected ${NIB_35} bytes for 35-track nib, got ${buf.length}`);
      }
      break;
    case MEDIA_FORMAT.Woz:
      if (buf.length < 12) errors.push("woz too short");
      else {
        const mag = Buffer.from(buf.subarray(0, 4)).toString("ascii");
        if (mag !== "WOZ1" && mag !== "WOZ2") {
          errors.push(`bad woz magic ${mag}`);
        }
      }
      break;
    default:
      errors.push("unknown format");
  }
  return { ok: errors.length === 0, errors };
}

export function sha256FileBuffer(buf) {
  return createHash("sha256").update(buf).digest("hex");
}

/**
 * @param {string} dbPath known-hashes.json
 * @param {string} sha256
 */
export function lookupKnownHash(dbPath, sha256) {
  if (!existsSync(dbPath)) return null;
  const db = JSON.parse(readFileSync(dbPath, "utf8"));
  const hit = (db.entries || []).find(
    (e) => e.sha256 === sha256.toLowerCase(),
  );
  return hit || null;
}

/**
 * Copy user-supplied media into a library game directory. Offline only.
 *
 * @param {{
 *   sourcePath: string,
 *   libraryRoot: string,
 *   gameId: string,
 *   destFileName?: string,
 *   hashDbPath?: string,
 * }} opts
 */
export function importUserMedia(opts) {
  const buf = readFileSync(opts.sourcePath);
  const format = detectMediaFormat(opts.sourcePath, buf);
  const validation = validateBasicMedia(format, buf);
  if (!validation.ok) {
    return {
      ok: false,
      format,
      errors: validation.errors,
    };
  }
  const sha256 = sha256FileBuffer(buf);
  const known = opts.hashDbPath
    ? lookupKnownHash(opts.hashDbPath, sha256)
    : null;

  const destName =
    opts.destFileName ||
    basename(opts.sourcePath).replace(/[^\w.\-]+/g, "_");
  const destDir = join(opts.libraryRoot, opts.gameId);
  mkdirSync(destDir, { recursive: true });
  const destPath = join(destDir, destName);
  copyFileSync(opts.sourcePath, destPath);

  return {
    ok: true,
    format,
    sha256,
    destPath,
    knownMatch: known,
    mediaInstalled: true,
  };
}

/**
 * Update game.json mediaInstalled flags when files appear/disappear.
 */
export function refreshMediaInstalledFlags(gameJsonPath, gameDir) {
  const raw = JSON.parse(readFileSync(gameJsonPath, "utf8"));
  let any = false;
  for (const d of raw.disks || []) {
    const p = join(gameDir, d.file);
    const installed = existsSync(p);
    d.mediaInstalled = installed;
    if (installed) any = true;
  }
  raw.mediaStatus = any ? "installed" : "not_installed";
  writeFileSync(gameJsonPath, JSON.stringify(raw, null, 2) + "\n", "utf8");
  return raw;
}

export function writeGameJson(path, meta) {
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, JSON.stringify(meta, null, 2) + "\n", "utf8");
}
