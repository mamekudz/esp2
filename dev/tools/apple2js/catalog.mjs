/**
 * Parse apple2js disk catalog indexes (git and/or website).
 * Does not load disk media blobs.
 */

import { existsSync } from "node:fs";
import { join } from "node:path";
import {
  APPLE2JS_CACHE_DIR,
  PIN_FILE,
  WEB_INDEX_CACHE,
  readJson,
} from "./paths.mjs";

/**
 * @typedef {object} Apple2jsCatalogEntry
 * @property {string} filename
 * @property {string} name
 * @property {string} category
 * @property {boolean} [e]  // Apple //e oriented
 * @property {string} [disk]
 * @property {"git"|"website"|"synthetic"} catalogSource
 */

/**
 * @param {unknown} raw
 * @param {"git"|"website"|"synthetic"} catalogSource
 * @returns {Apple2jsCatalogEntry[]}
 */
export function parseApple2jsIndex(raw, catalogSource) {
  if (!Array.isArray(raw)) {
    throw new Error("apple2js index must be a JSON array");
  }
  /** @type {Apple2jsCatalogEntry[]} */
  const out = [];
  for (let i = 0; i < raw.length; i++) {
    const e = raw[i];
    if (e == null || typeof e !== "object") {
      throw new Error(`apple2js index[${i}]: expected object`);
    }
    const filename = e.filename;
    const name = e.name;
    const category = e.category;
    if (typeof filename !== "string" || !filename) {
      throw new Error(`apple2js index[${i}]: missing filename`);
    }
    if (typeof name !== "string" || !name) {
      throw new Error(`apple2js index[${i}]: missing name`);
    }
    if (typeof category !== "string" || !category) {
      throw new Error(`apple2js index[${i}]: missing category`);
    }
    /** @type {Apple2jsCatalogEntry} */
    const entry = {
      filename,
      name,
      category,
      catalogSource,
    };
    if (typeof e.e === "boolean") entry.e = e.e;
    if (typeof e.disk === "string") entry.disk = e.disk;
    out.push(entry);
  }
  return out;
}

export function loadGitCatalog() {
  const indexPath = join(APPLE2JS_CACHE_DIR, "json", "disks", "index.json");
  if (!existsSync(indexPath)) {
    throw new Error(
      `Missing ${indexPath}. Run: gulp apple2js:sync (network)`,
    );
  }
  const pin = existsSync(PIN_FILE) ? readJson(PIN_FILE) : null;
  const entries = parseApple2jsIndex(readJson(indexPath), "git");
  return { pin, entries, indexPath };
}

export function loadWebCatalog() {
  if (!existsSync(WEB_INDEX_CACHE)) {
    throw new Error(
      `Missing ${WEB_INDEX_CACHE}. Run: gulp apple2js:sync (network)`,
    );
  }
  const cached = readJson(WEB_INDEX_CACHE);
  const entries = parseApple2jsIndex(cached.entries, "website");
  return {
    sourceUrl: cached.sourceUrl,
    fetchedAt: cached.fetchedAt,
    entries,
  };
}

/**
 * Merge git + website catalogs by filename. Website fills gaps not in git.
 */
export function mergeCatalogs(gitEntries, webEntries) {
  const byFile = new Map();
  for (const e of gitEntries) {
    byFile.set(e.filename, { ...e, inGitRepo: true, onWebsite: false });
  }
  for (const e of webEntries) {
    const prev = byFile.get(e.filename);
    if (prev) {
      byFile.set(e.filename, {
        ...prev,
        ...e,
        catalogSource: prev.catalogSource,
        inGitRepo: true,
        onWebsite: true,
      });
    } else {
      byFile.set(e.filename, {
        ...e,
        inGitRepo: false,
        onWebsite: true,
      });
    }
  }
  return [...byFile.values()].sort((a, b) => {
    const c = a.category.localeCompare(b.category);
    return c !== 0 ? c : a.name.localeCompare(b.name);
  });
}
