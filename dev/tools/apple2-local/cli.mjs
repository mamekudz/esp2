#!/usr/bin/env node
/**
 * CLI for local Apple II media workflow (gitignored local/apple2/).
 *
 *   node dev/tools/apple2-local/cli.mjs rom-sync
 *   node dev/tools/apple2-local/cli.mjs media-sync --title galaxian
 *   node dev/tools/apple2-local/cli.mjs media-sync --all
 *   node dev/tools/apple2-local/cli.mjs media-prepare --title galaxian
 *   node dev/tools/apple2-local/cli.mjs media-status
 *   node dev/tools/apple2-local/cli.mjs media-clean
 *   node dev/tools/apple2-local/cli.mjs media-audit
 *   node dev/tools/apple2-local/cli.mjs media-list
 *   node dev/tools/apple2-local/cli.mjs device-sync --title galaxian --port COM5
 */

import { syncRedistributableRoms } from "./rom-sync.mjs";
import {
  syncTitleMedia,
  syncAllMedia,
  auditLocalMedia,
  listCachedTitles,
} from "./media-sync.mjs";
import { prepareTitleMedia } from "./media-prepare.mjs";
import { printMediaStatus, cleanLocalMedia } from "./media-status.mjs";
import { deviceSyncTitle } from "./device-sync.mjs";

function argValue(args, name) {
  const i = args.indexOf(name);
  if (i >= 0 && args[i + 1]) return args[i + 1];
  const eq = args.find((a) => a.startsWith(`${name}=`));
  if (eq) return eq.slice(name.length + 1);
  return null;
}

function hasFlag(args, name) {
  return args.includes(name);
}

async function main() {
  const cmd = process.argv[2] || "help";
  const args = process.argv.slice(3);

  if (cmd === "rom-sync" || cmd === "rom:sync") {
    const r = await syncRedistributableRoms({ dryRun: hasFlag(args, "--dry-run") });
    console.log(JSON.stringify({ ok: r.ok, count: r.results.length, results: r.results.map((x) => ({
      id: x.id,
      ok: x.ok,
      skipped: x.skipped,
      sha256: x.sha256,
      size: x.size,
      provenance: x.provenance,
    })) }, null, 2));
    return;
  }

  if (cmd === "media-sync" || cmd === "media:sync") {
    if (hasFlag(args, "--all")) {
      const r = await syncAllMedia({
        dryRun: hasFlag(args, "--dry-run"),
        offline: hasFlag(args, "--offline"),
        skipIfPresent: hasFlag(args, "--skip-if-present"),
        ensureApple2jsSync: !hasFlag(args, "--offline"),
      });
      console.log(JSON.stringify(r.counts, null, 2));
      if (!r.ok) process.exitCode = 1;
      return;
    }
    const title = argValue(args, "--title");
    if (!title) {
      console.error("media-sync requires --title <id> or --all");
      process.exitCode = 1;
      return;
    }
    const r = await syncTitleMedia(title, {
      dryRun: hasFlag(args, "--dry-run"),
      offline: hasFlag(args, "--offline"),
      ensureApple2jsSync: !hasFlag(args, "--offline"),
    });
    // Title-specific sync also prepares when technically safe (per req §4)
    if (!r.dryRun && !hasFlag(args, "--no-prepare")) {
      try {
        const p = prepareTitleMedia(r.id);
        console.log(JSON.stringify({ sync: { id: r.id, provenance: r.record.provenance, sourceSha256: r.record.sourceSha256 }, prepare: p }, null, 2));
        return;
      } catch (err) {
        console.warn(`[MEDIA] sync OK but prepare deferred: ${err.message}`);
      }
    }
    console.log(JSON.stringify({ id: r.id, provenance: r.record?.provenance, sourceSha256: r.record?.sourceSha256 }, null, 2));
    return;
  }

  if (cmd === "media-prepare" || cmd === "media:prepare") {
    const title = argValue(args, "--title");
    if (!title) {
      console.error("media-prepare requires --title <id>");
      process.exitCode = 1;
      return;
    }
    const r = prepareTitleMedia(title);
    console.log(JSON.stringify({
      id: r.id,
      runtimeFormat: r.runtimeFormat,
      runtimeSha256: r.runtimeSha256,
      runtimeSize: r.runtimeSize,
      provenance: r.provenance,
      runtimePath: r.runtimePath,
    }, null, 2));
    return;
  }

  if (cmd === "media-status" || cmd === "media:status") {
    printMediaStatus();
    return;
  }

  if (cmd === "media-list" || cmd === "media:list") {
    console.log(JSON.stringify(listCachedTitles().map((t) => ({
      id: t.id,
      title: t.title,
      provenance: t.provenance,
      sourceType: t.sourceType,
      prepared: Boolean(t.runtimePath),
    })), null, 2));
    return;
  }

  if (cmd === "media-audit" || cmd === "media:audit") {
    console.log(JSON.stringify(auditLocalMedia(), null, 2));
    return;
  }

  if (cmd === "media-clean" || cmd === "media:clean") {
    const r = cleanLocalMedia({
      cache: hasFlag(args, "--cache") || hasFlag(args, "--purge-downloads"),
      user: hasFlag(args, "--user"),
    });
    console.log(JSON.stringify({ ok: r.ok, removedCount: r.removed.length }, null, 2));
    return;
  }

  if (cmd === "device-sync" || cmd === "device:sync") {
    const title = argValue(args, "--title");
    const port = argValue(args, "--port");
    const r = deviceSyncTitle({ title, port, romId: argValue(args, "--rom-id") });
    console.log(JSON.stringify({
      ok: r.ok,
      title: r.title,
      uploads: r.uploads.map((u) => ({
        kind: u.kind,
        target: u.target,
        size: u.deviceSize,
        sha256: u.deviceSha256,
      })),
    }, null, 2));
    return;
  }

  console.log(`Usage:
  rom-sync
  media-sync --title <id> | --all
  media-prepare --title <id>
  media-status | media-list | media-audit
  media-clean [--cache] [--user]
  device-sync --title <id> --port COMx`);
  process.exitCode = cmd === "help" ? 0 : 1;
}

main().catch((err) => {
  console.error(err.message || err);
  process.exitCode = 1;
});
