#!/usr/bin/env node
/**
 * Physical Galaxian presentation matrix (monitor × effect × orientation).
 * Leaves LANDSCAPE + ARTIFACT + CRT running for human visual confirmation.
 */
import { SerialPort } from "serialport";
import { createLineReader, DevCommand } from "./esp2-serial-framing.mjs";

const DISK = "/esp2/disks/Galaxian.dsk";
const PORT = process.env.ESP2_PORT || process.argv[2] || "COM5";
const HOLD_MS = Number(process.env.ESP2_PRESENT_HOLD_MS || 10000);

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

async function holdMode(port, reader, orient, monitor, effect) {
  console.log(`# MODE ${orient} ${monitor} ${effect}`);
  const ackO = await presentCmd(port, reader, `#ESP2PRESENT ORIENT ${orient}`);
  console.log(ackO);
  const ackM = await presentCmd(port, reader, `#ESP2PRESENT MONITOR ${monitor}`);
  console.log(ackM);
  const ackE = await presentCmd(port, reader, `#ESP2PRESENT EFFECT ${effect}`);
  console.log(ackE);
  console.log(await presentCmd(port, reader, "#ESP2PRESENT STATUS", 8000));
  let perf = null;
  const holdUntil = Date.now() + HOLD_MS;
  while (Date.now() < holdUntil) {
    try {
      const l = await reader.waitLine(() => true, 1000);
      if (/PRESENT|PERF|GALAXIAN|SPI|panic|Guru|ESP-ROM/.test(l)) {
        console.log(l);
      }
      if (
        l.includes("[PERF]") &&
        l.includes(`monitor=${monitor}`) &&
        l.includes(`effect=${effect}`) &&
        l.includes(`orient=${orient}`)
      ) {
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
  const key = `${orient}_${monitor}_${effect}`;
  console.log(`MODE_RESULT ${key}=WORKS_CMD`);
  return { key, status: "WORKS_CMD", perf };
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

  console.log("# wait ready (LEVEL_5 or already-running Galaxian HGR)");
  const readyUntil = Date.now() + 45000;
  let ready = false;
  let alreadyPlayfield = false;
  while (Date.now() < readyUntil && !ready) {
    try {
      const l = await reader.waitLine(() => true, 500);
      dump(l);
      if (l.includes("LEVEL_5=PARTIAL")) ready = true;
      if (l.includes("video=HGR") && l.includes("Galaxian")) alreadyPlayfield = true;
      if (l.includes("ESP32_VISIBLE")) alreadyPlayfield = true;
      const m = l.match(/hgr_lit=(\d+)/);
      if (m && Number(m[1]) >= 80) alreadyPlayfield = true;
      if (alreadyPlayfield) ready = true;
    } catch {
      /* */
    }
  }
  await new Promise((r) => setTimeout(r, 500));

  if (!alreadyPlayfield) {
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
  } else {
    console.log("# already on Galaxian playfield — skip remount");
  }

  console.log("# confirm playfield (HGR + hgr_lit)");
  const visUntil = Date.now() + 60000;
  let visible = alreadyPlayfield;
  let inHgr = alreadyPlayfield;
  let hgrStable = alreadyPlayfield ? 2 : 0;
  while (Date.now() < visUntil && !visible) {
    try {
      const l = await reader.waitLine(() => true, 1000);
      dump(l);
      if (l.includes("ESP32_VISIBLE")) visible = true;
      if (l.includes("video=HGR")) inHgr = true;
      if (l.includes("video=TEXT")) inHgr = false;
      const m = l.match(/hgr_lit=(\d+)/);
      if (inHgr && m) {
        const lit = Number(m[1]);
        if (lit >= 80) {
          hgrStable += 1;
          if (hgrStable >= 2) visible = true;
        }
      }
    } catch {
      /* */
    }
  }
  console.log(visible ? "GALAXIAN_REGRESSION=PASS" : "GALAXIAN_REGRESSION=FAIL");
  if (!visible) {
    throw new Error("galaxian playfield not visible");
  }

  await drainQuiet(reader, 1500);

  const monitors = ["WHITE", "GREEN", "AMBER", "ARTIFACT"];
  const results = {};

  for (const orient of ["CLASSIC", "LANDSCAPE"]) {
    for (const monitor of monitors) {
      const r = await holdMode(port, reader, orient, monitor, "CLEAN");
      results[r.key] = r;
    }
  }
  for (const monitor of monitors) {
    const r = await holdMode(port, reader, "LANDSCAPE", monitor, "CRT");
    results[r.key] = r;
  }

  // TEXT phosphor spot-check: Apple II reset → ROM text, GREEN then AMBER.
  console.log("# TEXT phosphor spot-check");
  await writeLine(port, "#ESP2APPLE RESET");
  await drainQuiet(reader, 2500);
  dump(await presentCmd(port, reader, "#ESP2PRESENT ORIENT CLASSIC"));
  dump(await presentCmd(port, reader, "#ESP2PRESENT EFFECT CLEAN"));
  dump(await presentCmd(port, reader, "#ESP2PRESENT MONITOR GREEN"));
  await drainQuiet(reader, 4000);
  dump(await presentCmd(port, reader, "#ESP2PRESENT MONITOR AMBER"));
  await drainQuiet(reader, 4000);
  console.log("TEXT_GREEN_AMBER=SPOT_OK");

  console.log("# leave LANDSCAPE + ARTIFACT + CRT for human inspection");
  dump(await presentCmd(port, reader, "#ESP2PRESENT ORIENT LANDSCAPE"));
  dump(await presentCmd(port, reader, "#ESP2PRESENT MONITOR ARTIFACT"));
  dump(await presentCmd(port, reader, "#ESP2PRESENT EFFECT CRT"));
  // Remount Galaxian so playfield is visible for inspection.
  await writeLine(port, `${DevCommand.DiskMount} ${DISK}`);
  try {
    dump(await reader.waitLine((l) => l.startsWith("#ACK") || l.startsWith("#NAK"), 30000));
  } catch {
    /* */
  }
  await writeLine(port, DevCommand.DiskBoot);
  await drainQuiet(reader, 8000);
  await writeLine(port, DevCommand.InputLive);
  await writeLine(port, `${DevCommand.InputKey} 41 1`);
  await new Promise((r) => setTimeout(r, 80));
  await writeLine(port, `${DevCommand.InputKey} 41 0`);

  console.log("MATRIX_SUMMARY", JSON.stringify(results));
  console.log("LEAVE_MODE=LANDSCAPE_ARTIFACT_CRT");
  reader.dispose();
  await new Promise((r) => port.close(() => r()));
}

main().catch((e) => {
  console.error("FAIL", e.message || e);
  process.exit(1);
});
