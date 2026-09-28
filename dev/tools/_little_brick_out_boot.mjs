/**
 * Mid-run Little Brick Out: PRESENT WHITE/CLEAN → MOUNT → BOOT → watch video=LORES.
 */
import { SerialPort } from "serialport";
import { createLineReader, DevCommand } from "./esp2-serial-framing.mjs";

const PORT = process.env.ESP2_PORT || process.argv[2] || "COM5";
const DISK = "/esp2/disks/LittleBrickOut.dsk";
const MONITOR_MS = Number(process.env.ESP2_MONITOR_MS || 120000);

async function writeLine(port, line) {
  await new Promise((resolve, reject) => {
    port.write(`${line}\n`, (err) => (err ? reject(err) : resolve()));
  });
  await new Promise((resolve, reject) => {
    port.drain((err) => (err ? reject(err) : resolve()));
  });
}

async function main() {
  const port = new SerialPort({ path: PORT, baudRate: 115200, autoOpen: false });
  await new Promise((res, rej) => port.open((e) => (e ? rej(e) : res())));
  await new Promise((r) => port.set({ dtr: false, rts: false }, () => r()));
  await new Promise((r) => setTimeout(r, 300));
  const reader = createLineReader(port);
  const dump = (l) => {
    if (
      /ACK|NAK|DISK|PERF|VIDEO|LORES|HGR|MIXED|MACRO|PRESENT|MONITOR|EFFECT|INPUT|PAD|BOOT|APPLE2|SPEAKER|DIRTY|cps|FAIL|SCHED/.test(
        l,
      )
    ) {
      console.log(l);
    }
  };

  // Drain briefly
  const drainUntil = Date.now() + 3000;
  while (Date.now() < drainUntil) {
    try {
      dump(await reader.waitLine(() => true, 200));
    } catch {
      /* */
    }
  }

  console.log("# PRESENT WHITE CLEAN LANDSCAPE");
  await writeLine(port, "#ESP2PRESENT MONITOR WHITE");
  dump(await reader.waitLine((l) => /PRESENT|ACK|NAK|monitor/i.test(l), 5000).catch((e) => String(e)));
  await writeLine(port, "#ESP2PRESENT EFFECT CLEAN");
  dump(await reader.waitLine((l) => /PRESENT|ACK|NAK|effect/i.test(l), 5000).catch((e) => String(e)));
  await writeLine(port, "#ESP2PRESENT ORIENT LANDSCAPE");
  dump(await reader.waitLine((l) => /PRESENT|ACK|NAK|orient/i.test(l), 5000).catch((e) => String(e)));
  await writeLine(port, "#ESP2PRESENT STATUS");
  dump(await reader.waitLine((l) => /PRESENT|monitor|effect/i.test(l), 5000).catch((e) => String(e)));

  console.log("# MOUNT");
  reader.clear();
  await writeLine(port, `${DevCommand.DiskMount} ${DISK}`);
  const mack = await reader.waitLine(
    (l) => l.startsWith("#ACK DISK MOUNT") || l.startsWith("#NAK"),
    90000,
  );
  dump(mack);
  if (!mack.startsWith("#ACK DISK MOUNT")) throw new Error(mack);

  console.log("# BOOT");
  await writeLine(port, DevCommand.DiskBoot);
  const back = await reader.waitLine(
    (l) => l.startsWith("#ACK DISK BOOT") || l.startsWith("#NAK"),
    90000,
  );
  dump(back);
  if (!back.startsWith("#ACK DISK BOOT")) throw new Error(back);

  console.log(`# MONITOR ${MONITOR_MS}ms — look for video=LORES`);
  const until = Date.now() + MONITOR_MS;
  let sawLores = false;
  let sawMixed = false;
  let lastPerf = "";
  while (Date.now() < until) {
    try {
      const l = await reader.waitLine(() => true, 1000);
      dump(l);
      if (/video=LORES/i.test(l)) {
        sawLores = true;
        if (/MIXED|\+M/i.test(l)) sawMixed = true;
        console.log(`# LORES_FLAG ${l}`);
      }
      if (l.includes("[PERF]")) lastPerf = l;
    } catch {
      /* */
    }
  }
  console.log(
    JSON.stringify({
      sawLores,
      sawMixed,
      lastPerf,
      leaveRunning: true,
    }),
  );
  reader.dispose();
  await new Promise((r) => port.close(() => r()));
  process.exit(sawLores ? 0 : 2);
}

main().catch((e) => {
  console.error(e);
  process.exit(1);
});
