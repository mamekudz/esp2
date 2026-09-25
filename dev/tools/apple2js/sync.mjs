/**
 * Clone / update apple2js into .cache/apple2js and record the pinned commit.
 * Explicit developer network task — never called by normal build/test/docs.
 */

import { existsSync } from "node:fs";
import { spawnSync } from "node:child_process";
import {
  APPLE2JS_CACHE_DIR,
  APPLE2JS_META_DIR,
  APPLE2JS_REPO_URL,
  APPLE2JS_SITE_INDEX_URL,
  PIN_FILE,
  WEB_INDEX_CACHE,
  ensureDir,
  writeJson,
} from "./paths.mjs";

function runGit(args, cwd) {
  const r = spawnSync("git", args, {
    cwd,
    encoding: "utf8",
    shell: false,
  });
  if (r.status !== 0) {
    const err = (r.stderr || r.stdout || "").trim();
    throw new Error(`git ${args.join(" ")} failed: ${err || `exit ${r.status}`}`);
  }
  return (r.stdout || "").trim();
}

/**
 * @param {{ fetchWebIndex?: boolean }} [opts]
 */
export async function syncApple2js(opts = {}) {
  const fetchWebIndex = opts.fetchWebIndex !== false;
  ensureDir(APPLE2JS_META_DIR);

  if (!existsSync(APPLE2JS_CACHE_DIR)) {
    ensureDir(APPLE2JS_CACHE_DIR);
    runGit(
      ["clone", "--depth", "1", APPLE2JS_REPO_URL, APPLE2JS_CACHE_DIR],
      undefined,
    );
  } else {
    runGit(["fetch", "--depth", "1", "origin", "main"], APPLE2JS_CACHE_DIR);
    runGit(["checkout", "main"], APPLE2JS_CACHE_DIR);
    runGit(["reset", "--hard", "origin/main"], APPLE2JS_CACHE_DIR);
  }

  const commit = runGit(["rev-parse", "HEAD"], APPLE2JS_CACHE_DIR);
  const short = runGit(["rev-parse", "--short", "HEAD"], APPLE2JS_CACHE_DIR);
  const subject = runGit(["log", "-1", "--pretty=%s"], APPLE2JS_CACHE_DIR);

  const pin = {
    repository: APPLE2JS_REPO_URL,
    commit,
    short,
    subject,
    syncedAt: new Date().toISOString(),
    note: "Local cache only (.cache/apple2js). Not part of ESP][ commits.",
  };
  writeJson(PIN_FILE, pin);

  let webIndex = null;
  if (fetchWebIndex) {
    const res = await fetch(APPLE2JS_SITE_INDEX_URL);
    if (!res.ok) {
      throw new Error(`web index fetch failed: HTTP ${res.status}`);
    }
    webIndex = await res.json();
    writeJson(WEB_INDEX_CACHE, {
      sourceUrl: APPLE2JS_SITE_INDEX_URL,
      fetchedAt: new Date().toISOString(),
      entryCount: webIndex.length,
      entries: webIndex,
    });
  }

  return {
    pin,
    gitDiskIndexPath: `${APPLE2JS_CACHE_DIR}/json/disks/index.json`,
    webIndexCount: webIndex ? webIndex.length : null,
  };
}
