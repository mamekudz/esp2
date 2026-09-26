/**
 * Paths for the gitignored local Apple II media root.
 *
 * Policy: all downloaded/runtime media lives under local/apple2/.
 * Tracked manifests stay in config/apple2/ and compatibility/tests/.
 */

import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { REDIS_STATUS } from "../apple2js/paths.mjs";

export { REDIS_STATUS };

export const ESP2_ROOT = join(dirname(fileURLToPath(import.meta.url)), "../../..");

/** Explicit local media root (gitignored except README). */
export const LOCAL_APPLE2_ROOT = join(ESP2_ROOT, "local", "apple2");
export const LOCAL_ROMS_DIR = join(LOCAL_APPLE2_ROOT, "roms");
export const LOCAL_DISKS_DIR = join(LOCAL_APPLE2_ROOT, "disks");
export const LOCAL_CACHE_DIR = join(LOCAL_APPLE2_ROOT, "cache");
export const LOCAL_APPLE2JS_CACHE_DIR = join(LOCAL_CACHE_DIR, "apple2js");
export const LOCAL_MANIFESTS_DIR = join(LOCAL_APPLE2_ROOT, "manifests");
export const LOCAL_USER_DIR = join(LOCAL_APPLE2_ROOT, "user");
export const LOCAL_LIBRARY_MANIFEST = join(
  LOCAL_MANIFESTS_DIR,
  "local-library.json",
);

export const TRACKED_ROMS_CATALOG = join(
  ESP2_ROOT,
  "config",
  "apple2",
  "roms-catalog.json",
);
export const TRACKED_MEDIA_CATALOG = join(
  ESP2_ROOT,
  "config",
  "apple2",
  "media-catalog.json",
);

export const LOCAL_MEDIA_KIND = Object.freeze({
  DOWNLOADED_CACHE: "downloaded_cache",
  GENERATED_RUNTIME: "generated_runtime",
  USER_SUPPLIED: "user_supplied",
  REDISTRIBUTABLE_ROM: "redistributable_rom",
});

export function ensureDir(path) {
  if (!existsSync(path)) {
    mkdirSync(path, { recursive: true });
  }
}

export function ensureLocalApple2Layout() {
  for (const d of [
    LOCAL_ROMS_DIR,
    LOCAL_DISKS_DIR,
    LOCAL_CACHE_DIR,
    LOCAL_APPLE2JS_CACHE_DIR,
    LOCAL_MANIFESTS_DIR,
    LOCAL_USER_DIR,
  ]) {
    ensureDir(d);
  }
}

export function readJson(path) {
  return JSON.parse(readFileSync(path, "utf8"));
}

export function writeJson(path, value) {
  ensureDir(dirname(path));
  writeFileSync(path, JSON.stringify(value, null, 2) + "\n", "utf8");
}

export function loadRomsCatalog() {
  return readJson(TRACKED_ROMS_CATALOG);
}

export function loadMediaCatalog() {
  return readJson(TRACKED_MEDIA_CATALOG);
}

export function loadLocalLibraryManifest() {
  if (!existsSync(LOCAL_LIBRARY_MANIFEST)) {
    return {
      schemaVersion: 1,
      updatedAt: null,
      roms: {},
      titles: {},
    };
  }
  return readJson(LOCAL_LIBRARY_MANIFEST);
}

export function saveLocalLibraryManifest(manifest) {
  manifest.updatedAt = new Date().toISOString();
  writeJson(LOCAL_LIBRARY_MANIFEST, manifest);
}

/**
 * Never upgrade provenance. Downloaded status must stay ≤ catalog status
 * in the sense that REDISTRIBUTABLE is only allowed if catalog says so.
 * @param {string} catalogStatus
 * @param {string} [incoming]
 */
export function retainProvenance(catalogStatus, incoming) {
  const allowed = new Set(Object.values(REDIS_STATUS));
  const base = allowed.has(catalogStatus)
    ? catalogStatus
    : REDIS_STATUS.UNKNOWN;
  if (!incoming || !allowed.has(incoming)) return base;
  // Never upgrade: if catalog is USER_SUPPLIED_ONLY, stay there even if
  // someone claims REDISTRIBUTABLE at download time.
  const rank = {
    [REDIS_STATUS.DO_NOT_DISTRIBUTE]: 0,
    [REDIS_STATUS.USER_SUPPLIED_ONLY]: 1,
    [REDIS_STATUS.UNKNOWN]: 2,
    [REDIS_STATUS.REDISTRIBUTABLE]: 3,
  };
  return rank[incoming] < rank[base] ? incoming : base;
}
