import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { parseApple2jsIndex, mergeCatalogs } from "../../dev/tools/apple2js/catalog.mjs";
import { auditCatalog, summarizeAudit } from "../../dev/tools/apple2js/audit.mjs";
import { normalizeToGameMetadata } from "../../dev/tools/apple2js/normalize.mjs";
import {
  jsonDiskToFlatImage,
  parseJsonDiskMeta,
  DOS33_BYTES,
  sha256Hex,
} from "../../dev/tools/apple2js/json_disk.mjs";
import { REDIS_STATUS } from "../../dev/tools/apple2js/paths.mjs";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../..");

export function testApple2jsCatalogParseAndNormalize() {
  const idxPath = path.join(
    root,
    "fixtures/synthetic/apple2js/sample-index.json",
  );
  const raw = JSON.parse(fs.readFileSync(idxPath, "utf8"));
  const entries = parseApple2jsIndex(raw, "synthetic");
  assert.equal(entries.length, 5);

  const audited = auditCatalog(
    entries.map((e) => ({ ...e, inGitRepo: false, onWebsite: true })),
  );
  const summary = summarizeAudit(audited);
  assert.equal(summary.USER_SUPPLIED_ONLY >= 2, true);

  const chop = audited.find((a) => a.basename === "choplifter");
  assert.ok(chop);
  assert.equal(chop.redistribution.status, REDIS_STATUS.USER_SUPPLIED_ONLY);

  const night = audited.find((a) => a.basename === "nightmission");
  assert.equal(night.redistribution.status, REDIS_STATUS.USER_SUPPLIED_ONLY);

  const meta = normalizeToGameMetadata(chop, { upstreamCommit: "deadbeef" });
  assert.equal(meta.schemaVersion, 1);
  assert.equal(meta.source.type, "apple2js");
  assert.equal(meta.source.upstreamCommit, "deadbeef");
  assert.equal(meta.redistribution.status, REDIS_STATUS.USER_SUPPLIED_ONLY);
  assert.equal(meta.mediaStatus, "not_installed");
  assert.equal(meta.disks[0].mediaInstalled, false);
  assert.ok(meta.source.upstreamPath.includes("choplifter"));
}

export function testApple2jsMalformedIndex() {
  assert.throws(() => parseApple2jsIndex({}, "synthetic"));
  assert.throws(() =>
    parseApple2jsIndex([{ filename: "x", name: "n" }], "synthetic"),
  );
  assert.throws(() =>
    parseApple2jsIndex([{ name: "n", category: "c" }], "synthetic"),
  );
}

export function testApple2jsJsonDiskRoundtrip() {
  const p = path.join(
    root,
    "fixtures/synthetic/apple2js/synthetic-zero.dsk.json",
  );
  const json = JSON.parse(fs.readFileSync(p, "utf8"));
  const meta = parseJsonDiskMeta(json);
  assert.equal(meta.type, "dsk");
  const img = jsonDiskToFlatImage(json);
  assert.equal(img.length, DOS33_BYTES);
  assert.equal(
    sha256Hex(img),
    "805f89516ae3725e591d02cc0594f250fbc9ab74461f1e03faeed1649f24e9a2",
  );

  const bad = JSON.parse(
    fs.readFileSync(
      path.join(root, "fixtures/synthetic/apple2js/invalid-empty-data.json"),
      "utf8",
    ),
  );
  assert.throws(() => jsonDiskToFlatImage(bad));
}

export function testCatalogMergePrefersGitFlag() {
  const git = parseApple2jsIndex(
    [
      {
        filename: "json/disks/audit.json",
        name: "Apple II Audit",
        category: "Utility",
      },
    ],
    "git",
  );
  const web = parseApple2jsIndex(
    [
      {
        filename: "json/disks/audit.json",
        name: "Apple II Audit",
        category: "Utility",
      },
      {
        filename: "json/disks/choplifter.json",
        name: "Choplifter",
        category: "Game",
      },
    ],
    "website",
  );
  const merged = mergeCatalogs(git, web);
  assert.equal(merged.length, 2);
  const audit = merged.find((e) => e.filename.includes("audit"));
  assert.equal(audit.inGitRepo, true);
  assert.equal(audit.onWebsite, true);
  const chop = merged.find((e) => e.filename.includes("choplifter"));
  assert.equal(chop.inGitRepo, false);
  assert.equal(chop.onWebsite, true);
}
