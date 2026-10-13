/**
 * Physical micros()-wrap regression soak for Galaxian on ESP][.
 * Opens CDC with DTR/RTS off. Snapshots PC/cycles across ~71.6 min boundary.
 * Does NOT reset the board after connect (except optional first boot settle).
 */
import { SerialPort } from "serialport";
import { createLineReader } from "./esp2-serial-framing.mjs";
import { appendFileSync, mkdirSync, writeFileSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "../..");
const PORT = process.env.ESP2_PORT || process.argv[2] || "COM5";
const OUT_DIR = join(ROOT, "local/apple2/forensics");
const OUT = join(OUT_DIR, `galaxian_wrap_soak_${Date.now()}.ndjson`);
const DURATION_MS = Number(process.env.ESP2_SOAK_MS || 2 * 60 * 60 * 1000);
const SNAP_AT_MS = [0, 60, 75, 90, 120].map((m) => m * 60 * 1000);

mkdirSync(OUT_DIR, { recursive: true });

function logEvent(obj) {
  const line = JSON.stringify({ t: new Date().toISOString(), ...obj });
  appendFileSync(OUT, line + "\n", "utf8");
  console.log(line);
}

async function writeLine(port, line) {
  await new Promise((resolve, reject) => {
    port.write(`${line}\n`, (err) => (err ? reject(err) : resolve()));
  });
  await new Promise((resolve, reject) => {
    port.drain((err) => (err ? reject(err) : resolve()));
  });
}

function parseSnap(lines) {
  const out = {};
  for (const l of lines) {
    const pc = l.match(/pc=\$([0-9A-Fa-f]+)/);
    const cy = l.match(/cycles=(\d+)/);
    const vid = l.match(/video=(\S+)/);
    const lit = l.match(/hgr_lit=(\d+)/);
    const motor = l.match(/motor=(\d+)/);
    if (pc) out.pc = pc[1];
    if (cy) out.cycles = cy[1];
    if (vid) out.video = vid[1];
    if (lit) out.hgr_lit = lit[1];
    if (motor) out.motor = motor[1];
    const cps = l.match(/cps=([0-9.]+)/);
    if (cps) out.cps = cps[1];
    const frames = l.match(/frames=(\d+)/);
    if (frames) out.frames = frames[1];
    const heap = l.match(/heap=(\d+)/);
    if (heap) out.heap = heap[1];
    if (/GALAXIAN|ESP32_VISIBLE|CONFIG boot|ARTIFACT|LANDSCAPE/.test(l)) {
      out.note = (out.note || "") + l.slice(0, 120) + "|";
    }
  }
  return out;
}

async function main() {
  writeFileSync(OUT, "", "utf8");
  logEvent({ type: "start", port: PORT, durationMs: DURATION_MS, out: OUT });

  const port = new SerialPort({ path: PORT, baudRate: 115200, autoOpen: false });
  await new Promise((res, rej) => port.open((e) => (e ? rej(e) : res())));
  await new Promise((r) => port.set({ dtr: false, rts: false }, () => r()));
  await new Promise((r) => setTimeout(r, 400));
  const reader = createLineReader(port);

  const drainUntil = Date.now() + 8000;
  while (Date.now() < drainUntil) {
    try {
      const l = await reader.waitLine(() => true, 500);
      if (/ESP2|APPLE2|CONFIG|ROM|PERF|build=/.test(l)) logEvent({ type: "boot", line: l });
    } catch {
      /* timeout */
    }
  }

  // Ensure Galaxian is mounted/booted (config may still be Star Blazer).
  for (const cmd of [
    "#ESP2PRESENT MONITOR ARTIFACT",
    "#ESP2PRESENT ORIENT LANDSCAPE",
    "#ESP2PRESENT EFFECT CLEAN",
    "#ESP2DISK MOUNT 1 /esp2/disks/Galaxian.dsk",
    "#ESP2DISK BOOT",
  ]) {
    logEvent({ type: "cmd", cmd });
    await writeLine(port, cmd);
    await new Promise((r) => setTimeout(r, 800));
  }

  // Wait for HGR / playfield; arm macro if present.
  await writeLine(port, "#ESP2MACRO RUN galaxian-start");
  const bootWait = Date.now() + 120000;
  while (Date.now() < bootWait) {
    try {
      const l = await reader.waitLine(() => true, 2000);
      logEvent({ type: "line", line: l });
      if (/ESP32_VISIBLE|MACRO DONE|HGR|playfield/i.test(l)) break;
    } catch {
      /* continue */
    }
  }

  const t0 = Date.now();
  const snapsDone = new Set();
  let lastCycles = null;
  let wrapPass = false;
  let freezeSuspect = false;

  async function takeSnap(label) {
    const buf = [];
    await writeLine(port, "#ESP2DIAG SNAP");
    await writeLine(port, "#ESP2CONFIG STATUS");
    await writeLine(port, "#ESP2MACRO STATUS");
    const end = Date.now() + 3500;
    while (Date.now() < end) {
      try {
        const l = await reader.waitLine(() => true, 400);
        if (/DISK|PERF|ACK|CONFIG|MACRO|GALAXIAN|cps=|pc=\$/.test(l)) {
          buf.push(l);
          logEvent({ type: "raw", label, line: l });
        }
      } catch {
        /* timeout */
      }
    }
    // Wait briefly for next PERF (every 5s)
    const perfEnd = Date.now() + 6000;
    while (Date.now() < perfEnd) {
      try {
        const l = await reader.waitLine((x) => /\[PERF\]/.test(x), 5500);
        buf.push(l);
        logEvent({ type: "raw", label, line: l });
        break;
      } catch {
        break;
      }
    }
    const snap = parseSnap(buf);
    snap.label = label;
    snap.uptimeMin = ((Date.now() - t0) / 60000).toFixed(2);
    if (lastCycles != null && snap.cycles != null) {
      snap.cyclesDelta = String(BigInt(snap.cycles) - BigInt(lastCycles));
      if (snap.cyclesDelta === "0" && snap.pc && lastCycles) {
        freezeSuspect = true;
        snap.freezeSuspect = true;
      }
    }
    if (snap.cycles) lastCycles = snap.cycles;
    const uptimeMs = Date.now() - t0;
    if (uptimeMs > 71.6 * 60 * 1000 && snap.cyclesDelta && snap.cyclesDelta !== "0") {
      wrapPass = true;
      snap.wrapBoundaryOk = true;
    }
    logEvent({ type: "snap", ...snap });
    return snap;
  }

  await takeSnap("startup");

  while (Date.now() - t0 < DURATION_MS) {
    const elapsed = Date.now() - t0;
    for (const at of SNAP_AT_MS) {
      if (elapsed >= at && !snapsDone.has(at)) {
        snapsDone.add(at);
        await takeSnap(`t_${Math.round(at / 60000)}m`);
      }
    }
    // Light listen for PERF every ~30s between major snaps
    try {
      const l = await reader.waitLine((x) => /\[PERF\]|\[DISK\]/.test(x), 30000);
      logEvent({ type: "heartbeat", line: l, uptimeMin: (elapsed / 60000).toFixed(2) });
      const cy = l.match(/cycles=(\d+)/);
      // PERF doesn't have cycles — ignore
      void cy;
    } catch {
      /* idle */
    }
    if (freezeSuspect) {
      logEvent({ type: "freeze_capture", note: "cycles stuck — preserving state, not resetting" });
      await takeSnap("freeze");
      break;
    }
  }

  await takeSnap("final");
  logEvent({
    type: "done",
    MICROS_WRAP_REGRESSION: wrapPass ? "PASS" : "FAIL_OR_INCOMPLETE",
    GALAXIAN_2H_SOAK:
      Date.now() - t0 >= 2 * 60 * 60 * 1000 - 60000 && wrapPass ? "PASS" : "INCOMPLETE",
    out: OUT,
  });
  port.close();
}

main().catch((e) => {
  logEvent({ type: "fatal", error: String(e && e.message ? e.message : e) });
  process.exit(1);
});
