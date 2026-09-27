#!/usr/bin/env node
/**
 * Physical Galaxian presentation matrix:
 *   CLASSIC+SHARP, CLASSIC+ARTIFACT, LANDSCAPE+SHARP, LANDSCAPE+ARTIFACT
 * Leaves LANDSCAPE+ARTIFACT running for human visual confirmation.
 */
import { SerialPort } from "serialport";
import { createLineReader, DevCommand } from "./esp2-serial-framing.mjs";

const DISK = "/esp2/disks/Galaxian.dsk";
const PORT = process.env.ESP2_PORT || process.argv[2] || "COM5";
const HOLD_MS = Number(process.env.ESP2_PRESENT_HOLD_MS || 22000);

async function writeLine(port, line) {
  await new Promise((res, rej) => port.write(`${line}\n`, (e) => (e ? rej(e) : res())));
  await new Promise((res, rej) => port.drain((e) => (e ? rej(e) : res())));
}

function isPresentAck(l) {
  return l.startsWith("#ACK PRESENT") || l.startsWith("#NAK PRESENT");
}

async function drainQuiet(reader, ms) {
  const until = Date.now() + ms;
  while (Date.now() < until) {
    try {
      await reader.waitLine(() => true, 200);
    } catch {
      /* */
    }
  }
}

async function presentCmd(port, reader, line, timeoutMs = 20000) {
  await writeLine(port, line);
  const until = Date.now() + timeoutMs;
  while (Date.now() < until) {
    try {
      const l = await reader.waitLine(() => true, 1000);
      if (isPresentAck(l)) {
        return l;
      }
      if (/PRESENT|PERF|GALAXIAN|SPI|panic|Guru|ESP-ROM/.test(l)) {
        console.log(l);
      }
      if (l.includes("ESP-ROM:") || l.includes("LEVEL_5=PARTIAL")) {
        throw new Error("device_reboot_during_present");
      }
    } catch (e) {
      if (e && e.message === "device_reboot_during_present") {
        throw e;
      }
    }
  }
  throw new Error("ack timeout");
}

async function main() {
  const port = new SerialPort({ path: PORT, baudRate: 115200, autoOpen: false });
  await new Promise((res, rej) => port.open((e) => (e ? rej(e) : res())));
  await new Promise((r) => port.set({ dtr: false, rts: false }, () => r()));
  const reader = createLineReader(port);
  const dump = (l) => {
    if (/NAK|ACK|GALAXIAN|DISK|PERF|PRESENT|INPUT|HB|VISIBLE|LEVEL|ROM|POST_RESET|SPI|ESP-ROM/.test(l)) {
      console.log(l);
    }
  };

  console.log("# wait LEVEL_5");
  const readyUntil = Date.now() + 120000;
  let ready = false;
  while (Date.now() < readyUntil && !ready) {
    try {
      const l = await reader.waitLine(() => true, 500);
      dump(l);
      if (l.includes("LEVEL_5=PARTIAL")) ready = true;
    } catch {
      /* */
    }
  }
  await new Promise((r) => setTimeout(r, 1500));

  console.log("# MOUNT/BOOT");
  await writeLine(port, `${DevCommand.DiskMount} ${DISK}`);
  dump(
    await reader.waitLine(
      (l) => l.startsWith("#ACK DISK MOUNT") || l.startsWith("#NAK"),
      90000,
    ),
  );
  await writeLine(port, DevCommand.DiskBoot);
  {
    const t = Date.now() + 90000;
    let done = false;
    while (Date.now() < t && !done) {
      const l = await reader.waitLine(() => true, 2000);
      dump(l);
      if (l.startsWith("#ACK DISK BOOT") || l.startsWith("#NAK BOOT")) done = true;
    }
  }

  console.log("# wait cracktro / VISIBLE");
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
    dump(await reader.waitLine((l) => l.startsWith("#ACK INPUT") || l.startsWith("#NAK"), 4000));
  } catch {
    /* */
  }
  await writeLine(port, `${DevCommand.InputKey} 41 1`);
  await new Promise((r) => setTimeout(r, 100));
  await writeLine(port, `${DevCommand.InputKey} 41 0`);

  console.log("# wait playfield VISIBLE");
  const visUntil = Date.now() + 180000;
  let visible = false;
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
  if (!visible) {
    throw new Error("galaxian playfield not visible");
  }

  // Let HGR settle; avoid flooding PAD while waiting for PRESENT ACKs.
  await drainQuiet(reader, 1500);

  const modes = [
    ["CLASSIC", "SHARP"],
    ["CLASSIC", "ARTIFACT"],
    ["LANDSCAPE", "SHARP"],
    ["LANDSCAPE", "ARTIFACT"],
  ];
  const results = {};
  for (const [orient, color] of modes) {
    const key = `${orient}_${color}`;
    console.log(`# MODE ${orient} ${color}`);
    try {
      dump(await presentCmd(port, reader, `#ESP2PRESENT ORIENT ${orient}`));
      dump(await presentCmd(port, reader, `#ESP2PRESENT COLOR ${color}`));
      dump(await presentCmd(port, reader, "#ESP2PRESENT STATUS", 8000));
      let perf = null;
      const holdUntil = Date.now() + HOLD_MS;
      while (Date.now() < holdUntil) {
        try {
          const l = await reader.waitLine(() => true, 1000);
          dump(l);
          if (l.includes("[PERF]") && l.includes(`color=${color}`) && l.includes(`orient=${orient}`)) {
            perf = l;
          }
          if (l.includes("ESP-ROM:")) {
            throw new Error("device_reboot_during_hold");
          }
        } catch (e) {
          if (e && String(e.message).includes("device_reboot")) {
            throw e;
          }
        }
      }
      results[key] = { status: "WORKS_CMD", perf };
      console.log(`MODE_RESULT ${key}=WORKS_CMD`);
    } catch (e) {
      results[key] = { status: "FAIL", err: e.message };
      console.log(`MODE_RESULT ${key}=FAIL ${e.message}`);
      throw e;
    }
  }

  console.log("# leave LANDSCAPE+ARTIFACT running");
  console.log("MATRIX_SUMMARY", JSON.stringify(results));
  reader.dispose();
  await new Promise((r) => port.close(() => r()));
}

main().catch((e) => {
  console.error("FAIL", e.message || e);
  process.exit(1);
});
