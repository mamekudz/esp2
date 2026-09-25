/**
 * Polished source-independent media identify / import workflow.
 * Never downloads commercial images because metadata matched.
 */

import { createHash } from "node:crypto";
import {
  copyFileSync,
  existsSync,
  mkdirSync,
  readdirSync,
  readFileSync,
  statSync,
  writeFileSync,
} from "node:fs";
import { basename, dirname, extname, join, resolve } from "node:path";
import {
  detectMediaFormat,
  formatSupport,
  hasSupportLevel,
  validateBasicMedia,
  MEDIA_FORMAT,
  SUPPORT_LEVEL,
} from "./formats.mjs";
import { mayAutoFetchMedia } from "../catalog/providers.mjs";
import { REDIS_STATUS, slugifyTitle } from "../apple2js/paths.mjs";

export { MEDIA_FORMAT, SUPPORT_LEVEL } from "./formats.mjs";
export {
  detectMediaFormat,
  validateBasicMedia,
  formatSupport,
  hasSupportLevel,
} from "./formats.mjs";

export function sha256FileBuffer(buf) {
  return createHash("sha256").update(buf).digest("hex");
}

export function lookupKnownHash(dbPath, sha256) {
  if (!existsSync(dbPath)) return null;
  const db = JSON.parse(readFileSync(dbPath, "utf8"));
  const hit = (db.entries || []).find(
    (e) => e.sha256 === sha256.toLowerCase(),
  );
  return hit || null;
}

/**
 * Scan library root for existing media by SHA-256 (duplicate detection).
 * Structure: libraryRoot/<gameId>/*.{dsk,po,...}
 */
export function findDuplicatesByHash(libraryRoot, sha256) {
  const hits = [];
  if (!existsSync(libraryRoot)) return hits;
  const want = sha256.toLowerCase();

  function scanGameDir(gameId, dir) {
    if (!existsSync(dir) || !statSync(dir).isDirectory()) return;
    for (const f of readdirSync(dir)) {
      const p = join(dir, f);
      if (!statSync(p).isFile()) continue;
      const ext = extname(f).toLowerCase();
      if (
        ![".dsk", ".do", ".po", ".nib", ".woz", ".2mg", ".hdv", ".bin"].includes(
          ext,
        )
      ) {
        continue;
      }
      try {
        const buf = readFileSync(p);
        if (sha256FileBuffer(buf) === want) {
          hits.push({ gameId, path: p, file: f });
        }
      } catch {
        /* skip */
      }
    }
  }

  const gamesDir = join(libraryRoot, "games");
  if (existsSync(gamesDir) && statSync(gamesDir).isDirectory()) {
    for (const gameId of readdirSync(gamesDir)) {
      scanGameDir(gameId, join(gamesDir, gameId));
    }
  } else {
    for (const gameId of readdirSync(libraryRoot)) {
      scanGameDir(gameId, join(libraryRoot, gameId));
    }
  }
  return hits;
}

/**
 * Identify a file without copying.
 * @param {{ path: string, catalogHub?: object, hashDbPath?: string, libraryRoot?: string }} opts
 */
export function identifyMedia(opts) {
  const abs = resolve(opts.path);
  if (!existsSync(abs)) {
    return { ok: false, errors: [`not found: ${abs}`] };
  }
  const st = statSync(abs);
  if (!st.isFile()) {
    return { ok: false, errors: ["path is not a file"] };
  }
  const buf = readFileSync(abs);
  const format = detectMediaFormat(abs, buf);
  const validation = validateBasicMedia(format, buf);
  const sha256 = sha256FileBuffer(buf);
  const support = formatSupport(format);

  /** @type {object[]} */
  let catalogMatches = [];
  if (opts.catalogHub) {
    catalogMatches = [
      ...opts.catalogHub.findByHash(sha256),
      ...opts.catalogHub.findByTitle(basename(abs, extname(abs))),
    ];
    // de-dupe by providerId+id
    const seen = new Set();
    catalogMatches = catalogMatches.filter((m) => {
      const k = `${m.providerId}:${m.id}`;
      if (seen.has(k)) return false;
      seen.add(k);
      return true;
    });
  }
  if (opts.hashDbPath) {
    const known = lookupKnownHash(opts.hashDbPath, sha256);
    if (known) {
      catalogMatches.push({
        providerId: "known-hashes",
        id: known.id,
        title: known.title,
        redistribution: {
          status:
            known.provenance?.redistributionStatus || REDIS_STATUS.UNKNOWN,
          evidence: known.provenance?.evidence,
        },
        raw: known,
      });
    }
  }

  const duplicates = opts.libraryRoot
    ? findDuplicatesByHash(opts.libraryRoot, sha256)
    : [];

  return {
    ok: validation.ok,
    path: abs,
    filename: basename(abs),
    size: buf.length,
    format,
    sha256,
    validation,
    support,
    importRecognized: hasSupportLevel(format, SUPPORT_LEVEL.ImportRecognized),
    parserSupported: hasSupportLevel(format, SUPPORT_LEVEL.ParserSupported),
    emulatorSupported: hasSupportLevel(format, SUPPORT_LEVEL.EmulatorSupported),
    catalogMatches,
    duplicates,
    // Explicit: matching metadata never implies download
    wouldAutoDownload: false,
  };
}

function pickGameId(opts, identify) {
  if (opts.gameId) return slugifyTitle(opts.gameId);
  if (opts.title) return slugifyTitle(opts.title);
  const match = identify.catalogMatches?.[0];
  if (match?.id) return match.id;
  return slugifyTitle(basename(identify.filename, extname(identify.filename)));
}

function pickTitle(opts, identify, gameId) {
  if (opts.title) return opts.title;
  const match = identify.catalogMatches?.[0];
  if (match?.title) return match.title;
  return gameId;
}

function mergeRedistribution(identify, opts) {
  if (opts.redistribution) return opts.redistribution;
  const match = identify.catalogMatches?.[0];
  if (match?.redistribution) return match.redistribution;
  return {
    status: REDIS_STATUS.USER_SUPPLIED_ONLY,
    evidence: "User-supplied media import; no catalog match with rights evidence.",
  };
}

/**
 * Import one disk into library. Never auto-downloads.
 *
 * @param {{
 *   sourcePath: string,
 *   libraryRoot: string,
 *   gameId?: string,
 *   title?: string,
 *   diskLabel?: string,
 *   destFileName?: string,
 *   catalogHub?: object,
 *   hashDbPath?: string,
 *   dryRun?: boolean,
 *   allowDuplicateCopy?: boolean,
 * }} opts
 */
export function importMedia(opts) {
  const identify = identifyMedia({
    path: opts.sourcePath,
    catalogHub: opts.catalogHub,
    hashDbPath: opts.hashDbPath,
    libraryRoot: opts.libraryRoot,
  });
  if (!identify.ok && identify.errors) {
    return { ok: false, identify, errors: identify.errors };
  }
  if (!identify.importRecognized) {
    return {
      ok: false,
      identify,
      errors: [`format ${identify.format} not IMPORT_RECOGNIZED`],
    };
  }
  if (!identify.validation.ok) {
    return {
      ok: false,
      identify,
      errors: identify.validation.errors,
    };
  }

  const redistribution = mergeRedistribution(identify, opts);
  if (mayAutoFetchMedia(redistribution.status) === false) {
    // Always true path for user file import — we only copy the local file.
    // Gate documents that we must never fetch remote media for this status.
  }
  // Defense: refuse if caller tried to set a download URL
  if (opts.downloadUrl) {
    return {
      ok: false,
      identify,
      errors: [
        "automatic media download is not supported; provide a local --file",
      ],
      redistribution,
    };
  }

  if (identify.duplicates.length && !opts.allowDuplicateCopy) {
    return {
      ok: false,
      identify,
      duplicate: true,
      errors: [
        `duplicate SHA-256 already in library (${identify.duplicates[0].gameId}/${identify.duplicates[0].file})`,
      ],
      existing: identify.duplicates,
      redistribution,
    };
  }

  const gameId = pickGameId(opts, identify);
  const title = pickTitle(opts, identify, gameId);
  const destName =
    opts.destFileName ||
    basename(identify.path).replace(/[^\w.\-]+/g, "_");
  const gamesRoot = join(opts.libraryRoot, "games");
  // Support both libraryRoot/games/<id> and libraryRoot/<id>
  const useGamesSubdir =
    existsSync(join(opts.libraryRoot, "games")) ||
    opts.layout === "apple2" ||
    opts.preferGamesSubdir !== false;
  const destDir = useGamesSubdir
    ? join(opts.libraryRoot, "games", gameId)
    : join(opts.libraryRoot, gameId);
  const destPath = join(destDir, destName);
  const gameJsonPath = join(destDir, "game.json");

  const diskLabel = opts.diskLabel || guessDiskLabel(destName);

  /** @type {object} */
  let meta;
  if (existsSync(gameJsonPath)) {
    meta = JSON.parse(readFileSync(gameJsonPath, "utf8"));
    const disks = Array.isArray(meta.disks) ? meta.disks : [];
    const existing = disks.find((d) => d.file === destName);
    if (!existing) {
      disks.push({
        file: destName,
        label: diskLabel,
        writeProtected: true,
        mediaInstalled: true,
        sha256: identify.sha256,
      });
    } else {
      existing.mediaInstalled = true;
      existing.sha256 = identify.sha256;
    }
    meta.disks = disks;
    meta.mediaStatus = "installed";
    meta.title = meta.title || title;
    meta.id = meta.id || gameId;
    if (!meta.bootDisk) meta.bootDisk = disks[0]?.file;
    if (!meta.defaultDrive1) meta.defaultDrive1 = disks[0]?.file;
    mergeCatalogSources(meta, identify.catalogMatches);
    if (!meta.redistribution) meta.redistribution = redistribution;
  } else {
    meta = {
      schemaVersion: 1,
      id: gameId,
      title,
      disks: [
        {
          file: destName,
          label: diskLabel,
          writeProtected: true,
          mediaInstalled: true,
          sha256: identify.sha256,
        },
      ],
      bootDisk: destName,
      defaultDrive1: destName,
      defaultDrive2: null,
      mediaStatus: "installed",
      redistribution,
      sourceMatches: identify.catalogMatches.map(simplifyMatch),
    };
  }

  if (opts.dryRun) {
    return {
      ok: true,
      dryRun: true,
      identify,
      gameId,
      destPath,
      gameJsonPath,
      meta,
      action: existsSync(destPath) ? "would_overwrite_media" : "would_copy",
    };
  }

  mkdirSync(destDir, { recursive: true });
  copyFileSync(identify.path, destPath);
  writeFileSync(gameJsonPath, JSON.stringify(meta, null, 2) + "\n", "utf8");

  return {
    ok: true,
    dryRun: false,
    identify,
    gameId,
    destPath,
    gameJsonPath,
    meta,
    mediaInstalled: true,
  };
}

function guessDiskLabel(filename) {
  const n = filename.toLowerCase();
  if (/disk\s*2|disk2|side\s*b|sideb/.test(n)) return "disk2";
  if (/disk\s*1|disk1|side\s*a|sidea/.test(n)) return "disk1";
  return "disk1";
}

function simplifyMatch(m) {
  return {
    providerId: m.providerId,
    id: m.id,
    title: m.title,
    redistributionStatus: m.redistribution?.status,
  };
}

function mergeCatalogSources(meta, matches) {
  const prev = Array.isArray(meta.sourceMatches) ? meta.sourceMatches : [];
  const map = new Map(prev.map((m) => [`${m.providerId}:${m.id}`, m]));
  for (const m of matches || []) {
    map.set(`${m.providerId}:${m.id}`, simplifyMatch(m));
  }
  meta.sourceMatches = [...map.values()];
}

/**
 * Non-recursive directory import: only immediate files with disk extensions.
 */
export function listImportableFilesInDirectory(dirPath) {
  const abs = resolve(dirPath);
  if (!existsSync(abs) || !statSync(abs).isDirectory()) {
    throw new Error(`not a directory: ${abs}`);
  }
  const exts = new Set([".dsk", ".do", ".po", ".nib", ".woz", ".2mg", ".hdv"]);
  return readdirSync(abs)
    .map((n) => join(abs, n))
    .filter((p) => {
      try {
        return statSync(p).isFile() && exts.has(extname(p).toLowerCase());
      } catch {
        return false;
      }
    });
}

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

/** Back-compat thin wrapper used by older tests/CLI. */
export function importUserMedia(opts) {
  return importMedia({
    sourcePath: opts.sourcePath,
    libraryRoot: opts.libraryRoot,
    gameId: opts.gameId,
    destFileName: opts.destFileName,
    hashDbPath: opts.hashDbPath,
    preferGamesSubdir: false,
  });
}
