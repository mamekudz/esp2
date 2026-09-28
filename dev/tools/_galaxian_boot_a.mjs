#!/usr/bin/env node
/** Physical Galaxian: wait setup → mount → boot → wait cracktro HGR → send A → monitor. */
import { SerialPort } from "serialport";
import { createLineReader, DevCommand } from "./esp2-serial-framing.mjs";

const DISK = "/esp2/disks/Galaxian.dsk";
const PORT = process.env.ESP2_PORT || process.argv[2] || "COM5";

async function writeLine(port, line) {
  await new Promise((res, rej) => port.write(`${line}\n`, (e) => (e ? rej(e) : res())));
  await new Promise((res, rej) => port.drain((e) => (e ? rej(e) : res())));
}

async function main() {
  const port = new SerialPort({ path: PORT, baudRate: 115200, autoOpen: false });
  await new Promise((res, rej) => port.open((e) => (e ? rej(e) : res())));
  await new Promise((r) => port.set({ dtr: false, rts: false }, () => r()));
  const reader = createLineReader(port);
  const dump = (l) => {
    if (/NAK|ACK|GALAXIAN|DISK|PERF|INPUT|HB|VISIBLE|ROM|LEVEL|POST_RESET|hgr_lit/.test(l)) {
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
  await new Promise((r) => setTimeout(r, 2000));

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

  console.log("# wait cracktro HGR");
  const crackUntil = Date.now() + 90000;
  let cracktro = false;
  while (Date.now() < crackUntil && !cracktro) {
    try {
      const l = await reader.waitLine(() => true, 1000);
      dump(l);
      if (
        l.includes("video=HGR") &&
        (l.includes("pc=$B7") || l.includes("pc=$B6") || l.includes("page2=1"))
      ) {
        cracktro = true;
      }
    } catch {
      /* */
    }
  }
  console.log(cracktro ? "# cracktro_likely=YES" : "# cracktro_likely=NO");

  console.log("# INPUT A");
  await writeLine(port, DevCommand.InputLive);
  try {
    dump(
      await reader.waitLine(
        (l) => l.startsWith("#ACK INPUT LIVE") || l.startsWith("#NAK"),
        5000,
      ),
    );
  } catch {
    /* */
  }
  await writeLine(port, `${DevCommand.InputKey} 41 1`);
  await new Promise((r) => setTimeout(r, 80));
  await writeLine(port, `${DevCommand.InputKey} 41 0`);

  const wake = setInterval(() => {
    writeLine(port, `${DevCommand.InputPad} 128 128 0 0 0`).catch(() => {});
  }, 4000);

  console.log("# MONITOR 240s (pad keep-alive) — leave running on VISIBLE");
  const monUntil = Date.now() + 240000;
  let visible = false;
  while (Date.now() < monUntil) {
    try {
      const l = await reader.waitLine(() => true, 1000);
      dump(l);
      if (l.includes("ESP32_VISIBLE")) {
        visible = true;
        console.log("# VISIBLE — continue 60s stability then leave port");
        const stab = Date.now() + 60000;
        while (Date.now() < stab) {
          try {
            dump(await reader.waitLine(() => true, 1000));
          } catch {
            /* */
          }
        }
        break;
      }
    } catch {
      /* */
    }
  }
  clearInterval(wake);
  console.log(visible ? "GALAXIAN_MARKER=YES" : "GALAXIAN_MARKER=NO");
  // Do NOT idle/reset — leave Apple II running for human visual confirm.
  reader.dispose();
  await new Promise((r) => port.close(() => r()));
}

main().catch((e) => {
  console.error("FAIL", e.message || e);
  process.exit(1);
});
