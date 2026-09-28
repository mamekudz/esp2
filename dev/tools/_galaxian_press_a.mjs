#!/usr/bin/env node
import { SerialPort } from "serialport";
import { createLineReader, DevCommand } from "./esp2-serial-framing.mjs";

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
    if (/NAK|ACK|GALAXIAN|DISK|PERF|INPUT|HB|VISIBLE|hgr_lit|SNAP/.test(l)) console.log(l);
  };

  await writeLine(port, DevCommand.InputLive);
  try {
    dump(
      await reader.waitLine(
        (l) => l.startsWith("#ACK INPUT") || l.startsWith("#NAK"),
        3000,
      ),
    );
  } catch {
    /* */
  }

  console.log("# A x3");
  for (let i = 0; i < 3; ++i) {
    await writeLine(port, `${DevCommand.InputKey} 41 1`);
    await new Promise((r) => setTimeout(r, 120));
    await writeLine(port, `${DevCommand.InputKey} 41 0`);
    await new Promise((r) => setTimeout(r, 400));
  }

  const wake = setInterval(() => {
    writeLine(port, `${DevCommand.InputPad} 128 128 0 0 0`).catch(() => {});
  }, 3000);
  const snap = setInterval(() => {
    writeLine(port, "#ESP2DIAG SNAP").catch(() => {});
  }, 10000);

  const until = Date.now() + 180000;
  let play = false;
  while (Date.now() < until) {
    try {
      const l = await reader.waitLine(() => true, 1000);
      dump(l);
      if (
        /pc=\$[9A][0-9A-Fa-f]{3}/.test(l) &&
        l.includes("video=HGR") &&
        l.includes("page2=0")
      ) {
        play = true;
      }
    } catch {
      /* */
    }
  }
  clearInterval(wake);
  clearInterval(snap);
  console.log(play ? "PLAYFIELD_PC=YES" : "PLAYFIELD_PC=NO");
  await writeLine(port, DevCommand.InputIdle);
  reader.dispose();
  await new Promise((r) => port.close(() => r()));
}

main().catch((e) => {
  console.error("FAIL", e.message || e);
  process.exit(1);
});
