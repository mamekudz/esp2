/**
 * Real-software compatibility harness (no proprietary media in repo).
 *
 * Usage:
 *   node host/tools/compat_runner.mjs --test <id>
 *   node host/tools/compat_runner.mjs --list
 *   node host/tools/compat_runner.mjs --test esp2-boot-test --run
 *
 * Statuses: PASS FAIL SKIPPED_NO_SYSTEM_ROM SKIPPED_NO_SLOT6_ROM
 *           SKIPPED_NO_MEDIA UNSUPPORTED_* TIMEOUT BLOCKED_MISSING_ASSET
 */
import fs from "node:fs";
import path from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";
import {
  applyCliOverrides,
  identifyAsset,
  loadMachineConfig,
  projectRoot,
  resolveUserMediaPath,
} from "./machine_config.mjs";

const root = projectRoot();
const testsDir = path.join(root, "compatibility/tests");

function parseArgs(argv) {
  const out = { test: null, list: false, run: false, config: null, rom: null, slot6Rom: null, disk1: null };
  for (let i = 2; i < argv.length; ++i) {
    const a = argv[i];
    if (a === "--list") out.list = true;
    else if (a === "--run") out.run = true;
    else if (a === "--test" && argv[i + 1]) out.test = argv[++i];
    else if (a === "--config" && argv[i + 1]) out.config = argv[++i];
    else if (a === "--rom" && argv[i + 1]) out.rom = argv[++i];
    else if ((a === "--slot6-rom" || a === "--slot6Rom") && argv[i + 1]) out.slot6Rom = argv[++i];
    else if (a === "--disk1" && argv[i + 1]) out.disk1 = argv[++i];
  }
  return out;
}

export function listCompatTests() {
  return fs
    .readdirSync(testsDir)
    .filter((f) => f.endsWith(".json"))
    .map((f) => JSON.parse(fs.readFileSync(path.join(testsDir, f), "utf8")));
}

export function loadCompatTest(id) {
  const p = path.join(testsDir, `${id}.json`);
  if (!fs.existsSync(p)) {
    throw new Error(`compat test not found: ${id}`);
  }
  return JSON.parse(fs.readFileSync(p, "utf8"));
}

export function evaluateAssetGate(test, cfg) {
  const report = {
    testId: test.id,
    status: "PASS",
    hostStatus: test.hostStatus || "NOT_TESTED",
    esp32Status: test.esp32Status || "NOT_TESTED",
    assets: {},
    checkpoints: [],
    evidence: {
      commitHint: "see git HEAD",
      machineProfile: cfg.machine,
      notes: [],
    },
  };

  const isProjectOwned = test.redistribution === "PROJECT_OWNED";

  if (!isProjectOwned) {
    const needRom = test.romProfile === "user_ii_plus";
    if (needRom) {
      const romId = identifyAsset(cfg.rom);
      report.assets.systemRom = romId;
      if (!romId.present) {
        report.status = "SKIPPED_NO_SYSTEM_ROM";
        report.hostStatus = "BLOCKED_MISSING_ASSET";
        return report;
      }
    }

    const slot6 = identifyAsset(cfg.slot6Rom);
    report.assets.slot6Rom = slot6;
    if (test.slot6Mode === "user" && !slot6.present) {
      report.status = "SKIPPED_NO_SLOT6_ROM";
      report.hostStatus = "BLOCKED_MISSING_ASSET";
      return report;
    }

    const catalogFile = test.media?.disk1?.catalogFile;
    const mediaPath = cfg.disk1 || resolveUserMediaPath(catalogFile);
    const mediaId = identifyAsset(mediaPath);
    report.assets.disk1 = mediaId;
    if (test.media?.disk1?.sha256 && mediaId.present && mediaId.sha256 !== test.media.disk1.sha256) {
      report.status = "FAIL";
      report.evidence.notes.push("disk1 sha256 mismatch vs test definition");
      return report;
    }
    if (!mediaId.present) {
      report.status = "SKIPPED_NO_MEDIA";
      report.hostStatus = "BLOCKED_MISSING_ASSET";
      return report;
    }
  } else {
    report.assets.disk1 = {
      present: true,
      id: test.media?.disk1?.id || "Esp2BootTest",
      type: "project_owned_generated",
    };
  }

  return report;
}

function runProjectOwnedBoot(test, report) {
  const exe = path.join(root, "host/.out/esp2_host.exe");
  if (!fs.existsSync(exe)) {
    // Build suite first
    const build = spawnSync("node", ["host/tools/build_and_test_apple2.mjs"], {
      cwd: root,
      encoding: "utf8",
      shell: false,
    });
    if (build.status !== 0) {
      report.status = "FAIL";
      report.evidence.notes.push("host build failed");
      return report;
    }
  }
  // Level-4 fixture path: mount Esp2BootTest via machine API is in C++ tests.
  // For CLI, use synthetic ROM + disk1 Esp2BootTest and cleanroom slot6.
  const args = [
    "--slot6",
    "cleanroom",
    "--disk1",
    "Esp2BootTest",
    "--cycles",
    String(Math.min(test.bootCycleBudget || 2000000, 5000000)),
    "--diagnostics",
    "--text",
    "--batch",
  ];
  const r = spawnSync(exe, args, { cwd: root, encoding: "utf8", shell: false });
  report.evidence.hostStdoutTail = (r.stdout || "").slice(-800);
  if (r.status !== 0) {
    report.status = "FAIL";
    return report;
  }
  // Automated Level-4 markers are asserted by test_level4.exe; CLI confirms process path.
  report.checkpoints.push({ type: "cliBatch", result: "ok" });
  report.status = "PASS";
  report.hostStatus = "COMPLETED_TEST_PATH";
  report.evidence.notes.push(
    "Project-owned Esp2BootTest exercised via esp2_host batch; detailed markers covered by test_level4",
  );
  return report;
}

export function runCompatTest(id, opts = {}) {
  const test = loadCompatTest(id);
  let cfg = loadMachineConfig(opts.config);
  cfg = applyCliOverrides(cfg, opts);
  let report = evaluateAssetGate(test, cfg);

  if (report.status !== "PASS" && report.status.startsWith("SKIPPED")) {
    return report;
  }
  if (report.status === "FAIL") {
    return report;
  }

  if (!opts.run) {
    report.status = "PASS";
    report.evidence.notes.push("Asset gate OK; pass --run to execute bounded host session");
    return report;
  }

  if (test.redistribution === "PROJECT_OWNED") {
    return runProjectOwnedBoot(test, report);
  }

  // User media present: launch bounded host (no title-specific hacks).
  const exe = path.join(root, "host/.out/esp2_host.exe");
  if (!fs.existsSync(exe)) {
    report.status = "FAIL";
    report.evidence.notes.push("esp2_host.exe missing — run npm run test:apple2 first");
    return report;
  }
  const args = ["--batch", "--diagnostics", "--text", "--cycles", String(test.bootCycleBudget || 2000000)];
  if (cfg.rom) args.push("--rom", cfg.rom);
  if (cfg.slot6Rom) args.push("--slot6-rom", cfg.slot6Rom);
  else if (cfg.slot6Mode) args.push("--slot6", cfg.slot6Mode);
  if (report.assets.disk1?.path) args.push("--disk1", report.assets.disk1.path);
  else if (cfg.disk1) args.push("--disk1", cfg.disk1);
  if (cfg.machine) args.push("--machine", cfg.machine);

  const r = spawnSync(exe, args, { cwd: root, encoding: "utf8", shell: false, timeout: 120000 });
  report.evidence.hostStdoutTail = (r.stdout || "").slice(-1200);
  if (r.error && r.error.code === "ETIMEDOUT") {
    report.status = "TIMEOUT";
    return report;
  }
  if (r.status !== 0) {
    report.status = "FAIL";
    return report;
  }
  report.status = "PASS";
  report.hostStatus = "BOOTS";
  report.evidence.notes.push(
    "Bounded host run completed without crash; interactive/playable requires manual checkpoints",
  );
  return report;
}

function main() {
  const args = parseArgs(process.argv);
  if (args.list) {
    for (const t of listCompatTests()) {
      console.log(`${t.id}\thost=${t.hostStatus}\tesp32=${t.esp32Status}\t${t.redistribution}`);
    }
    return;
  }
  if (!args.test) {
    console.log("Usage: node host/tools/compat_runner.mjs --list | --test <id> [--run] [--config path]");
    process.exit(args.list ? 0 : 2);
  }
  const report = runCompatTest(args.test, args);
  console.log(JSON.stringify(report, null, 2));
  if (report.status === "FAIL" || report.status === "TIMEOUT") process.exit(1);
  // SKIPPED_* are success for CI
  process.exit(0);
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  main();
}
