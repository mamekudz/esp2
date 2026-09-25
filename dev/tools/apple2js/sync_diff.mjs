/**
 * Diff two apple2js audit snapshots (metadata only).
 */

import { assertNoSilentRightsUpgrade } from "../catalog/providers.mjs";

/**
 * @param {object} previousAudit
 * @param {object} nextAudit
 * @param {{ allowRightsUpgrade?: boolean }} [opts]
 */
export function diffApple2jsAudits(previousAudit, nextAudit, opts = {}) {
  const prev = indexByBasename(previousAudit?.entries || []);
  const next = indexByBasename(nextAudit?.entries || []);

  const added = [];
  const removed = [];
  const changed = [];
  const provenanceChanges = [];
  const provenanceBlocked = [];

  for (const [key, n] of next) {
    const p = prev.get(key);
    if (!p) {
      added.push(summarize(n));
      continue;
    }
    const metaChanged =
      p.title !== n.title ||
      p.category !== n.category ||
      p.inGitRepo !== n.inGitRepo ||
      p.onWebsite !== n.onWebsite;
    const from = p.redistribution?.status;
    const to = n.redistribution?.status;
    if (from !== to) {
      const entry = {
        basename: key,
        title: n.title,
        from,
        to,
      };
      try {
        assertNoSilentRightsUpgrade(from, to, {
          allowUpgrade: Boolean(opts.allowRightsUpgrade),
        });
        provenanceChanges.push(entry);
      } catch (e) {
        provenanceBlocked.push({ ...entry, error: String(e.message || e) });
      }
    }
    if (metaChanged || from !== to) {
      changed.push({
        basename: key,
        before: summarize(p),
        after: summarize(n),
      });
    }
  }

  for (const [key, p] of prev) {
    if (!next.has(key)) removed.push(summarize(p));
  }

  return {
    added,
    removed,
    changed,
    provenanceChanges,
    provenanceBlocked,
    counts: {
      added: added.length,
      removed: removed.length,
      changed: changed.length,
      provenanceChanges: provenanceChanges.length,
      provenanceBlocked: provenanceBlocked.length,
    },
  };
}

function indexByBasename(entries) {
  const m = new Map();
  for (const e of entries) {
    const key = (e.basename || e.id || e.title || "").toLowerCase();
    if (key) m.set(key, e);
  }
  return m;
}

function summarize(e) {
  return {
    basename: e.basename || e.id,
    title: e.title,
    category: e.category,
    status: e.redistribution?.status,
    inGitRepo: e.inGitRepo,
    onWebsite: e.onWebsite,
  };
}
