#!/usr/bin/env node
/**
 * Write / upload ESP][ device configuration (paths only — no media payloads).
 *
 *   node dev/tools/esp2-device-config.mjs --profile galaxian-demo --port COM5
 *   node dev/tools/esp2-device-config.mjs --dry-run --profile galaxian-demo
 */
import { existsSync, readFileSync, mkdirSync, copyFileSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "../..");

function arg(name, def = undefined) {
  const i = process.argv.indexOf(`--${name}`);
  if (i >= 0 && process.argv[i + 1] && !process.argv[i + 1].startsWith("--")) {
    return process.argv[i + 1];
  }
  return def;
}

function has(name) {
  return process.argv.includes(`--${name}`);
}

function main() {
  const profile = arg("profile", "galaxian-demo");
  const port = arg("port", process.env.ESP2_PORT || "COM5");
  const dry = has("dry-run");
  const outDir = join(ROOT, "local/device/config");
  const srcDir = join(ROOT, "config/device/profiles", profile);
  if (!existsSync(srcDir)) {
    console.error(`profile not found: ${srcDir}`);
    process.exit(1);
  }
  mkdirSync(outDir, { recursive: true });
  const sysSrc = join(srcDir, "system.json");
  const macSrc = join(srcDir, "macros.json");
  copyFileSync(sysSrc, join(outDir, "system.json"));
  copyFileSync(macSrc, join(outDir, "macros.json"));
  console.log(`[CONFIG] staged ${profile} → local/device/config/`);
  console.log(readFileSync(join(outDir, "system.json"), "utf8"));
  if (dry) {
    console.log("[CONFIG] dry-run — skip upload");
    return;
  }
  for (const [file, target] of [
    ["system.json", "/esp2/config/system.json"],
    ["macros.json", "/esp2/config/macros.json"],
  ]) {
    const r = spawnSync(
      process.execPath,
      [
        join(ROOT, "dev/tools/esp2-upload.mjs"),
        "--port",
        port,
        "--file",
        join(outDir, file),
        "--target",
        target,
      ],
      { stdio: "inherit", cwd: ROOT },
    );
    if (r.status !== 0) {
      process.exit(r.status || 1);
    }
  }
  console.log("[CONFIG] uploaded system.json + macros.json — power-cycle for standalone boot");
}

main();
