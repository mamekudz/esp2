/**
 * Pack + run local SingleStepTests undoc vectors (9c/9e/9f/cb).
 * Skips cleanly if JSON is absent (CI without forensic assets).
 *
 * Usage: node host/tools/run_sst_undoc.mjs
 */
import { spawnSync } from "node:child_process";
import { existsSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "../..");
const sstDir = join(root, "local/apple2/forensics/sst");
const ops = ["9c", "9e", "9f", "cb"];
const runner = join(root, "host/.out/sst_undoc_runner.exe");

function run(cmd, args) {
  const r = spawnSync(cmd, args, { cwd: root, encoding: "utf8", shell: false });
  if (r.stdout) process.stdout.write(r.stdout);
  if (r.stderr) process.stderr.write(r.stderr);
  return r.status ?? 1;
}

let missing = 0;
for (const op of ops) {
  if (!existsSync(join(sstDir, `${op}.json`))) {
    console.log(`SST skip ${op}: no local JSON`);
    ++missing;
  }
}
if (missing === ops.length) {
  console.log("SST undoc: no local vectors (OK for CI)");
  process.exit(0);
}
if (missing) {
  console.error("SST undoc: partial vector set");
  process.exit(2);
}

if (run(process.execPath, [join(root, "host/tools/sst_pack.mjs"), "--all"]) !== 0) {
  process.exit(1);
}

// Build runner if missing
if (!existsSync(runner)) {
  console.error(`missing ${runner} — run node host/tools/build_and_test_apple2.mjs first`);
  process.exit(1);
}

let rc = 0;
for (const op of ops) {
  const st = run(runner, [join(sstDir, `${op}.sstb`), "--max-fail-samples", "8"]);
  if (st !== 0) {
    rc = st;
  }
}
process.exit(rc);
