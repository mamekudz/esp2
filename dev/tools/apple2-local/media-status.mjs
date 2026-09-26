/**
 * apple2:media:status / clean helpers.
 */

import {
  existsSync,
  readdirSync,
  rmSync,
  statSync,
  unlinkSync,
} from "node:fs";
import { join } from "node:path";
import {
  LOCAL_CACHE_DIR,
  LOCAL_DISKS_DIR,
  LOCAL_USER_DIR,
  LOCAL_APPLE2JS_CACHE_DIR,
  loadLocalLibraryManifest,
  saveLocalLibraryManifest,
  ensureLocalApple2Layout,
} from "./paths.mjs";

function shortSha(h) {
  return h ? String(h).slice(0, 12) : "—";
}

function yn(v) {
  return v ? "yes" : "no";
}

/**
 * Compact status rows for console table.
 */
export function mediaStatusRows() {
  ensureLocalApple2Layout();
  const manifest = loadLocalLibraryManifest();
  const rows = [];
  for (const t of Object.values(manifest.titles || {})) {
    const downloaded = Boolean(t.sourcePath && existsSync(t.sourcePath));
    const prepared = Boolean(t.runtimePath && existsSync(t.runtimePath));
    rows.push({
      Title: t.title || t.id,
      Source: t.sourceFormat || t.catalogFilename || "—",
      Downloaded: yn(downloaded),
      "Runtime prepared": yn(prepared),
      Format: t.runtimeFormat || t.sourceType || "—",
      "SHA-256 short": shortSha(t.runtimeSha256 || t.sourceSha256),
      Provenance: t.provenance || "UNKNOWN",
      "Device status": t.deviceSyncedAt ? "synced" : "—",
    });
  }
  for (const r of Object.values(manifest.roms || {})) {
    rows.push({
      Title: `[ROM] ${r.id}`,
      Source: r.sourceUrl || "rom-catalog",
      Downloaded: yn(r.path && existsSync(r.path)),
      "Runtime prepared": yn(r.path && existsSync(r.path)),
      Format: "rom",
      "SHA-256 short": shortSha(r.sha256),
      Provenance: r.provenance || "UNKNOWN",
      "Device status": r.deviceSyncedAt ? "synced" : "—",
    });
  }
  return rows;
}

export function printMediaStatus(log = console.log) {
  const rows = mediaStatusRows();
  if (!rows.length) {
    log("[STATUS] no local titles/roms yet");
    return rows;
  }
  const cols = [
    "Title",
    "Source",
    "Downloaded",
    "Runtime prepared",
    "Format",
    "SHA-256 short",
    "Provenance",
    "Device status",
  ];
  const widths = cols.map((c) =>
    Math.max(c.length, ...rows.map((r) => String(r[c] ?? "").length)),
  );
  const line = (cells) =>
    cells.map((v, i) => String(v).padEnd(widths[i])).join("  ");
  log(line(cols));
  log(widths.map((w) => "-".repeat(w)).join("  "));
  for (const r of rows) log(line(cols.map((c) => r[c])));
  return rows;
}

/**
 * Safe clean:
 * - default: remove generated runtime under disks/ (+ clear runtime fields)
 * - --cache / purgeDownloads: also remove downloaded apple2js cache
 * - --user: also remove user-supplied (explicit)
 */
export function cleanLocalMedia(opts = {}) {
  const log = opts.log ?? console.log;
  ensureLocalApple2Layout();
  const manifest = loadLocalLibraryManifest();
  const removed = [];

  // Always clear generated runtime disks by default
  if (opts.runtime !== false) {
    if (existsSync(LOCAL_DISKS_DIR)) {
      for (const name of readdirSync(LOCAL_DISKS_DIR)) {
        const p = join(LOCAL_DISKS_DIR, name);
        if (statSync(p).isFile()) {
          unlinkSync(p);
          removed.push(p);
        }
      }
    }
    for (const t of Object.values(manifest.titles || {})) {
      delete t.runtimePath;
      delete t.runtimeRelative;
      delete t.runtimeFormat;
      delete t.runtimeSha256;
      delete t.runtimeSize;
      delete t.preparedAt;
      delete t.deviceSyncedAt;
      if (t.kind === "generated_runtime") t.kind = "downloaded_cache";
    }
    log(`[CLEAN] removed generated runtime under local/apple2/disks/ (${removed.length} files)`);
  }

  if (opts.cache || opts.purgeDownloads) {
    if (existsSync(LOCAL_APPLE2JS_CACHE_DIR)) {
      for (const name of readdirSync(LOCAL_APPLE2JS_CACHE_DIR)) {
        const p = join(LOCAL_APPLE2JS_CACHE_DIR, name);
        if (statSync(p).isFile()) {
          unlinkSync(p);
          removed.push(p);
        }
      }
    }
    for (const id of Object.keys(manifest.titles || {})) {
      const t = manifest.titles[id];
      if (t.kind !== "user_supplied") {
        delete manifest.titles[id];
      }
    }
    log("[CLEAN] removed downloaded cache under local/apple2/cache/apple2js/");
  }

  if (opts.user) {
    if (existsSync(LOCAL_USER_DIR)) {
      rmSync(LOCAL_USER_DIR, { recursive: true, force: true });
      removed.push(LOCAL_USER_DIR);
    }
    for (const id of Object.keys(manifest.titles || {})) {
      if (manifest.titles[id].kind === "user_supplied") {
        delete manifest.titles[id];
      }
    }
    log("[CLEAN] removed user-supplied media under local/apple2/user/");
  } else {
    // Explicitly preserve user/
    if (existsSync(LOCAL_USER_DIR)) {
      log("[CLEAN] preserved local/apple2/user/ (pass --user to delete)");
    }
  }

  saveLocalLibraryManifest(manifest);
  return { ok: true, removed };
}
