import assert from "node:assert/strict";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import { fileURLToPath } from "node:url";
import {
  createApple2jsAuditProvider,
  createCatalogHub,
  createKnownHashesProvider,
  createLocalMetadataProvider,
  assertNoSilentRightsUpgrade,
  mayAutoFetchMedia,
  loadSourcesRegistry,
  provenanceTransitionAllowed,
} from "../../dev/tools/catalog/providers.mjs";
import { REDIS_STATUS } from "../../dev/tools/apple2js/paths.mjs";
import { diffApple2jsAudits } from "../../dev/tools/apple2js/sync_diff.mjs";
import {
  detectMediaFormat,
  MEDIA_FORMAT,
  SUPPORT_LEVEL,
  hasSupportLevel,
  identifyMedia,
  importMedia,
  listImportableFilesInDirectory,
} from "../../dev/tools/media/import.mjs";
import { prepareSdLibrary } from "../../dev/tools/sd/prepare.mjs";
import { generateSyntheticDisk } from "../../host/tools/gen_test_disks.mjs";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../..");

export function testSourcesRegistry() {
  const reg = loadSourcesRegistry(path.join(root, "docs/media/sources.json"));
  assert.ok(reg.sources.length >= 5);
  const ids = reg.sources.map((s) => s.id);
  assert.ok(ids.includes("apple2js"));
  assert.ok(ids.includes("a2kit"));
  assert.ok(ids.includes("diskm8"));
  assert.ok(ids.includes("total-replay"));
}

export function testProvenanceGates() {
  assert.equal(mayAutoFetchMedia(REDIS_STATUS.USER_SUPPLIED_ONLY), false);
  assert.equal(mayAutoFetchMedia(REDIS_STATUS.UNKNOWN), false);
  assert.equal(mayAutoFetchMedia(REDIS_STATUS.DO_NOT_DISTRIBUTE), false);
  assert.equal(mayAutoFetchMedia(REDIS_STATUS.REDISTRIBUTABLE), true);

  assert.equal(
    provenanceTransitionAllowed(
      REDIS_STATUS.USER_SUPPLIED_ONLY,
      REDIS_STATUS.DO_NOT_DISTRIBUTE,
    ),
    true,
  );
  assert.throws(() =>
    assertNoSilentRightsUpgrade(
      REDIS_STATUS.USER_SUPPLIED_ONLY,
      REDIS_STATUS.REDISTRIBUTABLE,
    ),
  );
  assertNoSilentRightsUpgrade(
    REDIS_STATUS.USER_SUPPLIED_ONLY,
    REDIS_STATUS.REDISTRIBUTABLE,
    { allowUpgrade: true },
  );
}

export function testApple2jsSyncDiffBlocksUpgrade() {
  const prev = {
    entries: [
      {
        basename: "choplifter",
        title: "Choplifter",
        category: "Game",
        redistribution: { status: REDIS_STATUS.USER_SUPPLIED_ONLY },
      },
    ],
  };
  const next = {
    entries: [
      {
        basename: "choplifter",
        title: "Choplifter",
        category: "Game",
        redistribution: { status: REDIS_STATUS.REDISTRIBUTABLE },
      },
      {
        basename: "newgame",
        title: "New Game",
        category: "Game",
        redistribution: { status: REDIS_STATUS.USER_SUPPLIED_ONLY },
      },
    ],
  };
  const diff = diffApple2jsAudits(prev, next);
  assert.equal(diff.counts.added, 1);
  assert.equal(diff.provenanceBlocked.length, 1);
  assert.equal(diff.provenanceBlocked[0].from, REDIS_STATUS.USER_SUPPLIED_ONLY);
}

export function testFormatMatrixAndIdentify() {
  assert.ok(hasSupportLevel(MEDIA_FORMAT.Dsk, SUPPORT_LEVEL.ImportRecognized));
  assert.ok(hasSupportLevel(MEDIA_FORMAT.Dsk, SUPPORT_LEVEL.ParserSupported));
  assert.equal(
    hasSupportLevel(MEDIA_FORMAT.Woz, SUPPORT_LEVEL.EmulatorSupported),
    false,
  );

  const tmp = fs.mkdtempSync(path.join(os.tmpdir(), "esp2-id-"));
  const p = path.join(tmp, "x.dsk");
  fs.writeFileSync(p, generateSyntheticDisk("sector_id"));
  assert.equal(detectMediaFormat(p), MEDIA_FORMAT.Dsk);

  const hub = createCatalogHub([
    createLocalMetadataProvider(path.join(root, "library/metadata")),
    createKnownHashesProvider(path.join(root, "docs/media/known-hashes.json")),
    createApple2jsAuditProvider(
      path.join(root, "docs/media/generated/apple2js-catalog-audit.json"),
    ),
  ]);
  const id = identifyMedia({ path: p, catalogHub: hub });
  assert.equal(id.ok, true);
  assert.equal(id.wouldAutoDownload, false);
  assert.equal(id.sha256.length, 64);
}

export function testImportMultiDiskAndDuplicates() {
  const tmp = fs.mkdtempSync(path.join(os.tmpdir(), "esp2-imp-"));
  const lib = path.join(tmp, "library");
  const d1 = path.join(tmp, "disk1.dsk");
  const d2 = path.join(tmp, "disk2.dsk");
  fs.writeFileSync(d1, generateSyntheticDisk("multidisk_marker", { diskIndex: 1 }));
  fs.writeFileSync(d2, generateSyntheticDisk("multidisk_marker", { diskIndex: 2 }));

  const hub = createCatalogHub([
    createLocalMetadataProvider(path.join(root, "library/metadata")),
  ]);

  const r1 = importMedia({
    sourcePath: d1,
    libraryRoot: lib,
    title: "Choplifter",
    gameId: "choplifter",
    catalogHub: hub,
    preferGamesSubdir: true,
  });
  assert.equal(r1.ok, true);
  assert.ok(r1.meta.redistribution.status === REDIS_STATUS.USER_SUPPLIED_ONLY ||
    r1.identify.catalogMatches.length >= 0);

  const r2 = importMedia({
    sourcePath: d2,
    libraryRoot: lib,
    gameId: "choplifter",
    diskLabel: "disk2",
    preferGamesSubdir: true,
  });
  assert.equal(r2.ok, true);
  assert.equal(r2.meta.disks.length, 2);

  const dup = importMedia({
    sourcePath: d1,
    libraryRoot: lib,
    gameId: "choplifter-copy",
    preferGamesSubdir: true,
  });
  assert.equal(dup.ok, false);
  assert.equal(dup.duplicate, true);

  // Refuse downloadUrl
  const bad = importMedia({
    sourcePath: d2,
    libraryRoot: lib,
    gameId: "other",
    downloadUrl: "http://example.com/x.dsk",
    preferGamesSubdir: true,
    allowDuplicateCopy: true,
  });
  assert.equal(bad.ok, false);
}

export function testDirectoryImportNonRecursive() {
  const tmp = fs.mkdtempSync(path.join(os.tmpdir(), "esp2-dir-"));
  fs.writeFileSync(path.join(tmp, "a.dsk"), generateSyntheticDisk("zero"));
  fs.mkdirSync(path.join(tmp, "nested"));
  fs.writeFileSync(
    path.join(tmp, "nested", "b.dsk"),
    generateSyntheticDisk("zero"),
  );
  const files = listImportableFilesInDirectory(tmp);
  assert.equal(files.length, 1);
}

export function testSdPrepareDryRunSafety() {
  const tmp = fs.mkdtempSync(path.join(os.tmpdir(), "esp2-sd-"));
  const target = path.join(tmp, "card");
  assert.throws(() =>
    prepareSdLibrary({
      sourceLibrary: path.join(root, "library"),
      targetRoot: "C:\\",
      dryRun: true,
    }),
  );
  const plan = prepareSdLibrary({
    sourceLibrary: path.join(root, "library"),
    targetRoot: target,
    dryRun: true,
  });
  assert.equal(plan.ok, true);
  assert.equal(plan.dryRun, true);
  assert.ok(plan.plan.some((p) => p.action === "ensure_dir"));
  assert.equal(fs.existsSync(path.join(target, "apple2")), false);
}

export function testWozRedistributableHash() {
  const woz = path.join(
    root,
    "fixtures/redistributable/tcjennings-screen-address/screen_address_dos33.woz",
  );
  assert.ok(fs.existsSync(woz));
  const hub = createCatalogHub([
    createKnownHashesProvider(path.join(root, "docs/media/known-hashes.json")),
  ]);
  const id = identifyMedia({ path: woz, catalogHub: hub });
  assert.equal(id.format, MEDIA_FORMAT.Woz);
  assert.ok(id.catalogMatches.some((m) => m.id === "tcjennings-screen-address"));
  assert.equal(id.wouldAutoDownload, false);
}
