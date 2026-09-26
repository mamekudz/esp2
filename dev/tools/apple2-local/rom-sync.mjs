/**
 * apple2:rom:sync — download ONLY redistributable ROMs from tracked catalog.
 */

import { createHash } from "node:crypto";
import { existsSync, writeFileSync, readFileSync } from "node:fs";
import { join } from "node:path";
import {
  LOCAL_ROMS_DIR,
  ensureLocalApple2Layout,
  loadRomsCatalog,
  loadLocalLibraryManifest,
  saveLocalLibraryManifest,
  REDIS_STATUS,
  retainProvenance,
} from "./paths.mjs";
import { assertDestinationIgnored } from "./git-safety.mjs";

async function fetchBinary(url, { timeoutMs = 60000 } = {}) {
  const ctrl = new AbortController();
  const t = setTimeout(() => ctrl.abort(), timeoutMs);
  try {
    const res = await fetch(url, { signal: ctrl.signal, redirect: "follow" });
    if (!res.ok) {
      throw new Error(
        `[APPLE2-LOCAL][NETWORK] HTTP ${res.status} fetching ${url}`,
      );
    }
    const buf = Buffer.from(await res.arrayBuffer());
    return buf;
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

function sha256(buf) {
  return createHash("sha256").update(buf).digest("hex");
}

/**
 * Sync redistributable ROMs into local/apple2/roms/.
 * @param {{ dryRun?: boolean, log?: (s: string) => void }} [opts]
 */
export async function syncRedistributableRoms(opts = {}) {
  const log = opts.log ?? console.log;
  ensureLocalApple2Layout();
  assertDestinationIgnored(LOCAL_ROMS_DIR);

  const catalog = loadRomsCatalog();
  const manifest = loadLocalLibraryManifest();
  const results = [];

  for (const rom of catalog.roms || []) {
    if (rom.provenance !== REDIS_STATUS.REDISTRIBUTABLE) {
      log(
        `[ROM] skip id=${rom.id} provenance=${rom.provenance} (only REDISTRIBUTABLE auto-downloaded)`,
      );
      results.push({ id: rom.id, skipped: true, reason: "not-redistributable" });
      continue;
    }

    const dest = join(LOCAL_ROMS_DIR, rom.filename);
    const provenance = retainProvenance(rom.provenance);

    if (opts.dryRun) {
      log(`[ROM] dry-run would fetch ${rom.sourceUrl} -> ${dest}`);
      results.push({ id: rom.id, dryRun: true, dest });
      continue;
    }

    log(`[ROM] fetching id=${rom.id} url=${rom.sourceUrl}`);
    const buf = await fetchBinary(rom.sourceUrl);

    if (rom.expectedSize != null && buf.length !== rom.expectedSize) {
      throw new Error(
        `[ROM][FAIL] id=${rom.id} size=${buf.length} expected=${rom.expectedSize}`,
      );
    }
    const hash = sha256(buf);
    if (rom.expectedSha256 && rom.expectedSha256 !== hash) {
      throw new Error(
        `[ROM][FAIL] id=${rom.id} sha256 mismatch got=${hash} expected=${rom.expectedSha256}`,
      );
    }

    writeFileSync(dest, buf);
    const record = {
      id: rom.id,
      filename: rom.filename,
      path: dest,
      relativePath: `local/apple2/roms/${rom.filename}`,
      sourceUrl: rom.sourceUrl,
      author: rom.author,
      license: rom.license,
      provenance,
      evidence: rom.evidence,
      size: buf.length,
      sha256: hash,
      mapping: rom.mapping,
      profile: rom.profile,
      devicePath: rom.devicePath,
      downloadedAt: new Date().toISOString(),
      kind: "redistributable_rom",
    };
    manifest.roms[rom.id] = record;
    log(
      `[ROM] OK id=${rom.id} size=${buf.length} sha256=${hash.slice(0, 12)}… provenance=${provenance}`,
    );
    results.push({ id: rom.id, ok: true, ...record });
  }

  saveLocalLibraryManifest(manifest);
  return { ok: true, results, romsDir: LOCAL_ROMS_DIR };
}

/**
 * Resolve a synced ROM path by id (offline).
 */
export function resolveLocalRom(romId) {
  const manifest = loadLocalLibraryManifest();
  const rec = manifest.roms?.[romId];
  if (rec?.path && existsSync(rec.path)) return rec;
  const catalog = loadRomsCatalog();
  const entry = (catalog.roms || []).find((r) => r.id === romId);
  if (!entry) return null;
  const path = join(LOCAL_ROMS_DIR, entry.filename);
  if (!existsSync(path)) return null;
  const buf = readFileSync(path);
  return {
    id: romId,
    path,
    size: buf.length,
    sha256: sha256(buf),
    provenance: entry.provenance,
    profile: entry.profile,
    devicePath: entry.devicePath,
    filename: entry.filename,
  };
}
