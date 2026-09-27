#!/usr/bin/env node
/**
 * Chain several boot-window uploads in one CDC session (firmware multi-WAIT).
 *
 *   node dev/tools/esp2-upload-chain.mjs --port COM5 \
 *     --file local/roms/system.rom --target /esp2/roms/system.rom \
 *     --file local/roms/diskii_apple2js_16.prom --target /esp2/roms/diskii.prom \
 *     --file local/apple2/disks/Galaxian.dsk --target /esp2/disks/Galaxian.dsk
 */
import { spawnSync } from "node:child_process";

function parseArgs(argv) {
  const out = { port: process.env.ESP2_PORT || "", pairs: [], listenMs: 50000 };
  for (let i = 0; i < argv.length; i++) {
    if (argv[i] === "--port") out.port = argv[++i];
    else if (argv[i] === "--listen-ms") out.listenMs = Number(argv[++i]);
    else if (argv[i] === "--file") {
      const file = argv[++i];
      if (argv[i + 1] !== "--target") throw new Error("--file needs --target");
      i++;
      const target = argv[++i];
      out.pairs.push({ file, target });
    }
  }
  return out;
}

const args = parseArgs(process.argv.slice(2));
if (!args.port || args.pairs.length === 0) {
  console.error("need --port and one or more --file/--target pairs");
  process.exit(2);
}

for (let i = 0; i < args.pairs.length; i++) {
  const { file, target } = args.pairs[i];
  console.log(`\n=== chain ${i + 1}/${args.pairs.length} ${file} -> ${target} ===`);
  const r = spawnSync(
    process.execPath,
    [
      "dev/tools/esp2-upload.mjs",
      "--port",
      args.port,
      "--file",
      file,
      "--target",
      target,
      "--boot-window",
      "--listen-ms",
      String(args.listenMs),
    ],
    { encoding: "utf8", stdio: "inherit" },
  );
  if (r.status !== 0) {
    process.exit(r.status ?? 1);
  }
}
console.log("\nCHAIN_OK", args.pairs.length);
