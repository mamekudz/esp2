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
} from "../../dev/tools/media/import.mjs";
import { MEDIA_FORMAT } from "../../dev/tools/media/import.mjs";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../..");

export function testMediaIdentifySynthetic() {
  const diskPath = path.join(
    root,
    "fixtures/synthetic/library/DemoCart/disk1.dsk.bin",
  );
  if (!fs.existsSync(diskPath)) {
    // blank disk may be generated on demand
    const buf = Buffer.alloc(143360, 0);
    fs.mkdirSync(path.dirname(diskPath), { recursive: true });
    fs.writeFileSync(diskPath, buf);
  }
  const buf = fs.readFileSync(diskPath);
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
  fs.writeFileSync(src, Buffer.alloc(143360, 0));

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
  assert.equal(result.mediaInstalled, true);
  assert.ok(fs.existsSync(result.destPath));

  meta = refreshMediaInstalledFlags(gameJsonPath, gameDir);
  assert.equal(meta.mediaStatus, "installed");
  assert.equal(meta.disks[0].mediaInstalled, true);

  // malformed size
  const bad = path.join(tmp, "bad.dsk");
  fs.writeFileSync(bad, Buffer.alloc(100, 0));
  const fail = importUserMedia({
    sourcePath: bad,
    libraryRoot: lib,
    gameId: "bad",
  });
  assert.equal(fail.ok, false);
}
