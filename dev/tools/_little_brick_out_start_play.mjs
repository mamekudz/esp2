/**
 * From title/name prompts: CDC LIVE → SPACE + PB0 → wait LORES+MIXED → leave port free for dial bridge.
 */
import { SerialPort } from "serialport";
import { createLineReader, DevCommand } from "./esp2-serial-framing.mjs";

const PORT = process.env.ESP2_PORT || process.argv[2] || "COM5";
const WAIT_MS = Number(process.env.ESP2_WAIT_MS || 90000);

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
  await new Promise((r) => setTimeout(r, 200));
  const reader = createLineReader(port);
  const dump = (l) => {
    if (/ACK|NAK|DISK|PERF|LORES|HGR|MIXED|INPUT|PAD|KEY|MACRO|PRESENT|cps/.test(l)) {
      console.log(l);
    }
  };

  console.log("# INPUT LIVE");
  await writeLine(port, DevCommand.InputLive);
  dump(await reader.waitLine((l) => /INPUT/.test(l), 5000));

  // Pulse SPACE (0x20) and hold PB0 briefly — same generic paths as Windows bridge.
  for (let i = 0; i < 8 && Date.now(); ++i) {
    await writeLine(port, `${DevCommand.InputKey} 20 1`);
    await writeLine(port, `${DevCommand.InputPad} 128 128 1 0 0`);
    console.log(`# pulse ${i} SPACE+PB0`);
    await new Promise((r) => setTimeout(r, 400));
    await writeLine(port, `${DevCommand.InputPad} 128 128 0 0 0`);
    await new Promise((r) => setTimeout(r, 600));

    // Drain logs looking for LORES
    const sliceEnd = Date.now() + 2000;
    while (Date.now() < sliceEnd) {
      try {
        const l = await reader.waitLine(() => true, 300);
        dump(l);
        if (/video=LORES/i.test(l)) {
          console.log(`# LORES_FLAG ${l}`);
          await writeLine(port, DevCommand.InputIdle);
          dump(await reader.waitLine((x) => /INPUT/.test(x), 3000).catch(() => "idle?"));
          console.log(JSON.stringify({ sawLores: true, readyForDial: true }));
          reader.dispose();
          await new Promise((r) => port.close(() => r()));
          process.exit(0);
        }
      } catch {
        /* */
      }
    }
  }

  // Longer watch after pulses
  const until = Date.now() + WAIT_MS;
  let sawLores = false;
  while (Date.now() < until) {
    try {
      const l = await reader.waitLine(() => true, 1000);
      dump(l);
      if (/video=LORES/i.test(l)) {
        sawLores = true;
        console.log(`# LORES_FLAG ${l}`);
        break;
      }
      // Keep gently offering PB0 every few seconds (name skip / new game)
      if (Math.floor(Date.now() / 3000) % 2 === 0) {
        await writeLine(port, `${DevCommand.InputPad} 140 128 1 0 0`);
        await new Promise((r) => setTimeout(r, 200));
        await writeLine(port, `${DevCommand.InputPad} 140 128 0 0 0`);
      }
    } catch {
      /* */
    }
  }

  await writeLine(port, DevCommand.InputIdle);
  console.log(JSON.stringify({ sawLores, readyForDial: sawLores }));
  reader.dispose();
  await new Promise((r) => port.close(() => r()));
  process.exit(sawLores ? 0 : 2);
}

main().catch((e) => {
  console.error(e);
  process.exit(1);
});
