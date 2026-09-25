#!/usr/bin/env node
/**
 * Offline media identify / import CLI (source-independent).
 *
 *   node dev/tools/media/cli.mjs identify --file path
 *   node dev/tools/media/cli.mjs import --file path [--library dir] [--title T] [--game-id id]
 *   node dev/tools/media/cli.mjs import --directory path [--library dir]
 */

import { resolve, join } from "node:path";
import {
  identifyMedia,
  importMedia,
  listImportableFilesInDirectory,
} from "./import.mjs";
import {
  createApple2jsAuditProvider,
  createCatalogHub,
  createKnownHashesProvider,
  createLocalMetadataProvider,
} from "../catalog/providers.mjs";
import { ESP2_ROOT } from "../apple2js/paths.mjs";

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

function buildHub() {
  return createCatalogHub([
    createLocalMetadataProvider(join(ESP2_ROOT, "library/metadata")),
    createKnownHashesProvider(join(ESP2_ROOT, "docs/media/known-hashes.json")),
    createApple2jsAuditProvider(
      join(ESP2_ROOT, "docs/media/generated/apple2js-catalog-audit.json"),
    ),
  ]);
}

function main() {
  const cmd = process.argv[2];
  const args = process.argv.slice(3);
  const hub = buildHub();
  const hashDb = join(ESP2_ROOT, "docs/media/known-hashes.json");

  if (cmd === "identify") {
    const file = argValue(args, "--file") || args[0];
    if (!file) {
      console.error("identify requires --file <path>");
      process.exit(1);
    }
    const library =
      argValue(args, "--library") || join(ESP2_ROOT, "library/user");
    const result = identifyMedia({
      path: resolve(file),
      catalogHub: hub,
      hashDbPath: hashDb,
      libraryRoot: resolve(library),
    });
    console.log(JSON.stringify(result, null, 2));
    process.exit(result.ok ? 0 : 1);
  }

  if (cmd === "import") {
    const directory = argValue(args, "--directory");
    const library =
      argValue(args, "--library") ||
      argValue(args, "--target") ||
      join(ESP2_ROOT, "library/user");
    const dryRun = hasFlag(args, "--dry-run");
    const title = argValue(args, "--title");
    const gameId = argValue(args, "--game-id") || argValue(args, "--gameId");
    const diskLabel = argValue(args, "--disk-label");

    const files = [];
    if (directory) {
      files.push(...listImportableFilesInDirectory(resolve(directory)));
    } else {
      const file = argValue(args, "--file") || args[0];
      if (!file) {
        console.error("import requires --file or --directory");
        process.exit(1);
      }
      files.push(resolve(file));
    }

    const results = [];
    for (const sourcePath of files) {
      const r = importMedia({
        sourcePath,
        libraryRoot: resolve(library),
        title: title || undefined,
        gameId: gameId || undefined,
        diskLabel: diskLabel || undefined,
        catalogHub: hub,
        hashDbPath: hashDb,
        dryRun,
        preferGamesSubdir: true,
      });
      results.push(r);
    }
    console.log(JSON.stringify({ count: results.length, results }, null, 2));
    process.exit(results.every((r) => r.ok) ? 0 : 1);
  }

  console.log(`Usage:
  node dev/tools/media/cli.mjs identify --file <path>
  node dev/tools/media/cli.mjs import --file <path> [--library dir] [--title T] [--game-id id]
  node dev/tools/media/cli.mjs import --directory <dir> [--library dir]
`);
}

main();
