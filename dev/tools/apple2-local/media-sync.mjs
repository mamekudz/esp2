/**
 * apple2:media:sync — retrieve Apple ][js media into local/apple2/cache/.
 * Reuses apple2js catalog/audit infrastructure. Never upgrades provenance.
 */

import { createHash } from "node:crypto";
import { existsSync, writeFileSync, readFileSync } from "node:fs";
import { basename, join } from "node:path";
import { APPLE2JS_SITE_URL, REDIS_STATUS } from "../apple2js/paths.mjs";
import {
  loadGitCatalog,
  loadWebCatalog,
  mergeCatalogs,
} from "../apple2js/catalog.mjs";
import { auditCatalogEntry } from "../apple2js/audit.mjs";
import { syncApple2js } from "../apple2js/sync.mjs";
import {
  LOCAL_APPLE2JS_CACHE_DIR,
  LOCAL_CACHE_DIR,
  LOCAL_MANIFESTS_DIR,
  ensureLocalApple2Layout,
  loadMediaCatalog,
  loadLocalLibraryManifest,
  saveLocalLibraryManifest,
  retainProvenance,
  writeJson,
  ensureDir,
} from "./paths.mjs";
import { assertDestinationIgnored } from "./git-safety.mjs";

function sha256(buf) {
  return createHash("sha256").update(buf).digest("hex");
}

function slugFromEntry(entry) {
  const base = basename(entry.filename || "").replace(/\.json$/i, "");
  return (base || entry.name || "untitled").toLowerCase();
}

/**
 * Locate a title in merged apple2js catalog (git + website).
 * @param {string} titleQuery
 * @param {object[]} merged
 */
export function findCatalogTitle(titleQuery, merged) {
  const q = String(titleQuery || "")
    .trim()
    .toLowerCase();
  if (!q) return null;

  const tracked = loadMediaCatalog();
  const trackedHit = (tracked.titles || []).find(
    (t) =>
      t.id === q ||
      t.catalogId === q ||
      t.apple2jsBasename === q ||
      String(t.title).toLowerCase() === q,
  );

  const candidates = merged.filter((e) => {
    const id = slugFromEntry(e);
    const name = String(e.name || "").toLowerCase();
    return (
      id === q ||
      name === q ||
      name.includes(q) ||
      (trackedHit &&
        (id === trackedHit.apple2jsBasename ||
          e.filename === trackedHit.apple2jsFilename))
    );
  });

  if (candidates.length === 0) return null;
  const exact =
    candidates.find((e) => slugFromEntry(e) === q) ||
    candidates.find((e) => String(e.name || "").toLowerCase() === q) ||
    candidates[0];

  const audited = auditCatalogEntry(exact);
  let provenance = audited.redistribution?.status || REDIS_STATUS.UNKNOWN;
  if (trackedHit?.provenance) {
    provenance = retainProvenance(trackedHit.provenance, provenance);
  }

  return {
    id: trackedHit?.id || slugFromEntry(exact),
    title: trackedHit?.title || exact.name,
    entry: exact,
    audited,
    provenance,
    tracked: trackedHit || null,
    sourceUrl: `${APPLE2JS_SITE_URL}${exact.filename}`,
  };
}

async function fetchText(url, { timeoutMs = 90000 } = {}) {
  const ctrl = new AbortController();
  const t = setTimeout(() => ctrl.abort(), timeoutMs);
  try {
    const res = await fetch(url, { signal: ctrl.signal, redirect: "follow" });
    if (!res.ok) {
      throw new Error(
        `[APPLE2-LOCAL][NETWORK] HTTP ${res.status} fetching ${url}`,
      );
    }
    return await res.text();
  } catch (err) {
    if (err.name === "AbortError") {
      throw new Error(`[APPLE2-LOCAL][NETWORK] timeout fetching ${url}`);
    }
    if (
      err.cause?.code === "ENOTFOUND" ||
      err.code === "ENOTFOUND" ||
      /fetch failed|ECONNREFUSED|network/i.test(String(err.message || err))
    ) {
      throw new Error(
        `[APPLE2-LOCAL][NETWORK] cannot reach ${url}: ${err.message || err}`,
      );
    }
    throw err;
  } finally {
    clearTimeout(t);
  }
}

export function loadMergedCatalog({ requireCache = false } = {}) {
  let gitEntries = [];
  let webEntries = [];
  try {
    gitEntries = loadGitCatalog().entries;
  } catch (e) {
    if (requireCache) throw e;
  }
  try {
    webEntries = loadWebCatalog().entries;
  } catch (e) {
    if (requireCache) throw e;
  }
  if (!gitEntries.length && !webEntries.length) {
    throw new Error(
      "[APPLE2-LOCAL] No apple2js catalog. Run: gulp apple2js:sync (network)",
    );
  }
  return mergeCatalogs(gitEntries, webEntries);
}

/**
 * Download one title's original apple2js JSON into cache.
 */
export async function syncTitleMedia(titleQuery, opts = {}) {
  const log = opts.log ?? console.log;
  ensureLocalApple2Layout();
  assertDestinationIgnored(LOCAL_APPLE2JS_CACHE_DIR);

  if (opts.ensureApple2jsSync) {
    log("[MEDIA] ensuring apple2js catalog cache…");
    await syncApple2js({ fetchWebIndex: true });
  }

  const merged = loadMergedCatalog();
  const hit = findCatalogTitle(titleQuery, merged);
  if (!hit) {
    throw new Error(
      `[APPLE2-LOCAL] title not found in apple2js catalog: ${titleQuery}`,
    );
  }

  const destName = `${slugFromEntry(hit.entry)}.json`;
  const dest = join(LOCAL_APPLE2JS_CACHE_DIR, destName);
  const provenance = retainProvenance(hit.provenance);

  if (opts.dryRun) {
    log(`[MEDIA] dry-run ${hit.sourceUrl} -> ${dest}`);
    return { ok: true, dryRun: true, id: hit.id, dest, provenance };
  }

  let body;
  if (opts.offline && existsSync(dest)) {
    body = readFileSync(dest, "utf8");
    log(`[MEDIA] offline cache hit ${dest}`);
  } else if (existsSync(dest) && opts.skipIfPresent) {
    body = readFileSync(dest, "utf8");
    log(`[MEDIA] cache present ${dest}`);
  } else {
    log(`[MEDIA] fetching title=${hit.id} url=${hit.sourceUrl}`);
    body = await fetchText(hit.sourceUrl);
    writeFileSync(dest, body, "utf8");
  }

  const buf = Buffer.from(body, "utf8");
  const hash = sha256(buf);
  let meta = {};
  try {
    meta = JSON.parse(body);
  } catch {
    throw new Error(`[MEDIA][FAIL] invalid JSON for ${hit.id}`);
  }

  const manifest = loadLocalLibraryManifest();
  const prev = manifest.titles[hit.id] || {};
  const record = {
    ...prev,
    id: hit.id,
    title: hit.title,
    catalogFilename: hit.entry.filename,
    sourceUrl: hit.sourceUrl,
    sourcePath: dest,
    sourceRelative: `local/apple2/cache/apple2js/${destName}`,
    sourceFormat: "apple2js-json",
    sourceType: typeof meta.type === "string" ? meta.type.toLowerCase() : null,
    sourceSha256: hash,
    sourceSize: buf.length,
    provenance: retainProvenance(provenance, prev.provenance),
    evidence:
      hit.tracked?.evidence ||
      hit.audited?.redistribution?.evidence ||
      "LOCAL_TEST_ONLY",
    localTestOnly: true,
    downloadedAt: new Date().toISOString(),
    kind: "downloaded_cache",
    defaultRomId: hit.tracked?.defaultRomId || "appleiigo",
    slot6Mode: hit.tracked?.slot6Mode || "cleanroom",
    deviceDiskName: hit.tracked?.deviceDiskName || null,
    devicePath: hit.tracked?.devicePath || null,
  };
  manifest.titles[hit.id] = record;
  saveLocalLibraryManifest(manifest);

  log(
    `[MEDIA] OK title=${hit.id} type=${record.sourceType} sha256=${hash.slice(0, 12)}… provenance=${record.provenance}`,
  );
  return { ok: true, id: hit.id, record, dest };
}

/**
 * Sync all website/git catalog titles into local cache (LOCAL_TEST_ONLY).
 */
export async function syncAllMedia(opts = {}) {
  const log = opts.log ?? console.log;
  ensureLocalApple2Layout();
  assertDestinationIgnored(LOCAL_CACHE_DIR);

  if (opts.ensureApple2jsSync !== false) {
    log("[MEDIA] apple2js:sync before --all…");
    await syncApple2js({ fetchWebIndex: true });
  }

  const merged = loadMergedCatalog({ requireCache: true });
  const counts = {
    total: merged.length,
    ok: 0,
    failed: 0,
    byProvenance: {},
  };
  const failures = [];

  for (const entry of merged) {
    const id = slugFromEntry(entry);
    try {
      const r = await syncTitleMedia(id, {
        log,
        ensureApple2jsSync: false,
        dryRun: opts.dryRun,
        offline: opts.offline,
        skipIfPresent: opts.skipIfPresent,
      });
      counts.ok += 1;
      const p = r.record?.provenance || r.provenance || "UNKNOWN";
      counts.byProvenance[p] = (counts.byProvenance[p] || 0) + 1;
    } catch (err) {
      counts.failed += 1;
      failures.push({ id, error: String(err.message || err) });
      log(`[MEDIA][FAIL] title=${id} ${err.message || err}`);
    }
  }

  ensureDir(LOCAL_MANIFESTS_DIR);
  writeJson(join(LOCAL_MANIFESTS_DIR, "sync-all-summary.json"), {
    generatedAt: new Date().toISOString(),
    counts,
    failures,
    note: "LOCAL_TEST_ONLY — website availability ≠ redistribution permission",
  });

  return { ok: counts.failed === 0, counts, failures };
}

export function listCachedTitles() {
  const manifest = loadLocalLibraryManifest();
  return Object.values(manifest.titles || {});
}

export function auditLocalMedia() {
  const titles = listCachedTitles();
  const byProv = {};
  for (const t of titles) {
    const p = t.provenance || REDIS_STATUS.UNKNOWN;
    byProv[p] = (byProv[p] || 0) + 1;
  }
  return {
    titleCount: titles.length,
    byProvenance: byProv,
    titles: titles.map((t) => ({
      id: t.id,
      title: t.title,
      provenance: t.provenance,
      sourceType: t.sourceType,
      sourceSha256: t.sourceSha256,
      runtimePath: t.runtimePath || null,
      runtimeSha256: t.runtimeSha256 || null,
      localTestOnly: t.localTestOnly !== false,
    })),
  };
}
