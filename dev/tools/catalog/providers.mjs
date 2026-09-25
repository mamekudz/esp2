/**
 * Catalog provider abstraction — source-independent metadata lookup.
 * Providers never download commercial media.
 */

import { existsSync, readFileSync, readdirSync } from "node:fs";
import { basename, join } from "node:path";
import { REDIS_STATUS } from "../apple2js/paths.mjs";

/**
 * @typedef {object} CatalogMatch
 * @property {string} providerId
 * @property {string} id
 * @property {string} title
 * @property {string} [category]
 * @property {{ status: string, evidence?: string }} redistribution
 * @property {object} [raw]
 */

/**
 * @typedef {object} CatalogProvider
 * @property {string} id
 * @property {() => CatalogMatch[]} listEntries
 * @property {(sha256: string) => CatalogMatch[]} findByHash
 * @property {(query: string) => CatalogMatch[]} findByTitle
 */

export function loadSourcesRegistry(path) {
  const raw = JSON.parse(readFileSync(path, "utf8"));
  if (!Array.isArray(raw.sources)) {
    throw new Error("sources.json: missing sources[]");
  }
  return raw;
}

/** Hard gate: may ESP][ auto-fetch media for this status? Always false for non-REDISTRIBUTABLE. */
export function mayAutoFetchMedia(status) {
  return status === REDIS_STATUS.REDISTRIBUTABLE;
}

/**
 * Prevent silent rights upgrade (e.g. USER_SUPPLIED_ONLY → REDISTRIBUTABLE)
 * without explicit allowlist of transitions.
 */
export function provenanceTransitionAllowed(from, to) {
  if (from === to) return true;
  const rank = {
    [REDIS_STATUS.REDISTRIBUTABLE]: 3,
    [REDIS_STATUS.UNKNOWN]: 2,
    [REDIS_STATUS.USER_SUPPLIED_ONLY]: 1,
    [REDIS_STATUS.DO_NOT_DISTRIBUTE]: 0,
  };
  // More restrictive always OK; less restrictive requires explicit review flag.
  if ((rank[to] ?? -1) < (rank[from] ?? -1)) return true;
  return false;
}

export function assertNoSilentRightsUpgrade(from, to, { allowUpgrade = false } = {}) {
  if (provenanceTransitionAllowed(from, to)) return;
  if (allowUpgrade) return;
  throw new Error(
    `provenance regression blocked: ${from} -> ${to} (requires explicit review)`,
  );
}

/** Local metadata stubs under library/metadata/*.game.json */
export function createLocalMetadataProvider(metadataDir) {
  /** @type {CatalogMatch[]} */
  let cache = null;
  function load() {
    if (cache) return cache;
    cache = [];
    if (!existsSync(metadataDir)) return cache;
    for (const name of readdirSync(metadataDir)) {
      if (!name.endsWith(".game.json") && !name.endsWith(".json")) continue;
      const p = join(metadataDir, name);
      const j = JSON.parse(readFileSync(p, "utf8"));
      cache.push({
        providerId: "local",
        id: j.id || basename(name, ".game.json").replace(/\.game$/, ""),
        title: j.title || j.id || name,
        category: j.category,
        redistribution: j.redistribution || {
          status: REDIS_STATUS.UNKNOWN,
          evidence: "local metadata without redistribution block",
        },
        raw: j,
      });
    }
    return cache;
  }
  return {
    id: "local",
    listEntries: () => load(),
    findByHash: () => [],
    findByTitle: (query) => {
      const q = String(query || "").toLowerCase();
      return load().filter(
        (e) =>
          e.title.toLowerCase().includes(q) ||
          e.id.toLowerCase().includes(q),
      );
    },
  };
}

/** known-hashes.json provider */
export function createKnownHashesProvider(hashDbPath) {
  function load() {
    if (!existsSync(hashDbPath)) return { entries: [] };
    return JSON.parse(readFileSync(hashDbPath, "utf8"));
  }
  return {
    id: "known-hashes",
    listEntries: () =>
      (load().entries || []).map((e) => ({
        providerId: "known-hashes",
        id: e.id,
        title: e.title,
        redistribution: e.provenance?.redistributionStatus
          ? {
              status: e.provenance.redistributionStatus,
              evidence: e.provenance.evidence,
            }
          : { status: REDIS_STATUS.UNKNOWN },
        raw: e,
      })),
    findByHash: (sha256) => {
      const hit = (load().entries || []).find(
        (e) => e.sha256 === String(sha256).toLowerCase(),
      );
      if (!hit) return [];
      return [
        {
          providerId: "known-hashes",
          id: hit.id,
          title: hit.title,
          redistribution: {
            status:
              hit.provenance?.redistributionStatus || REDIS_STATUS.UNKNOWN,
            evidence: hit.provenance?.evidence,
          },
          raw: hit,
        },
      ];
    },
    findByTitle: (query) => {
      const q = String(query || "").toLowerCase();
      return (load().entries || [])
        .filter((e) => (e.title || "").toLowerCase().includes(q))
        .map((e) => ({
          providerId: "known-hashes",
          id: e.id,
          title: e.title,
          redistribution: {
            status:
              e.provenance?.redistributionStatus || REDIS_STATUS.UNKNOWN,
            evidence: e.provenance?.evidence,
          },
          raw: e,
        }));
    },
  };
}

/**
 * apple2js audit JSON provider (metadata only; never fetches blobs).
 * @param {string} auditPath docs/media/generated/apple2js-catalog-audit.json
 */
export function createApple2jsAuditProvider(auditPath) {
  function load() {
    if (!existsSync(auditPath)) return { entries: [] };
    return JSON.parse(readFileSync(auditPath, "utf8"));
  }
  return {
    id: "apple2js",
    listEntries: () =>
      (load().entries || []).map((e) => ({
        providerId: "apple2js",
        id: e.id,
        title: e.title,
        category: e.category,
        redistribution: e.redistribution,
        raw: e,
      })),
    findByHash: () => [],
    findByTitle: (query) => {
      const q = String(query || "").toLowerCase();
      return (load().entries || [])
        .filter(
          (e) =>
            (e.title || "").toLowerCase().includes(q) ||
            (e.id || "").toLowerCase().includes(q) ||
            (e.basename || "").toLowerCase() === q,
        )
        .map((e) => ({
          providerId: "apple2js",
          id: e.id,
          title: e.title,
          category: e.category,
          redistribution: e.redistribution,
          raw: e,
        }));
    },
  };
}

/**
 * @param {CatalogProvider[]} providers
 */
export function createCatalogHub(providers) {
  return {
    providers,
    findByHash(sha256) {
      return providers.flatMap((p) => p.findByHash(sha256));
    },
    findByTitle(query) {
      return providers.flatMap((p) => p.findByTitle(query));
    },
    listAll() {
      return providers.flatMap((p) => p.listEntries());
    },
  };
}
