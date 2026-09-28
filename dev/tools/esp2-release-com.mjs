#!/usr/bin/env node
/**
 * Release Windows COM ports held by ESP][ host tools (input bridge, PIO monitor, uploads).
 *
 *   node dev/tools/esp2-release-com.mjs
 *   node dev/tools/esp2-release-com.mjs --port COM5
 *   node dev/tools/esp2-release-com.mjs --dry-run
 */
import { spawnSync } from "node:child_process";
import { SerialPort } from "serialport";

/** Command-line needles for processes that commonly lock ESP][ CDC. */
const HOLDER_NEEDLES = [
  "esp2-input-bridge",
  "esp2-upload",
  "esp2-macro-run",
  "esp2-release-com", // never match self via full path alone — filtered below
  "device monitor",
  "pio device monitor",
  "platformio.exe\" monitor",
  "platformio\" monitor",
  "serialport-list",
];

function parseArgs(argv) {
  const out = {
    port: process.env.ESP2_PORT || "",
    dryRun: false,
    help: false,
  };
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    if (a === "--port") out.port = String(argv[++i] || "");
    else if (a === "--dry-run") out.dryRun = true;
    else if (a === "--help" || a === "-h") out.help = true;
  }
  return out;
}

/**
 * @returns {Array<{ pid: number, name: string, cmd: string }>}
 */
export function listSerialHolderCandidates() {
  const ps = `
Get-CimInstance Win32_Process |
  Where-Object { $_.CommandLine } |
  ForEach-Object {
    '{0}\t{1}\t{2}' -f $_.ProcessId, $_.Name, ($_.CommandLine -replace '[\\r\\n\\t]+',' ')
  }
`;
  const r = spawnSync(
    "powershell.exe",
    ["-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", ps],
    { encoding: "utf8", windowsHide: true },
  );
  if (r.status !== 0 && !r.stdout) {
    throw new Error(r.stderr || `process list exit ${r.status}`);
  }
  const selfPid = process.pid;
  const rows = [];
  for (const line of String(r.stdout || "").split(/\r?\n/)) {
    if (!line.trim()) continue;
    const tab = line.indexOf("\t");
    if (tab < 0) continue;
    const tab2 = line.indexOf("\t", tab + 1);
    if (tab2 < 0) continue;
    const pid = Number(line.slice(0, tab));
    const name = line.slice(tab + 1, tab2);
    const cmd = line.slice(tab2 + 1);
    if (!Number.isFinite(pid) || pid === selfPid) continue;
    const lower = cmd.toLowerCase();
    // Never kill Cursor / µGulp host / this release tool's parent shell blindly.
    if (lower.includes("esp2-release-com")) continue;
    if (lower.includes("\\cursor\\") && lower.includes("gulpfile")) continue;
    const hit = HOLDER_NEEDLES.some((n) => {
      if (n === "esp2-release-com") return false;
      return lower.includes(n.toLowerCase());
    });
    if (!hit) continue;
    rows.push({ pid, name, cmd });
  }
  return rows;
}

/**
 * @param {number} pid
 * @param {boolean} dryRun
 */
function killPid(pid, dryRun) {
  if (dryRun) return { ok: true, dryRun: true };
  const r = spawnSync(
    "taskkill",
    ["/PID", String(pid), "/T", "/F"],
    { encoding: "utf8", windowsHide: true },
  );
  return { ok: r.status === 0, status: r.status, out: r.stdout, err: r.stderr };
}

/**
 * Probe whether a COM port can be opened exclusively.
 * @param {string} port
 */
export async function probeComPort(port) {
  if (!port) return { ok: false, reason: "no port" };
  try {
    const sp = new SerialPort({ path: port, baudRate: 115200, autoOpen: false });
    await new Promise((resolve, reject) => {
      sp.open((err) => (err ? reject(err) : resolve()));
    });
    await new Promise((resolve) => sp.close(() => resolve()));
    return { ok: true };
  } catch (e) {
    return { ok: false, reason: e?.message || String(e) };
  }
}

/**
 * Kill known ESP][ serial holders and optionally verify --port is free.
 * @param {{ port?: string, dryRun?: boolean, log?: (s: string) => void }} [opts]
 */
export async function releaseComPort(opts = {}) {
  const log = opts.log || ((s) => console.log(s));
  const dryRun = Boolean(opts.dryRun);
  const port = String(opts.port || "").trim();
  const holders = listSerialHolderCandidates();
  const killed = [];
  const failed = [];

  if (holders.length === 0) {
    log("no known ESP][ serial holder processes");
  }
  for (const h of holders) {
    log(`holder pid=${h.pid} name=${h.name} cmd=${h.cmd.slice(0, 160)}`);
    const r = killPid(h.pid, dryRun);
    if (r.ok) {
      killed.push(h.pid);
      log(dryRun ? `dry-run would kill pid=${h.pid}` : `killed pid=${h.pid}`);
    } else {
      failed.push(h.pid);
      log(`FAIL kill pid=${h.pid}: ${r.err || r.out || r.status}`);
    }
  }

  let probe = null;
  if (port) {
    // Brief settle after taskkill.
    await new Promise((r) => setTimeout(r, dryRun ? 0 : 400));
    probe = await probeComPort(port);
    log(
      probe.ok
        ? `probe ${port}: FREE`
        : `probe ${port}: BUSY (${probe.reason})`,
    );
  }

  return {
    holders,
    killed,
    failed,
    port: port || null,
    probe,
    dryRun,
  };
}

async function main() {
  const args = parseArgs(process.argv.slice(2));
  if (args.help) {
    console.log(`Usage:
  node dev/tools/esp2-release-com.mjs [--port COM5] [--dry-run]
Kills ESP][ host processes that typically lock CDC (input-bridge, pio monitor, upload).`);
    process.exit(0);
  }
  const result = await releaseComPort({
    port: args.port,
    dryRun: args.dryRun,
  });
  if (result.failed.length) process.exitCode = 1;
  else if (args.port && result.probe && !result.probe.ok) process.exitCode = 2;
}

const isMain =
  process.argv[1] &&
  (process.argv[1].endsWith("esp2-release-com.mjs") ||
    process.argv[1].endsWith("esp2-release-com"));
if (isMain) {
  main().catch((e) => {
    console.error("FAIL", e.message || e);
    process.exit(1);
  });
}
