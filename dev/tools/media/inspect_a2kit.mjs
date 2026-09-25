#!/usr/bin/env node
/**
 * Optional a2kit oracle (external). Never required for build/test.
 *
 *   node dev/tools/media/inspect_a2kit.mjs --file path
 */

import { spawnSync } from "node:child_process";
import { resolve } from "node:path";
import { existsSync } from "node:fs";
import { identifyMedia } from "./import.mjs";

const file = process.argv.includes("--file")
  ? process.argv[process.argv.indexOf("--file") + 1]
  : process.argv[2];

if (!file) {
  console.error("Usage: node dev/tools/media/inspect_a2kit.mjs --file <path>");
  process.exit(1);
}

const abs = resolve(file);
const local = identifyMedia({ path: abs });

const a2kit = process.env.ESP2_A2KIT || "a2kit";
const probe = spawnSync(a2kit, ["--help"], { encoding: "utf8", shell: false });
if (probe.error || probe.status === null) {
  console.log(
    JSON.stringify(
      {
        ok: true,
        a2kitAvailable: false,
        note: "a2kit not on PATH; ESP][ identify-only result below",
        identify: local,
      },
      null,
      2,
    ),
  );
  process.exit(0);
}

// Best-effort: many a2kit versions support `a2kit info` / `a2kit get`
const info = spawnSync(a2kit, ["info", abs], { encoding: "utf8", shell: false });
console.log(
  JSON.stringify(
    {
      ok: true,
      a2kitAvailable: true,
      identify: local,
      a2kit: {
        status: info.status,
        stdout: (info.stdout || "").slice(0, 4000),
        stderr: (info.stderr || "").slice(0, 1000),
      },
    },
    null,
    2,
  ),
);
