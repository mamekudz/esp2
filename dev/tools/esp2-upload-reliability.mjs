#!/usr/bin/env node
/**
 * 10 consecutive harmless media uploads — reliability gate before ROM/disk transfer.
 *
 *   node dev/tools/esp2-upload-reliability.mjs --port COM5 [--rounds 10]
 *
 * Uses boot-window open (port open resets USB-Serial/JTAG into #ESP2UPLOAD WAIT).
 */

import { createHash, randomBytes } from "node:crypto";
import { writeFileSync, unlinkSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { spawnSync } from "node:child_process";

function parseArgs(argv) {
  const out = { port: process.env.ESP2_PORT || "", rounds: 10 };
  for (let i = 0; i < argv.length; i++) {
    if (argv[i] === "--port") out.port = argv[++i];
    else if (argv[i] === "--rounds") out.rounds = Number(argv[++i]);
  }
  return out;
}

function uploadOnce(port, file, target) {
  return spawnSync(
    process.execPath,
    [
      resolve("dev/tools/esp2-upload.mjs"),
      "--port",
      port,
      "--file",
      file,
      "--target",
      target,
      "--boot-window",
      "--listen-ms",
      "55000",
    ],
    { encoding: "utf8", timeout: 180000 },
  );
}

function main() {
  const args = parseArgs(process.argv.slice(2));
  if (!args.port) {
    console.error("ERROR: --port required");
    process.exit(2);
  }

  let pass = 0;
  for (let i = 1; i <= args.rounds; i++) {
    const payload = randomBytes(4096 + (i * 17) % 200);
    const local = join(tmpdir(), `esp2-upload-rel-${i}.bin`);
    const target = `/esp2/tmp/reltest_${i}.bin`;
    writeFileSync(local, payload);
    const sha = createHash("sha256").update(payload).digest("hex");
    console.log(`\n=== round ${i}/${args.rounds} size=${payload.length} sha=${sha.slice(0, 16)}… ===`);

    let r = uploadOnce(args.port, local, target);
    // One retry — open/reset timing can miss the WAIT banner once.
    if (r.status !== 0) {
      console.log("retry round", i);
      r = uploadOnce(args.port, local, target);
    }
    try {
      unlinkSync(local);
    } catch {
      /* ignore */
    }
    if (r.status !== 0) {
      console.error(r.stdout);
      console.error(r.stderr);
      console.error(`FAIL round ${i}: upload exit ${r.status}`);
      process.exit(1);
    }
    const out = `${r.stdout || ""}\n${r.stderr || ""}`;
    if (!out.includes('"ok": true') && !out.includes('"ok":true')) {
      console.error(out);
      console.error(`FAIL round ${i}: missing ok JSON`);
      process.exit(1);
    }
    if (!out.toLowerCase().includes(sha.toLowerCase())) {
      console.error(out);
      console.error(`FAIL round ${i}: SHA mismatch in response`);
      process.exit(1);
    }
    pass++;
    console.log(`PASS round ${i}`);
  }
  console.log(`\nRELIABILITY ${pass}/${args.rounds} PASS`);
}

main();
