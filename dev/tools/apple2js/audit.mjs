/**
 * Provenance / redistribution classification for apple2js catalog entries.
 *
 * Default: UNKNOWN. Never infer rights from mere website availability.
 * MIT on the apple2js *emulator* does not cover third-party disk media.
 */

import { REDIS_STATUS, slugifyTitle } from "./paths.mjs";

/**
 * Explicit overrides keyed by basename without .json (e.g. "choplifter").
 * Evidence must be documented; do not expand without review.
 */
const KNOWN = Object.freeze({
  choplifter: {
    status: REDIS_STATUS.USER_SUPPLIED_ONLY,
    evidence:
      "Commercial Broderbund title. Listed on apple2js website catalog only; not in public git json/disks on inspected main. No redistribution grant found.",
    publisherHint: "Broderbund",
  },
  nightmission: {
    status: REDIS_STATUS.USER_SUPPLIED_ONLY,
    evidence:
      "Commercial title (Night Mission / Sublogic lineage commonly cited). Website catalog only; not in public git main. No redistribution grant found.",
    publisherHint: "Sublogic (historical)",
  },
  audit: {
    status: REDIS_STATUS.REDISTRIBUTABLE,
    evidence:
      "Corresponds to Zellyn Hunter a2audit (MIT). Prefer upstream https://github.com/zellyn/a2audit audit/audit.dsk rather than website JSON blob. apple2js README acknowledges a2audit.",
    preferredUpstream: "https://github.com/zellyn/a2audit",
  },
  blank_dos33: {
    status: REDIS_STATUS.UNKNOWN,
    evidence:
      "Blank DOS 3.3 template in apple2js git. May still embed Apple DOS boot/format IP; treat as non-shipped template until legal review.",
  },
  blank_prodos: {
    status: REDIS_STATUS.UNKNOWN,
    evidence:
      "Blank ProDOS template in apple2js git. ProDOS is Apple copyrighted system software territory; UNKNOWN.",
  },
  dos33master: {
    status: REDIS_STATUS.DO_NOT_DISTRIBUTE,
    evidence:
      "Apple DOS 3.3 system master — Apple copyrighted system software. Not redistributable with ESP][.",
  },
  prodos: {
    status: REDIS_STATUS.DO_NOT_DISTRIBUTE,
    evidence:
      "Apple ProDOS system disk — Apple copyrighted. Not redistributable with ESP][.",
  },
});

function basenameKey(filename) {
  const base = String(filename).split("/").pop() || "";
  return base.replace(/\.json$/i, "").toLowerCase();
}

/**
 * @param {object} entry catalog entry
 */
export function auditCatalogEntry(entry) {
  const key = basenameKey(entry.filename);
  const known = KNOWN[key];
  let status = REDIS_STATUS.UNKNOWN;
  let evidence =
    "No explicit redistribution evidence in apple2js MIT LICENSE for third-party media. Default UNKNOWN.";

  if (known) {
    status = known.status;
    evidence = known.evidence;
  } else if (entry.category === "Game" || entry.category === "Interactive Fiction") {
    status = REDIS_STATUS.USER_SUPPLIED_ONLY;
    evidence =
      "Commercial/entertainment category on public website catalog without a clear redistribution grant. User must supply legally obtained media.";
  } else if (
    entry.category === "System" ||
    /dos|prodos|master/i.test(entry.name)
  ) {
    status = REDIS_STATUS.DO_NOT_DISTRIBUTE;
    evidence =
      "Likely Apple system software. Do not distribute with ESP][ without Apple redistribution rights.";
  } else if (entry.category === "Blank") {
    status = REDIS_STATUS.UNKNOWN;
    evidence = "Blank template — may still contain OS boot IP; UNKNOWN pending review.";
  }

  return {
    id: slugifyTitle(entry.name),
    title: entry.name,
    category: entry.category,
    upstreamFilename: entry.filename,
    basename: key,
    inGitRepo: Boolean(entry.inGitRepo),
    onWebsite: Boolean(entry.onWebsite),
    catalogSource: entry.catalogSource,
    redistribution: {
      status,
      evidence,
      preferredUpstream: known?.preferredUpstream,
      publisherHint: known?.publisherHint,
    },
  };
}

export function auditCatalog(entries) {
  return entries.map(auditCatalogEntry);
}

export function summarizeAudit(audited) {
  const counts = {
    REDISTRIBUTABLE: 0,
    USER_SUPPLIED_ONLY: 0,
    UNKNOWN: 0,
    DO_NOT_DISTRIBUTE: 0,
  };
  for (const a of audited) {
    counts[a.redistribution.status] =
      (counts[a.redistribution.status] || 0) + 1;
  }
  return counts;
}
