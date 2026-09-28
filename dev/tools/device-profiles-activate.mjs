/**
 * Activate ESP][ device profiles after clone: download + prepare media,
 * optionally seed local/device/config from the chosen profile.
 *
 * Media stays under gitignored local/apple2/. Profiles are path-only JSON.
 */
import {
  listProfileIds,
  loadProfileSystemJson,
  normalizeSystemConfig,
  saveConfigLocal,
  profilePaths,
} from "./device-config-form.mjs";
import { syncTitleMedia } from "./apple2-local/media-sync.mjs";
import { prepareTitleMedia } from "./apple2-local/media-prepare.mjs";
import { syncRedistributableRoms } from "./apple2-local/rom-sync.mjs";
import { PROTECTED_PROFILE_IDS } from "./device-profiles-from-apple2js.mjs";
import { writeFileSync } from "node:fs";

/** Curated hand profiles for first-run style activation. */
export const DEMO_PROFILE_IDS = Object.freeze([
  "galaxian-demo",
  "little-brick-out",
  "star-blazer",
]);

/**
 * Resolve apple2js / media-sync title id from a profile folder.
 * @param {string} root
 * @param {string} profileId
 */
export function mediaTitleIdForProfile(root, profileId) {
  const raw = loadProfileSystemJson(root, profileId);
  const metaBase = raw?.meta?.apple2jsBasename;
  if (metaBase) return String(metaBase);
  if (profileId === "galaxian-demo") return "galaxian";
  if (profileId === "little-brick-out") return "littlebrickout";
  if (profileId === "star-blazer") return "starblazer";
  return profileId;
}

/**
 * @param {string} root
 * @param {string} profileId
 */
export function profileRedistribution(root, profileId) {
  const raw = loadProfileSystemJson(root, profileId);
  return String(raw?.meta?.redistribution || "UNKNOWN");
}

/**
 * Expand selection tokens into concrete profile ids.
 * Tokens: profile id, `demos`, `category:<Name>`, `all-safe`.
 * @param {string} root
 * @param {string|string[]} selection
 */
export function resolveProfileSelection(root, selection) {
  const available = new Set(listProfileIds(root));
  const tokens = (Array.isArray(selection) ? selection : String(selection || "").split(/[,+\s]+/))
    .map((s) => String(s || "").trim())
    .filter(Boolean);
  const out = [];
  const push = (id) => {
    if (available.has(id) && !out.includes(id)) out.push(id);
  };

  for (const token of tokens) {
    const lower = token.toLowerCase();
    if (lower === "demos" || lower === "__demos__") {
      for (const id of DEMO_PROFILE_IDS) push(id);
      continue;
    }
    if (lower === "all-safe" || lower === "__all_safe__") {
      for (const id of available) {
        if (profileRedistribution(root, id) === "DO_NOT_DISTRIBUTE") continue;
        push(id);
      }
      continue;
    }
    if (lower.startsWith("category:") || lower.startsWith("__category_")) {
      let cat = token;
      if (lower.startsWith("category:")) cat = token.slice("category:".length);
      else {
        cat = token.replace(/^__category_/i, "").replace(/__$/, "");
      }
      for (const id of available) {
        const raw = loadProfileSystemJson(root, id);
        if (String(raw?.meta?.category || "") === cat) push(id);
      }
      if (cat === "Game" || cat === "Demo") {
        for (const id of DEMO_PROFILE_IDS) {
          const raw = loadProfileSystemJson(root, id);
          if (!raw?.meta?.category) push(id);
        }
      }
      continue;
    }
    push(token);
  }
  return out;
}

/**
 * @param {string} root
 * @param {string[]} profileIds
 * @param {{
 *   download?: boolean,
 *   prepare?: boolean,
 *   seedLocal?: boolean,
 *   seedProfileId?: string|null,
 *   syncRoms?: boolean,
 *   log?: Function,
 * }} [opts]
 */
export async function activateProfiles(root, profileIds, opts = {}) {
  const log = opts.log ?? console.log;
  const download = opts.download !== false;
  const prepare = opts.prepare !== false;
  const syncRoms = opts.syncRoms === true;
  const results = [];

  if (syncRoms) {
    log("[PROFILES] syncing redistributable ROMs…");
    await syncRedistributableRoms({ log });
  }

  for (const profileId of profileIds) {
    const id = String(profileId || "").trim();
    if (!id) continue;
    if (!listProfileIds(root).includes(id)) {
      results.push({ id, ok: false, error: "profile_not_found" });
      continue;
    }
    const redist = profileRedistribution(root, id);
    if (redist === "DO_NOT_DISTRIBUTE") {
      results.push({
        id,
        ok: false,
        error: "do_not_distribute",
        redistribution: redist,
      });
      log(`[PROFILES] skip ${id} — DO_NOT_DISTRIBUTE`);
      continue;
    }

    const titleId = mediaTitleIdForProfile(root, id);
    const row = { id, titleId, ok: true, redistribution: redist };

    try {
      if (download) {
        log(`[PROFILES] media-sync title=${titleId} (profile=${id})…`);
        const sync = await syncTitleMedia(titleId, {
          ensureApple2jsSync: true,
          log,
        });
        row.sync = {
          provenance: sync.record?.provenance || sync.provenance,
          sourceSha256: sync.record?.sourceSha256 || sync.sourceSha256,
        };
      }
      if (prepare) {
        log(`[PROFILES] media-prepare title=${titleId}…`);
        try {
          const prep = prepareTitleMedia(titleId, { log });
          row.prepare = {
            runtimePath: prep.runtimePath,
            runtimeSha256: prep.runtimeSha256,
            devicePath: prep.record?.devicePath,
          };
          if (prep.record?.devicePath && !PROTECTED_PROFILE_IDS.includes(id)) {
            const paths = profilePaths(root, id);
            const raw = loadProfileSystemJson(root, id);
            if (raw?.media) {
              raw.media.drive1 = prep.record.devicePath;
              writeFileSync(
                paths.system,
                `${JSON.stringify(raw, null, 2)}\n`,
                "utf8",
              );
            }
          }
        } catch (err) {
          row.prepareError = String(err.message || err);
          log(`[PROFILES] prepare deferred for ${id}: ${row.prepareError}`);
        }
      }
    } catch (err) {
      row.ok = false;
      row.error = String(err.message || err);
      log(`[PROFILES] FAIL ${id}: ${row.error}`);
    }
    results.push(row);
  }

  const seedId = opts.seedProfileId || (opts.seedLocal ? profileIds[0] : null);
  if (seedId && listProfileIds(root).includes(seedId)) {
    const raw = loadProfileSystemJson(root, seedId);
    const normalized = normalizeSystemConfig(raw);
    if (!normalized.ok) {
      throw new Error(`seed profile invalid: ${normalized.errors.join("; ")}`);
    }
    const saved = saveConfigLocal(root, normalized.config, seedId, seedId);
    log(`[PROFILES] seeded local config from ${seedId} → ${saved.localSystem}`);
    return { ok: results.every((r) => r.ok), results, seeded: saved.localSystem };
  }

  return { ok: results.every((r) => r.ok), results, seeded: null };
}

/**
 * List profiles with light metadata for forms.
 * @param {string} root
 */
export function listProfilesForActivation(root) {
  return listProfileIds(root).map((id) => {
    const raw = loadProfileSystemJson(root, id) || {};
    return {
      id,
      title: raw.meta?.title || id,
      category: raw.meta?.category || "",
      redistribution: raw.meta?.redistribution || "UNKNOWN",
      drive1: raw.media?.drive1 || "",
    };
  });
}
