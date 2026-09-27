#!/usr/bin/env node
/** Run a named ESP][ input macro over CDC. */
import { SerialPort } from "serialport";
import { createLineReader } from "./esp2-serial-framing.mjs";

const portPath = (() => {
  const i = process.argv.indexOf("--port");
  return i >= 0 ? process.argv[i + 1] : process.env.ESP2_PORT || "COM5";
})();
const macro = (() => {
  const i = process.argv.indexOf("--macro");
  return i >= 0 ? process.argv[i + 1] : "galaxian-start";
})();

async function main() {
  const port = new SerialPort({ path: portPath, baudRate: 115200, autoOpen: false });
  await new Promise((res, rej) => port.open((e) => (e ? rej(e) : res())));
  await new Promise((r) => port.set({ dtr: false, rts: false }, () => r()));
  const reader = createLineReader(port);
  const line = `#ESP2MACRO RUN ${macro}`;
  await new Promise((res, rej) => port.write(`${line}\n`, (e) => (e ? rej(e) : res())));
  await new Promise((res, rej) => port.drain((e) => (e ? rej(e) : res())));
  console.log(`> ${line}`);
  const until = Date.now() + 10000;
  let ok = false;
  while (Date.now() < until) {
    try {
      const l = await reader.waitLine(() => true, 1000);
      console.log(l);
      if (l.startsWith("#ACK MACRO RUN") || l.startsWith("#ACK MACRO DONE")) {
        ok = true;
        break;
      }
      if (l.startsWith("#NAK MACRO")) {
        break;
      }
    } catch {
      /* */
    }
  }
  reader.dispose();
  await new Promise((r) => port.close(() => r()));
  process.exit(ok ? 0 : 1);
}

main().catch((e) => {
  console.error(e);
  process.exit(1);
});
