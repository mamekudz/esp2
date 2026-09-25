import assert from "node:assert/strict";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import { fileURLToPath } from "node:url";
import {
  detectMediaFormat,
  importUserMedia,
  lookupKnownHash,
  refreshMediaInstalledFlags,
  sha256FileBuffer,
  validateBasicMedia,
  writeGameJson,
  MEDIA_FORMAT,
} from "../../dev/tools/media/import.mjs";
import { generateSyntheticDisk } from "../../host/tools/gen_test_disks.mjs";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../..");

export function testMediaIdentifySynthetic() {
  const buf = generateSyntheticDisk("zero");
  const diskPath = path.join(os.tmpdir(), `esp2-blank-${Date.now()}.dsk`);
  fs.writeFileSync(diskPath, buf);
  const format = detectMediaFormat(diskPath, buf);
  assert.equal(format, MEDIA_FORMAT.Dsk);
  const v = validateBasicMedia(format, buf);
  assert.equal(v.ok, true);
  const sha = sha256FileBuffer(buf);
  assert.equal(sha.length, 64);
  const known = lookupKnownHash(
    path.join(root, "docs/media/known-hashes.json"),
    sha,
  );
  assert.equal(known, null);
}

export function testMediaImportAndNoMediaState() {
  const tmp = fs.mkdtempSync(path.join(os.tmpdir(), "esp2-media-"));
  const src = path.join(tmp, "user.dsk");
  fs.writeFileSync(src, generateSyntheticDisk("zero"));

  const lib = path.join(tmp, "library");
  const gameId = "demo-import";
  const gameDir = path.join(lib, gameId);
  fs.mkdirSync(gameDir, { recursive: true });
  const gameJsonPath = path.join(gameDir, "game.json");
  writeGameJson(gameJsonPath, {
    schemaVersion: 1,
    title: "Demo Import",
    disks: [{ file: "user.dsk", label: "disk1", mediaInstalled: false }],
    mediaStatus: "not_installed",
  });

  let meta = refreshMediaInstalledFlags(gameJsonPath, gameDir);
  assert.equal(meta.mediaStatus, "not_installed");
  assert.equal(meta.disks[0].mediaInstalled, false);

  const result = importUserMedia({
    sourcePath: src,
    libraryRoot: lib,
    gameId,
    destFileName: "user.dsk",
    hashDbPath: path.join(root, "docs/media/known-hashes.json"),
  });
  assert.equal(result.ok, true);

  meta = refreshMediaInstalledFlags(gameJsonPath, gameDir);
  assert.equal(meta.mediaStatus, "installed");
  assert.equal(meta.disks[0].mediaInstalled, true);

  const bad = path.join(tmp, "bad.dsk");
  fs.writeFileSync(bad, Buffer.alloc(100, 0));
  const fail = importUserMedia({
    sourcePath: bad,
    libraryRoot: lib,
    gameId: "bad",
  });
  assert.equal(fail.ok, false);
}
