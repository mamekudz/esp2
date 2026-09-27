/**
 * Real-software compatibility harness (no proprietary media in repo).
 *
 * Usage:
 *   node host/tools/compat_runner.mjs --test <id>
 *   node host/tools/compat_runner.mjs --list
 *   node host/tools/compat_runner.mjs --test galaxian --run
 *
 * Statuses: PASS FAIL SKIPPED_NO_SYSTEM_ROM SKIPPED_NO_SLOT6_ROM
 *           SKIPPED_NO_MEDIA UNSUPPORTED_* TIMEOUT BLOCKED_MISSING_ASSET
 *           BLOCKED_MISSING_USER_APPLE_II_PLUS_ROM
 *           BLOCKED_REQUIRES_USER_APPLE_II_PLUS_ROM
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
  resolveLocalApple2Title,
  resolveUserAppleIIPlusRom,
  resolveUserMediaPath,
} from "./machine_config.mjs";

const root = projectRoot();
const testsDir = path.join(root, "compatibility/tests");

function parseArgs(argv) {
  const out = {
    test: null,
    list: false,
    run: false,
    config: null,
    rom: null,
    slot6Rom: null,
    disk1: null,
  };
  for (let i = 2; i < argv.length; ++i) {
    const a = argv[i];
    if (a === "--list") out.list = true;
    else if (a === "--run") out.run = true;
    else if (a === "--test" && argv[i + 1]) out.test = argv[++i];
    else if (a === "--config" && argv[i + 1]) out.config = argv[++i];
    else if (a === "--rom" && argv[i + 1]) out.rom = argv[++i];
    else if ((a === "--slot6-rom" || a === "--slot6Rom") && argv[i + 1])
      out.slot6Rom = argv[++i];
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

/**
 * Apply local/apple2 manifest paths when CLI/config did not override
 * with a *present* file.
 */
export function applyLocalApple2Assets(test, cfg) {
  const out = { ...cfg };
  const local =
    resolveLocalApple2Title(test.id) ||
    resolveLocalApple2Title(test.catalogId) ||
    resolveLocalApple2Title(
      test.media?.disk1?.catalogFile?.replace(/\.(dsk|po|nib)$/i, ""),
    );

  const romMissing = !out.rom || !fs.existsSync(out.rom);
  if (local?.rom?.path && romMissing) {
    out.rom = local.rom.path;
  }
  const diskMissing = !out.disk1 || !fs.existsSync(out.disk1);
  if (local?.disk?.path && diskMissing) {
    out.disk1 = local.disk.path;
  }
  if (!out.slot6Mode && test.slot6Mode === "cleanroom") {
    out.slot6Mode = "cleanroom";
  } else if (
    !out.slot6Mode &&
    (test.slot6Mode === "user_or_cleanroom" || !test.slot6Mode)
  ) {
    out.slot6Mode = "cleanroom";
  }
  // Prefer machine profile from test when using replacement ROM
  if (test.machineProfile && romMissing && local?.rom?.path) {
    out.machine = test.machineProfile;
  }
  out._localApple2 = local;
  return out;
}

export function evaluateAssetGate(test, cfg) {
  const report = {
    testId: test.id,
    status: "PASS",
    hostStatus: test.hostStatus || "NOT_TESTED",
    esp32Status: test.esp32Status || "NOT_TESTED",
    assets: {},
    checkpoints: [],
    furthestState: null,
    evidence: {
      commitHint: "see git HEAD",
      machineProfile: cfg.machine,
      notes: [],
    },
  };

  const isProjectOwned = test.redistribution === "PROJECT_OWNED";

  if (!isProjectOwned) {
    const needRom =
      test.romProfile === "user_ii_plus" ||
      test.romProfile === "appleiigo" ||
      test.romProfile === "replacement_pd";
    if (needRom) {
      const romId = identifyAsset(cfg.rom);
      report.assets.systemRom = romId;
      // user_ii_plus may temporarily resolve AppleIIGo via local sync; the
      // romDependency gate below rejects it for titles that need Applesoft data.
      if (!romId.present && test.romDependency !== "USER_APPLE_II_PLUS_ROM") {
        report.status = "SKIPPED_NO_SYSTEM_ROM";
        report.hostStatus = "BLOCKED_MISSING_ASSET";
        report.evidence.notes.push(
          "Missing system ROM — run gulp apple2:rom:sync for AppleIIGo, or supply user ROM",
        );
        return report;
      }
      if (!romId.present && test.romDependency === "USER_APPLE_II_PLUS_ROM") {
        // Fall through to romDependency gate (clearer status).
        report.assets.systemRom = { present: false, path: null };
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
    const mediaPath =
      cfg.disk1 ||
      resolveUserMediaPath(catalogFile) ||
      (cfg._localApple2?.disk?.path ?? null);
    const mediaId = identifyAsset(mediaPath);
    report.assets.disk1 = mediaId;
    if (
      test.media?.disk1?.sha256 &&
      mediaId.present &&
      mediaId.sha256 !== test.media.disk1.sha256
    ) {
      report.status = "FAIL";
      report.evidence.notes.push("disk1 sha256 mismatch vs test definition");
      return report;
    }
    if (!mediaId.present) {
      report.status = "SKIPPED_NO_MEDIA";
      report.hostStatus = "BLOCKED_MISSING_ASSET";
      report.evidence.notes.push(
        `Missing disk — run gulp apple2:media:sync --title ${test.id} && gulp apple2:media:prepare --title ${test.id}`,
      );
      return report;
    }
  } else {
    report.assets.disk1 = {
      present: true,
      id: test.media?.disk1?.id || "Esp2BootTest",
      type: "project_owned_generated",
    };
  }

  // Titles that intentionally read Apple II+ Applesoft ROM *data* (not code).
  // AppleIIGo PD replacement is insufficient — do not auto-download proprietary ROMs.
  if (test.romDependency === "USER_APPLE_II_PLUS_ROM") {
    const userRom = resolveUserAppleIIPlusRom(cfg);
    report.assets.userAppleIIPlusRom = userRom;
    report.evidence.romDependency = {
      required: "USER_APPLE_II_PLUS_ROM",
      reason: test.romDependencyReason || "ROM_DATA",
      classification: test.appleiigoClassification || "APPLEIIGO_MISSING_REQUIRED_DATA",
    };
    if (!userRom.present) {
      report.status = "BLOCKED_MISSING_USER_APPLE_II_PLUS_ROM";
      // Furthest proven host state under AppleIIGo remains BOOT — not PLAYABLE.
      report.hostStatus = test.hostStatus || "BOOT";
      report.evidence.notes.push(
        "BLOCKED_REQUIRES_USER_APPLE_II_PLUS_ROM: place a legal 12288-byte Apple II+ system ROM in local/roms/ (or set config/roms.local.json motherboardRom). Runtime device path: /esp2/roms/system.rom. Do not use apple2:rom:sync for this asset (that syncs AppleIIGo PD only).",
      );
      return report;
    }
    // Prefer the user II+ ROM for subsequent --run.
    cfg.rom = userRom.path;
    report.assets.systemRom = identifyAsset(userRom.path);
  }

  return report;
}

function runProjectOwnedBoot(test, report) {
  const exe = path.join(root, "host/.out/esp2_host.exe");
  if (!fs.existsSync(exe)) {
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
  report.checkpoints.push({ type: "cliBatch", result: "ok" });
  report.status = "PASS";
  report.hostStatus = "COMPLETED_TEST_PATH";
  report.furthestState = "BOOT";
  report.evidence.notes.push(
    "Project-owned Esp2BootTest exercised via esp2_host batch; detailed markers covered by test_level4",
  );
  return report;
}

/**
 * Classify furthest smoke state from host diagnostics + text dump.
 * States: NONE | BOOT | TITLE_SCREEN | ATTRACT_MODE
 * No title-specific emulator hacks — only generic video/text heuristics.
 * Never treat filesystem paths in the log as on-screen title text.
 */
export function classifySmokeState(stdout, test) {
  const text = String(stdout || "");
  let furthest = "NONE";
  const videoMatch = text.match(/VIDEO\s+(\S+)/);
  const video = videoMatch ? videoMatch[1] : "";
  const driveMatch = text.match(/DRIVE1\s+(\S+)/);
  const driveInserted = driveMatch && driveMatch[1] !== "(empty)";
  const pcMatch = text.match(/PC\s+([0-9A-Fa-f]{4})/);
  const pc = pcMatch ? parseInt(pcMatch[1], 16) : null;

  // Only the dumped text screen — not paths / diagnostics lines.
  const screenMatch = text.match(/---- TEXT ----\r?\n([\s\S]*?)\r?\n--------------/);
  const screen = screenMatch ? screenMatch[1] : "";
  const screenUpper = screen.toUpperCase();

  const bootRomBanner = /APPLE\s*\]?\s*\[\s*GO|APPLE\s*II/i.test(screen);
  const inSlot6 = pc != null && pc >= 0xc600 && pc <= 0xc6ff;
  const leftResetIdle = pc != null && pc !== 0xff69 && pc !== 0;

  if (driveInserted && (leftResetIdle || inSlot6 || bootRomBanner)) {
    furthest = "BOOT";
  }

  const titleHint = String(test.title || test.id || "")
    .toUpperCase()
    .replace(/[^A-Z0-9]/g, "");
  const screenCompact = screenUpper.replace(/[^A-Z0-9]/g, "");
  const titleOnScreen =
    (titleHint.length >= 4 && screenCompact.includes(titleHint)) ||
    /INSERTCOIN|HIGHSCORE|PLAYER[12]|CREDIT/i.test(screenCompact);

  const graphics = /HGR|LORES/.test(video);

  if (graphics || (titleOnScreen && !bootRomBanner)) {
    furthest = "TITLE_SCREEN";
  }

  if (
    furthest === "TITLE_SCREEN" &&
    graphics &&
    (/ANIM|ATTRACT|DEMO|PRESS/i.test(screen) ||
      (pc != null && pc > 0x0800 && pc < 0xc000))
  ) {
    furthest = "ATTRACT_MODE";
    return {
      furthest,
      softAttract: !/ATTRACT|DEMO/i.test(screen),
      video,
      pc,
      titleVisible: titleOnScreen,
      screenPreview: screen.slice(0, 200),
    };
  }

  return {
    furthest,
    softAttract: false,
    video,
    pc,
    titleVisible: titleOnScreen,
    screenPreview: screen.slice(0, 200),
  };
}

export function runCompatTest(id, opts = {}) {
  const test = loadCompatTest(id);
  let cfg = loadMachineConfig(opts.config);
  cfg = applyCliOverrides(cfg, opts);
  cfg = applyLocalApple2Assets(test, cfg);
  let report = evaluateAssetGate(test, cfg);

  if (report.status !== "PASS" && report.status.startsWith("SKIPPED")) {
    return report;
  }
  if (
    report.status === "FAIL" ||
    report.status === "BLOCKED_MISSING_USER_APPLE_II_PLUS_ROM" ||
    report.status === "BLOCKED_REQUIRES_USER_APPLE_II_PLUS_ROM"
  ) {
    return report;
  }

  if (!opts.run) {
    report.status = "PASS";
    report.evidence.notes.push(
      "Asset gate OK; pass --run to execute bounded host session",
    );
    return report;
  }

  if (test.redistribution === "PROJECT_OWNED") {
    return runProjectOwnedBoot(test, report);
  }

  const exe = path.join(root, "host/.out/esp2_host.exe");
  if (!fs.existsSync(exe)) {
    report.status = "FAIL";
    report.evidence.notes.push(
      "esp2_host.exe missing — run npm run test:apple2 first",
    );
    return report;
  }
  const args = [
    "--batch",
    "--diagnostics",
    "--text",
    "--cycles",
    String(test.bootCycleBudget || 2000000),
  ];
  if (cfg.rom) args.push("--rom", cfg.rom);
  if (cfg.slot6Rom) args.push("--slot6-rom", cfg.slot6Rom);
  else if (cfg.slot6Mode) args.push("--slot6", cfg.slot6Mode);
  if (report.assets.disk1?.path) args.push("--disk1", report.assets.disk1.path);
  else if (cfg.disk1) args.push("--disk1", cfg.disk1);
  if (cfg.machine) args.push("--machine", cfg.machine);
  // AppleIIGo / non-Autostart ROMs need an explicit Slot-6 entry (generic, not title-specific).
  if (
    test.romProfile === "appleiigo" ||
    test.romProfile === "replacement_pd" ||
    test.romProfile === "user_ii_plus" ||
    test.bootDisk === true
  ) {
    args.push("--boot-disk");
  }

  const r = spawnSync(exe, args, {
    cwd: root,
    encoding: "utf8",
    shell: false,
    timeout: 180000,
  });
  report.evidence.hostStdoutTail = (r.stdout || "").slice(-2000);
  if (r.error && r.error.code === "ETIMEDOUT") {
    report.status = "TIMEOUT";
    report.furthestState = "NONE";
    return report;
  }
  if (r.status !== 0) {
    report.status = "FAIL";
    report.furthestState = "NONE";
    report.evidence.notes.push(`esp2_host exit=${r.status}`);
    return report;
  }

  const smoke = classifySmokeState(r.stdout || "", test);
  report.furthestState = smoke.furthest;
  report.evidence.smoke = smoke;
  report.checkpoints.push({ type: "smokeState", result: smoke.furthest });

  const desired = test.smokeTargets || ["BOOT"];
  const order = ["NONE", "BOOT", "TITLE_SCREEN", "ATTRACT_MODE"];
  const got = order.indexOf(smoke.furthest);
  const need = order.indexOf(desired[desired.length - 1] || "BOOT");
  if (got >= order.indexOf("BOOT")) {
    report.status = "PASS";
    report.hostStatus = smoke.furthest;
    report.evidence.notes.push(
      `Host smoke furthest=${smoke.furthest} video=${smoke.video || "?"} (no input/audio required)`,
    );
    if (got < need) {
      report.evidence.notes.push(
        `Reached ${smoke.furthest}; target chain ${desired.join("→")} not fully observed`,
      );
    }
  } else {
    report.status = "FAIL";
    report.hostStatus = "NO_BOOT";
    report.evidence.notes.push("Host ran but smoke classifier did not observe BOOT");
  }
  return report;
}

function main() {
  const args = parseArgs(process.argv);
  if (args.list) {
    for (const t of listCompatTests()) {
      console.log(
        `${t.id}\thost=${t.hostStatus}\tesp32=${t.esp32Status}\t${t.redistribution}`,
      );
    }
    return;
  }
  if (!args.test) {
    console.log(
      "Usage: node host/tools/compat_runner.mjs --list | --test <id> [--run] [--config path]",
    );
    process.exit(args.list ? 0 : 2);
  }
  const report = runCompatTest(args.test, args);
  console.log(JSON.stringify(report, null, 2));
  if (report.status === "FAIL" || report.status === "TIMEOUT") process.exit(1);
  process.exit(0);
}

if (
  process.argv[1] &&
  path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)
) {
  main();
}
