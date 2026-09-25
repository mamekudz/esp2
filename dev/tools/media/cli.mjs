#!/usr/bin/env node
/**
 * Offline media identify / import CLI.
 *
 *   node dev/tools/media/cli.mjs identify path/to/disk.dsk
 *   node dev/tools/media/cli.mjs import path/to/disk.dsk --game-id demo-cart --library fixtures/synthetic/library
 */

import { readFileSync } from "node:fs";
import { resolve } from "node:path";
import {
  detectMediaFormat,
  importUserMedia,
  lookupKnownHash,
  sha256FileBuffer,
  validateBasicMedia,
} from "./import.mjs";
import { ESP2_ROOT } from "../apple2js/paths.mjs";

const cmd = process.argv[2];
const args = process.argv.slice(3);

function getFlag(name) {
  const i = args.indexOf(name);
  if (i >= 0 && args[i + 1]) return args[i + 1];
  return null;
}

function main() {
  if (cmd === "identify") {
    const path = args[0];
    if (!path) {
      console.error("identify requires a file path");
      process.exit(1);
    }
    const abs = resolve(path);
    const buf = readFileSync(abs);
    const format = detectMediaFormat(abs, buf);
    const validation = validateBasicMedia(format, buf);
    const sha256 = sha256FileBuffer(buf);
    const db = resolve(
      ESP2_ROOT,
      getFlag("--hash-db") || "docs/media/known-hashes.json",
    );
    const known = lookupKnownHash(db, sha256);
    console.log(
      JSON.stringify(
        { path: abs, format, validation, sha256, knownMatch: known },
        null,
        2,
      ),
    );
    return;
  }

  if (cmd === "import") {
    const path = args[0];
    const gameId = getFlag("--game-id");
    const library = getFlag("--library") || "library/user";
    if (!path || !gameId) {
      console.error("import requires path and --game-id");
      process.exit(1);
    }
    const result = importUserMedia({
      sourcePath: resolve(path),
      libraryRoot: resolve(ESP2_ROOT, library),
      gameId,
      hashDbPath: resolve(
        ESP2_ROOT,
        getFlag("--hash-db") || "docs/media/known-hashes.json",
      ),
    });
    console.log(JSON.stringify(result, null, 2));
    process.exit(result.ok ? 0 : 1);
  }

  console.log(`Usage:
  node dev/tools/media/cli.mjs identify <file> [--hash-db path]
  node dev/tools/media/cli.mjs import <file> --game-id <id> [--library dir]
`);
}

main();
