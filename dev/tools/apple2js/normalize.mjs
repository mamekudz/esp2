/**
 * Normalize apple2js catalog (+ audit) entries into ESP][ game metadata.
 * Runtime must not depend on apple2js JSON shapes.
 */

import {
  APPLE2JS_REPO_URL,
  APPLE2JS_SITE_URL,
  REDIS_STATUS,
  slugifyTitle,
} from "./paths.mjs";
import { auditCatalogEntry } from "./audit.mjs";

/**
 * @param {object} entry apple2js catalog entry (optional audit fields)
 * @param {{ upstreamCommit?: string, pin?: object }} [ctx]
 */
export function normalizeToGameMetadata(entry, ctx = {}) {
  const audited = entry.redistribution
    ? entry
    : auditCatalogEntry(entry);

  const id = audited.id || slugifyTitle(audited.title || entry.name);
  const mediaStatus =
    audited.redistribution.status === REDIS_STATUS.REDISTRIBUTABLE
      ? "available_if_obtained"
      : "not_installed";

  const diskFileHint = `${id}.dsk`;

  return {
    schemaVersion: 1,
    id,
    title: audited.title || entry.name,
    category: audited.category || entry.category,
    disks: [
      {
        file: diskFileHint,
        label: "disk1",
        writeProtected: true,
        mediaInstalled: false,
      },
    ],
    bootDisk: diskFileHint,
    defaultDrive1: diskFileHint,
    defaultDrive2: null,
    joystickMode: "auto",
    videoColorMode: "CompositeColor",
    displayEffect: "Sharp",
    effectStrength: "Off",
    notes:
      "Metadata imported from apple2js catalog. Disk image not bundled with ESP][.",
    mediaStatus,
    source: {
      type: "apple2js",
      repository: APPLE2JS_REPO_URL,
      site: APPLE2JS_SITE_URL,
      upstreamCommit: ctx.upstreamCommit || ctx.pin?.commit || null,
      upstreamPath: audited.upstreamFilename || entry.filename,
      upstreamUrl: `${APPLE2JS_SITE_URL}${audited.upstreamFilename || entry.filename}`,
      inGitRepo: Boolean(audited.inGitRepo ?? entry.inGitRepo),
      onWebsite: Boolean(audited.onWebsite ?? entry.onWebsite),
    },
    redistribution: {
      status: audited.redistribution.status,
      evidence: audited.redistribution.evidence,
      preferredUpstream: audited.redistribution.preferredUpstream || null,
      publisherHint: audited.redistribution.publisherHint || null,
    },
  };
}

/**
 * Library directory layout helper (metadata may exist without media).
 */
export function libraryRelativeDir(gameMeta) {
  return `apple2/games/${gameMeta.id}`;
}
