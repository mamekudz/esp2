/**
 * Paths and constants for apple2js developer tooling.
 * Cache lives under .cache/ (gitignored) — never assumed present at build time.
 */

import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

export const ESP2_ROOT = join(dirname(fileURLToPath(import.meta.url)), "../../..");

export const APPLE2JS_REPO_URL = "https://github.com/whscullin/apple2js";
export const APPLE2JS_SITE_URL = "https://www.scullinsteel.com/apple2/";
export const APPLE2JS_SITE_INDEX_URL =
  "https://www.scullinsteel.com/apple2/json/disks/index.json";

export const CACHE_DIR = join(ESP2_ROOT, ".cache");
export const APPLE2JS_CACHE_DIR = join(CACHE_DIR, "apple2js");
export const APPLE2JS_META_DIR = join(CACHE_DIR, "apple2js-meta");
export const PIN_FILE = join(APPLE2JS_META_DIR, "upstream-pin.json");
export const WEB_INDEX_CACHE = join(APPLE2JS_META_DIR, "web-index.json");

export const REDIS_STATUS = Object.freeze({
  REDISTRIBUTABLE: "REDISTRIBUTABLE",
  USER_SUPPLIED_ONLY: "USER_SUPPLIED_ONLY",
  UNKNOWN: "UNKNOWN",
  DO_NOT_DISTRIBUTE: "DO_NOT_DISTRIBUTE",
});

export function ensureDir(path) {
  if (!existsSync(path)) {
    mkdirSync(path, { recursive: true });
  }
}

export function readJson(path) {
  return JSON.parse(readFileSync(path, "utf8"));
}

export function writeJson(path, value) {
  ensureDir(dirname(path));
  writeFileSync(path, JSON.stringify(value, null, 2) + "\n", "utf8");
}

export function slugifyTitle(name) {
  return String(name || "untitled")
    .normalize("NFKD")
    .replace(/[^\w\s-]/g, "")
    .trim()
    .replace(/[\s_]+/g, "-")
    .replace(/-+/g, "-")
    .toLowerCase() || "untitled";
}
