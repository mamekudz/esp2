/**
 * µGulp readiness helpers for local Apple II media (read-only).
 *
 * Guides the next acquisition step via µAttention / RevealTask.
 * Missing optional media is never a project-wide error.
 */

import { existsSync } from "node:fs";
import {
  LOCAL_LIBRARY_MANIFEST,
  LOCAL_ROMS_DIR,
  loadLocalLibraryManifest,
  loadRomsCatalog,
} from "./paths.mjs";

/** Default first title for the guided media path (LOCAL_TEST_ONLY). */
export const DEFAULT_GUIDE_TITLE = "galaxian";

/**
 * @typedef {'rom_sync'|'media_sync'|'media_prepare'|'compat'|'device_sync'|'ready'|null} ReadinessStep
 */

/**
 * Inspect gitignored local/apple2 state without mutating it.
 * @param {{ title?: string, manifest?: object|null }} [_opts]
 * @returns {{
 *   step: ReadinessStep,
 *   emphasizeTaskId: string|null,
 *   title: string,
 *   freeRomReady: boolean,
 *   titleCached: boolean,
 *   titlePrepared: boolean,
 *   titleCompatHint: boolean,
 *   titleDeviceSynced: boolean,
 * }}
 */
export function computeApple2MediaReadiness(_opts = {}) {
  const title = String(_opts.title || DEFAULT_GUIDE_TITLE).toLowerCase();
  const manifestProvided = Object.prototype.hasOwnProperty.call(_opts, "manifest");
  let manifest = _opts.manifest;
  if (!manifestProvided) {
    try {
      manifest = existsSync(LOCAL_LIBRARY_MANIFEST)
        ? loadLocalLibraryManifest()
        : { roms: {}, titles: {} };
    } catch {
      manifest = { roms: {}, titles: {} };
    }
  }
  if (!manifest) manifest = { roms: {}, titles: {} };

  let freeRomReady = false;
  try {
    const catalog = loadRomsCatalog();
    const redis = (catalog.roms || []).filter(
      (r) => r.provenance === "REDISTRIBUTABLE" && r.autoDownload !== false,
    );
    freeRomReady = redis.some((r) => {
      const local = manifest.roms?.[r.id];
      // When a test/injected manifest is provided, do not fall back to the
      // real local/apple2 tree — that would make readiness non-deterministic.
      const path =
        local?.path ||
        (!manifestProvided && r.id
          ? `${LOCAL_ROMS_DIR}/${r.filename || `${r.id}.rom`}`
          : null);
      return Boolean(path && existsSync(path));
    });
  } catch {
    freeRomReady = Object.values(manifest.roms || {}).some(
      (r) => r.path && existsSync(r.path),
    );
  }

  const entry = manifest.titles?.[title] || null;
  const titleCached = Boolean(
    entry && entry.sourcePath && existsSync(entry.sourcePath),
  );
  const titlePrepared = Boolean(
    entry && entry.runtimePath && existsSync(entry.runtimePath),
  );
  const titleCompatHint = Boolean(entry?.hostCompatPass);
  const titleDeviceSynced = Boolean(
    entry?.deviceSyncedAt && entry?.deviceSyncedPath,
  );

  /** @type {ReadinessStep} */
  let step = null;
  /** @type {string|null} */
  let emphasizeTaskId = null;

  if (!freeRomReady) {
    step = "rom_sync";
    emphasizeTaskId = "apple2:rom:sync";
  } else if (!titleCached) {
    step = "media_sync";
    emphasizeTaskId = "apple2:media:sync";
  } else if (!titlePrepared) {
    step = "media_prepare";
    emphasizeTaskId = "apple2:media:prepare";
  } else if (!titleCompatHint) {
    step = "compat";
    emphasizeTaskId = "apple2:compat";
  } else if (!titleDeviceSynced) {
    step = "device_sync";
    emphasizeTaskId = "apple2:device:sync";
  } else {
    step = "ready";
    emphasizeTaskId = null;
  }

  return {
    step,
    emphasizeTaskId,
    title,
    freeRomReady,
    titleCached,
    titlePrepared,
    titleCompatHint,
    titleDeviceSynced,
  };
}

/**
 * Bind µAttention on the guided Apple II tasks.
 * @param {Record<string, Function>} tasks map of gulpName → task fn
 */
export function bindApple2ReadinessAttention(tasks) {
  const ids = [
    "apple2:rom:sync",
    "apple2:media:sync",
    "apple2:media:prepare",
    "apple2:compat",
    "apple2:device:sync",
  ];
  /** Early media steps are owned by setup:first-run attention. */
  const firstRunSteps = new Set(["rom_sync", "media_sync", "media_prepare"]);
  for (const id of ids) {
    const fn = tasks[id];
    if (!fn) continue;
    fn["\u00b5Attention"] = () => {
      const r = computeApple2MediaReadiness();
      if (firstRunSteps.has(r.step)) return false;
      return r.emphasizeTaskId === id;
    };
    const tip =
      'Next Apple II media readiness step.<context="µAttentionTooltip"/>';
    fn["\u00b5AttentionTooltip"] =
      typeof tip.i18xRegister === "function" ? tip.i18xRegister() : tip;
  }
}
