#!/usr/bin/env node
/** Boot Galaxian → LANDSCAPE + ARTIFACT; leave running for visual SCORE check. */
import { SerialPort } from "serialport";
import { createLineReader, DevCommand } from "./esp2-serial-framing.mjs";

const DISK = "/esp2/disks/Galaxian.dsk";
const PORT = process.env.ESP2_PORT || process.argv[2] || "COM5";

async function writeLine(port, line) {
  await new Promise((res, rej) => port.write(`${line}\n`, (e) => (e ? rej(e) : res())));
  await new Promise((res, rej) => port.drain((e) => (e ? rej(e) : res())));
}

async function presentCmd(port, reader, line, timeoutMs = 20000) {
  await writeLine(port, line);
  const until = Date.now() + timeoutMs;
  while (Date.now() < until) {
    const l = await reader.waitLine(() => true, 1000).catch(() => null);
    if (!l) continue;
    if (/PRESENT|PERF|GALAXIAN|ESP-ROM|LEVEL_5/.test(l)) console.log(l);
    if (l.startsWith("#ACK PRESENT") || l.startsWith("#NAK PRESENT")) return l;
    if (l.includes("ESP-ROM:")) throw new Error("reboot");
  }
  throw new Error("ack timeout");
}

async function main() {
  const port = new SerialPort({ path: PORT, baudRate: 115200, autoOpen: false });
  await new Promise((res, rej) => port.open((e) => (e ? rej(e) : res())));
  await new Promise((r) => port.set({ dtr: false, rts: false }, () => r()));
  const reader = createLineReader(port);
  const dump = (l) => {
    if (/ACK|NAK|GALAXIAN|DISK|PERF|PRESENT|LEVEL|VISIBLE|build=/.test(l)) console.log(l);
  };

  console.log("# wait boot");
  const readyUntil = Date.now() + 120000;
  let ready = false;
  while (Date.now() < readyUntil && !ready) {
    try {
      const l = await reader.waitLine(() => true, 500);
      dump(l);
      if (l.includes("LEVEL_5=PARTIAL") || l.includes("apple2_present_v3")) ready = l.includes("LEVEL_5=PARTIAL");
    } catch {
      /* */
    }
  }
  await new Promise((r) => setTimeout(r, 1000));

  await writeLine(port, `${DevCommand.DiskMount} ${DISK}`);
  dump(await reader.waitLine((l) => l.startsWith("#ACK DISK MOUNT") || l.startsWith("#NAK"), 90000));
  await writeLine(port, DevCommand.DiskBoot);
  {
    const t = Date.now() + 90000;
    while (Date.now() < t) {
      const l = await reader.waitLine(() => true, 2000);
      dump(l);
      if (l.startsWith("#ACK DISK BOOT") || l.startsWith("#NAK BOOT")) break;
    }
  }

  const crackUntil = Date.now() + 90000;
  while (Date.now() < crackUntil) {
    try {
      const l = await reader.waitLine(() => true, 1000);
      dump(l);
      if (l.includes("video=HGR") && (l.includes("page2=1") || l.includes("pc=$B"))) break;
    } catch {
      /* */
    }
  }

  await writeLine(port, DevCommand.InputLive);
  try {
    dump(await reader.waitLine((l) => l.startsWith("#ACK INPUT"), 4000));
  } catch {
    /* */
  }
  await writeLine(port, `${DevCommand.InputKey} 41 1`);
  await new Promise((r) => setTimeout(r, 100));
  await writeLine(port, `${DevCommand.InputKey} 41 0`);

  let visible = false;
  const visUntil = Date.now() + 180000;
  while (Date.now() < visUntil && !visible) {
    try {
      const l = await reader.waitLine(() => true, 1000);
      dump(l);
      if (l.includes("ESP32_VISIBLE")) visible = true;
    } catch {
      /* */
    }
  }
  console.log(visible ? "GALAXIAN_REGRESSION=PASS" : "GALAXIAN_REGRESSION=FAIL");
  if (!visible) throw new Error("no playfield");

  dump(await presentCmd(port, reader, "#ESP2PRESENT ORIENT LANDSCAPE"));
  dump(await presentCmd(port, reader, "#ESP2PRESENT COLOR ARTIFACT"));
  dump(await presentCmd(port, reader, "#ESP2PRESENT STATUS"));

  let cps = null;
  const holdUntil = Date.now() + 20000;
  while (Date.now() < holdUntil) {
    try {
      const l = await reader.waitLine(() => true, 1000);
      dump(l);
      const m = l.match(/cps=([0-9.]+)/);
      if (m && l.includes("orient=LANDSCAPE") && l.includes("color=ARTIFACT")) cps = m[1];
    } catch {
      /* */
    }
  }
  console.log("LANDSCAPE_ARTIFACT_CPS=" + (cps || "NOT_SEEN"));
  console.log("# leave LANDSCAPE+ARTIFACT for visual SCORE check");
  reader.dispose();
  await new Promise((r) => port.close(() => r()));
}

main().catch((e) => {
  console.error("FAIL", e.message || e);
  process.exit(1);
});
