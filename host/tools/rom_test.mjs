/**
 * Optional Apple II / II+ ROM identify + bounded host exercise.
 *
 * Never downloads ROMs. Looks for:
 *   --rom <path>
 *   config/roms.local.json  { "motherboardRom": "..." }
 *   local/roms/*.rom
 *
 * Exit codes:
 *   0 PASS or SKIPPED_NO_ROM
 *   1 FAIL
 *   2 UNSUPPORTED / bad args
 */
import { createHash } from "node:crypto";
import { existsSync, readFileSync, readdirSync, mkdirSync } from "node:fs";
import { join, dirname, extname } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

const root = join(dirname(fileURLToPath(import.meta.url)), "../..");
const EXPECTED_SIZE = 12 * 1024;

function parseArgs(argv) {
  const out = { rom: null, machine: null, cycles: "200000", identifyOnly: false };
  for (let i = 0; i < argv.length; i++) {
    if (argv[i] === "--rom" && argv[i + 1]) out.rom = argv[++i];
    else if (argv[i] === "--machine" && argv[i + 1]) out.machine = argv[++i];
    else if (argv[i] === "--cycles" && argv[i + 1]) out.cycles = argv[++i];
    else if (argv[i] === "--identify-only") out.identifyOnly = true;
  }
  return out;
}

function loadLocalConfig() {
  const p = join(root, "config/roms.local.json");
  if (!existsSync(p)) return {};
  try {
    return JSON.parse(readFileSync(p, "utf8"));
  } catch {
    return {};
  }
}

function findRom(explicit) {
  if (explicit) return explicit;
  const cfg = loadLocalConfig();
  if (cfg.motherboardRom) {
    const p = join(root, cfg.motherboardRom);
    if (existsSync(p)) return p;
    if (existsSync(cfg.motherboardRom)) return cfg.motherboardRom;
  }
  const dir = join(root, "local/roms");
  if (!existsSync(dir)) return null;
  const files = readdirSync(dir).filter((f) => {
    const e = extname(f).toLowerCase();
    return e === ".rom" || e === ".bin";
  });
  if (files.length === 1) return join(dir, files[0]);
  if (cfg.motherboardFile && files.includes(cfg.motherboardFile)) {
    return join(dir, cfg.motherboardFile);
  }
  return null;
}

function identify(path) {
  const buf = readFileSync(path);
  const sha256 = createHash("sha256").update(buf).digest("hex");
  const dbPath = join(root, "host/data/rom_database.json");
  let known = null;
  if (existsSync(dbPath)) {
    const db = JSON.parse(readFileSync(dbPath, "utf8"));
    known = (db.entries || []).find((e) => e.sha256 === sha256) || null;
  }
  let status = "UNKNOWN_ROM";
  if (buf.length !== EXPECTED_SIZE) status = "INVALID_SIZE";
  else if (known) status = "OK";
  return {
    status,
    path,
    size: buf.length,
    sha256,
    name: known?.name || "unrecognized",
    profile: known?.profile || "Unknown",
    provenance: known?.provenance || "not in metadata DB",
  };
}

function ensureHostBuilt() {
  const exe = join(root, "host/.out/esp2_host.exe");
  if (existsSync(exe)) return exe;
  const r = spawnSync(process.execPath, [join(root, "host/tools/build_and_test_apple2.mjs")], {
    cwd: root,
    encoding: "utf8",
    shell: false,
  });
  if (r.status !== 0) {
    console.error("host build failed");
    process.exit(1);
  }
  return exe;
}

const args = parseArgs(process.argv.slice(2));
mkdirSync(join(root, "local/roms"), { recursive: true });

const romPath = findRom(args.rom);
if (!romPath) {
  console.log("RESULT SKIPPED_NO_ROM");
  console.log("Place a legal 12KiB Apple II/II+ ROM in local/roms/ or pass --rom <path>");
  process.exit(0);
}

const id = identify(romPath);
console.log(JSON.stringify({ result: id.status === "INVALID_SIZE" ? "FAIL" : "IDENTIFY", ...id }, null, 2));

if (id.status === "INVALID_SIZE") {
  console.log("RESULT FAIL");
  process.exit(1);
}

if (args.identifyOnly) {
  console.log("RESULT PASS");
  process.exit(0);
}

const exe = ensureHostBuilt();
const hostArgs = ["--rom", romPath, "--slot6", "none", "--cycles", args.cycles, "--diagnostics", "--text", "--batch"];
if (args.machine) {
  hostArgs.push("--machine", args.machine);
} else if (id.profile === "AppleII" || id.profile === "AppleIIPlus") {
  hostArgs.push("--machine", id.profile);
} else {
  hostArgs.push("--machine", "AppleIIPlus");
}

const run = spawnSync(exe, hostArgs, { cwd: root, encoding: "utf8", shell: false });
if (run.stdout) process.stdout.write(run.stdout);
if (run.stderr) process.stderr.write(run.stderr);
if (run.status !== 0) {
  console.log("RESULT FAIL");
  process.exit(1);
}
console.log("RESULT PASS");
process.exit(0);
