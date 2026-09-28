/**
 * Temporary physical Little Brick Out chain (LOCAL forensic).
 * Upload/verify disk → apply profile paths via MOUNT/BOOT serial → monitor LORES.
 * Does not modify Galaxian.dsk.
 */
import { spawn, spawnSync } from "node:child_process";
import { createHash } from "node:crypto";
import { readFileSync } from "node:fs";
import { SerialPort } from "serialport";
import { createLineReader } from "./esp2-serial-framing.mjs";
import { applyConfigToDevice, saveConfigLocal } from "./device-config-form.mjs";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "../..");
const PORT = process.env.ESP2_PORT || process.argv[2] || "COM5";
const DISK_LOCAL = join(ROOT, "local/apple2/disks/LittleBrickOut.dsk");
const DISK_DEV = "/esp2/disks/LittleBrickOut.dsk";
const EXPECT = createHash("sha256").update(readFileSync(DISK_LOCAL)).digest("hex");

async function writeLine(port, line) {
  await new Promise((resolve, reject) => {
    port.write(`${line}\n`, (err) => (err ? reject(err) : resolve()));
  });
  await new Promise((resolve, reject) => {
    port.drain((err) => (err ? reject(err) : resolve()));
  });
}

function runUpload() {
  console.log(`# upload ${DISK_LOCAL} → ${DISK_DEV}`);
  const r = spawnSync(
    process.execPath,
    [
      "dev/tools/esp2-upload.mjs",
      "--port",
      PORT,
      "--file",
      DISK_LOCAL,
      "--target",
      DISK_DEV,
      "--listen-ms",
      "30000",
    ],
    { cwd: ROOT, encoding: "utf8", stdio: "inherit" },
  );
  return r.status === 0;
}

function runVerify() {
  console.log(`# verify ${DISK_DEV} expect=${EXPECT}`);
  const r = spawnSync(
    process.execPath,
    [
      "dev/tools/esp2-upload.mjs",
      "--port",
      PORT,
      "--verify",
      DISK_DEV,
      "--listen-ms",
      "20000",
    ],
    { cwd: ROOT, encoding: "utf8", stdio: "inherit" },
  );
  return r.status === 0;
}

async function main() {
  console.log(`# Little Brick Out physical PORT=${PORT} sha256=${EXPECT}`);

  // Seed local config from tracked profile (does not touch Galaxian profile files)
  const sys = JSON.parse(
    readFileSync(join(ROOT, "config/device/profiles/little-brick-out/system.json"), "utf8"),
  );
  saveConfigLocal(ROOT, sys, "little-brick-out", "little-brick-out");

  if (!runUpload()) {
    console.error("# FAIL upload");
    process.exit(1);
  }
  if (!runVerify()) {
    console.error("# FAIL verify");
    process.exit(1);
  }

  console.log("# apply config JSON");
  applyConfigToDevice(ROOT, PORT);

  const port = new SerialPort({ path: PORT, baudRate: 115200, autoOpen: false });
  await new Promise((res, rej) => port.open((e) => (e ? rej(e) : res())));
  await new Promise((r) => port.set({ dtr: false, rts: false }, () => r()));
  await new Promise((r) => setTimeout(r, 200));
  const reader = createLineReader(port);

  const interesting = (l) =>
    /NAK|ACK|DISK|ROM|panic|Guru|SELFTEST|UPLOAD|INPUT|BOOT|PERF|APPLE2|SPEAKER|LEVEL|SCHED|VIDEO|LORES|HGR|MIXED|MACRO|PRESENT|MONITOR|EFFECT|DIRTY|cps|PDL|PAD|SCREEN|FAIL/.test(
      l,
    );

  console.log("# MOUNT + BOOT + PRESENT WHITE/CLEAN");
  await writeLine(port, `#ESP2DISK MOUNT 1 ${DISK_DEV}`);
  await writeLine(port, `#ESP2PRESENT MONITOR WHITE`);
  await writeLine(port, `#ESP2PRESENT EFFECT CLEAN`);
  await writeLine(port, `#ESP2DISK BOOT`);

  const until = Date.now() + 180000;
  let sawLores = false;
  let sawMixed = false;
  let cps = null;
  while (Date.now() < until) {
    try {
      const l = await reader.waitLine(() => true, 1000);
      if (interesting(l)) console.log(l);
      if (/LORES|MIXED|video=LORES/i.test(l)) {
        sawLores = true;
        if (/MIXED/i.test(l)) sawMixed = true;
        console.log(`# FLAG lores=${sawLores} mixed=${sawMixed} :: ${l}`);
      }
      const m = l.match(/cps[=:\s]+([0-9.]+)/i);
      if (m) cps = m[1];
      if (sawLores && Date.now() > until - 60000) break;
    } catch {
      /* timeout slice */
    }
  }

  console.log(`# RESULT sawLores=${sawLores} sawMixed=${sawMixed} cps=${cps || "?"}`);
  console.log("# leaving serial open drain 5s then exit (device keeps running)");
  await new Promise((r) => setTimeout(r, 5000));
  reader.dispose();
  await new Promise((r) => port.close(() => r()));
  process.exit(sawLores ? 0 : 2);
}

main().catch((e) => {
  console.error(e);
  process.exit(1);
});
