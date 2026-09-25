#!/usr/bin/env node
/**
 * CLI for apple2js catalog tooling.
 * Usage:
 *   node dev/tools/apple2js/cli.mjs sync
 *   node dev/tools/apple2js/cli.mjs catalog
 *   node dev/tools/apple2js/cli.mjs audit
 */

import { existsSync } from "node:fs";
import { join } from "node:path";
import { syncApple2js } from "./sync.mjs";
import {
  loadGitCatalog,
  loadWebCatalog,
  mergeCatalogs,
  parseApple2jsIndex,
} from "./catalog.mjs";
import { auditCatalog, summarizeAudit } from "./audit.mjs";
import { normalizeToGameMetadata } from "./normalize.mjs";
import {
  ESP2_ROOT,
  PIN_FILE,
  WEB_INDEX_CACHE,
  ensureDir,
  readJson,
  writeJson,
} from "./paths.mjs";

const cmd = process.argv[2] || "help";

async function main() {
  if (cmd === "sync") {
    const r = await syncApple2js({ fetchWebIndex: true });
    console.log(
      JSON.stringify(
        {
          commit: r.pin.commit,
          short: r.pin.short,
          webIndexCount: r.webIndexCount,
          cache: ".cache/apple2js",
        },
        null,
        2,
      ),
    );
    return;
  }

  if (cmd === "catalog" || cmd === "audit") {
    let gitEntries = [];
    let webEntries = [];
    let pin = null;
    try {
      const g = loadGitCatalog();
      gitEntries = g.entries;
      pin = g.pin;
    } catch (e) {
      console.error(String(e.message || e));
    }
    try {
      const w = loadWebCatalog();
      webEntries = w.entries;
    } catch (e) {
      console.error(String(e.message || e));
    }
    if (!gitEntries.length && !webEntries.length) {
      process.exitCode = 1;
      console.error("No catalog available. Run: node dev/tools/apple2js/cli.mjs sync");
      return;
    }
    const merged = mergeCatalogs(gitEntries, webEntries);
    const audited = auditCatalog(merged);
    const summary = summarizeAudit(audited);

    const outDir = join(ESP2_ROOT, "docs", "media", "generated");
    ensureDir(outDir);
    const auditPath = join(outDir, "apple2js-catalog-audit.json");
    const prevPath = join(outDir, "apple2js-catalog-audit.prev.json");
    let previous = null;
    if (existsSync(auditPath)) {
      previous = readJson(auditPath);
      writeJson(prevPath, previous);
    }

    const nextAudit = {
      schemaVersion: 1,
      generatedAt: new Date().toISOString(),
      upstreamPin: pin || (existsSync(PIN_FILE) ? readJson(PIN_FILE) : null),
      webIndexCachePresent: existsSync(WEB_INDEX_CACHE),
      gitEntryCount: gitEntries.length,
      webEntryCount: webEntries.length,
      mergedEntryCount: merged.length,
      summary,
      entries: audited,
    };
    writeJson(auditPath, nextAudit);

    let diff = null;
    if (previous && (cmd === "audit" || process.argv.includes("--diff"))) {
      const { diffApple2jsAudits } = await import("./sync_diff.mjs");
      diff = diffApple2jsAudits(previous, nextAudit);
      writeJson(join(outDir, "apple2js-catalog-diff.json"), {
        generatedAt: new Date().toISOString(),
        ...diff,
      });
    }

    if (cmd === "catalog") {
      console.log(
        JSON.stringify(
          {
            gitEntryCount: gitEntries.length,
            webEntryCount: webEntries.length,
            mergedEntryCount: merged.length,
            summary,
            diff: diff?.counts || null,
          },
          null,
          2,
        ),
      );
      return;
    }

    // audit: also emit metadata-only library stubs for key titles
    const stubDir = join(ESP2_ROOT, "library", "metadata");
    ensureDir(stubDir);
    const interesting = audited.filter((a) =>
      ["choplifter", "nightmission", "audit"].includes(a.basename),
    );
    for (const a of interesting) {
      const meta = normalizeToGameMetadata(a, {
        upstreamCommit: pin?.commit,
      });
      writeJson(join(stubDir, `${meta.id}.game.json`), meta);
    }
    console.log(
      JSON.stringify(
        {
          summary,
          wroteAudit: "docs/media/generated/apple2js-catalog-audit.json",
          stubs: interesting.map((a) => a.id),
          diff: diff?.counts || null,
          provenanceBlocked: diff?.provenanceBlocked || [],
        },
        null,
        2,
      ),
    );
    if (diff?.provenanceBlocked?.length) {
      process.exitCode = 2;
    }
    return;
  }

  console.log(`Usage:
  node dev/tools/apple2js/cli.mjs sync     # network: clone/fetch + web index
  node dev/tools/apple2js/cli.mjs catalog  # offline if cache present
  node dev/tools/apple2js/cli.mjs audit    # offline if cache present
`);
}

main().catch((e) => {
  console.error(e);
  process.exit(1);
});
