#!/usr/bin/env node
/**
 * One-shot physical Galaxian chain: mid-run VERIFY → MOUNT → BOOT → monitor.
 * Temporary helper for the Galaxian ESP32 milestone — not a product CLI.
 */
import { SerialPort } from "serialport";
import { createLineReader, DevCommand } from "./esp2-serial-framing.mjs";
import {
  FrameType,
  MAGIC,
  PROTOCOL_VERSION,
  sanitizeEsp2Path,
} from "./esp2-upload.mjs";

const EXPECT =
  "b1e85b78b689a64c5787893c4954d309a8d20e4d1d4a551dc4d9cb5dc132c2d1";
const DISK = "/esp2/disks/Galaxian.dsk";
const PORT = process.env.ESP2_PORT || process.argv[2] || "COM5";
const BOOT_DRAIN_MS = Number(process.env.ESP2_BOOT_DRAIN_MS || 90000);
const MONITOR_MS = Number(process.env.ESP2_MONITOR_MS || 180000);
const READY_FOR_DISK =
  process.env.ESP2_READY_NEEDLE ||
  "LEVEL_5=PARTIAL/READY_FOR_REAL_SOFTWARE_TEST";

function u32(n) {
  const b = Buffer.alloc(4);
  b.writeUInt32LE(n >>> 0, 0);
  return b;
}

function frameHeader(type) {
  return Buffer.concat([
    u32(MAGIC),
    Buffer.from([PROTOCOL_VERSION, type, 0, 0]),
  ]);
}

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
    if (
      /NAK|ACK|GALAXIAN|DISK|ROM|SPI|panic|Guru|SELFTEST|INTERACTIVE|UPLOAD|INPUT|BOOT|qt |cache|WRONG|Stage|PERF|APPLE2|SPEAKER|LEVEL|SCHED|VIDEO|MOTOR|head|HGR/.test(
        l,
      )
    ) {
      console.log(l);
    }
  };

  console.log(`# wait setup complete (needle=${READY_FOR_DISK}, max ${BOOT_DRAIN_MS}ms)`);
  const drainUntil = Date.now() + BOOT_DRAIN_MS;
  let setupReady = false;
  while (Date.now() < drainUntil && !setupReady) {
    try {
      const l = await reader.waitLine(() => true, 500);
      dump(l);
      if (l.includes(READY_FOR_DISK) || l.includes("[SCHED] emu")) {
        setupReady = true;
      }
    } catch {
      /* ignore */
    }
  }
  if (!setupReady) {
    console.log("# WARN setup needle not seen — proceeding cautiously");
  } else {
    // Allow scheduler tasks to start.
    await new Promise((r) => setTimeout(r, 1500));
  }

  console.log("# mid-run VERIFY");
  reader.clear();
  let ready = false;
  const readyUntil = Date.now() + 25000;
  while (Date.now() < readyUntil && !ready) {
    await writeLine(port, DevCommand.UploadEnter);
    try {
      const line = await reader.waitLine(
        (l) =>
          l.includes("#ESP2UPLOAD READY") ||
          l.includes("#ESP2UPLOAD ENTER") ||
          l.startsWith("#NAK"),
        1200,
      );
      dump(line);
      if (line.startsWith("#NAK")) {
        throw new Error(line);
      }
      if (line.includes("READY")) {
        ready = true;
        break;
      }
      // ENTER seen — keep waiting for READY without re-ping flooding
      try {
        const r2 = await reader.waitLine(
          (l) => l.includes("#ESP2UPLOAD READY") || l.startsWith("#NAK"),
          8000,
        );
        dump(r2);
        if (r2.startsWith("#NAK")) {
          throw new Error(r2);
        }
        ready = true;
      } catch (e) {
        if (String(e.message || e).startsWith("#NAK")) {
          throw e;
        }
      }
    } catch (e) {
      if (String(e.message || e).startsWith("#NAK")) {
        throw e;
      }
    }
  }
  if (!ready) {
    throw new Error("timeout waiting for #ESP2UPLOAD READY");
  }

  reader.clear();
  await new Promise((r) => setTimeout(r, 100));
  const target = sanitizeEsp2Path(DISK);
  const frame = Buffer.concat([
    frameHeader(FrameType.Verify),
    u32(Buffer.byteLength(target, "utf8")),
    Buffer.from(target, "utf8"),
  ]);
  await new Promise((res, rej) => port.write(frame, (e) => (e ? rej(e) : res())));
  const vack = await reader.waitLine(
    (l) => l.startsWith("#ACK VERIFY") || l.startsWith("#NAK"),
    45000,
  );
  dump(vack);
  if (!vack.startsWith("#ACK VERIFY") || !vack.includes(EXPECT)) {
    throw new Error(`verify mismatch: ${vack}`);
  }
  console.log(`VERIFY_OK sha256=${EXPECT}`);
  try {
    dump(await reader.waitLine((l) => l.includes("DONE"), 5000));
  } catch {
    /* older path */
  }

  console.log("# MOUNT");
  reader.clear();
  await writeLine(port, `${DevCommand.DiskMount} ${DISK}`);
  const mack = await reader.waitLine(
    (l) => l.startsWith("#ACK DISK MOUNT") || l.startsWith("#NAK"),
    90000,
  );
  dump(mack);
  if (!mack.startsWith("#ACK DISK MOUNT")) {
    throw new Error(mack);
  }

  console.log("# BOOT");
  await writeLine(port, DevCommand.DiskBoot);
  const back = await reader.waitLine(
    (l) => l.startsWith("#ACK DISK BOOT") || l.startsWith("#NAK"),
    90000,
  );
  dump(back);
  if (!back.startsWith("#ACK DISK BOOT")) {
    throw new Error(back);
  }

  console.log(`# MONITOR ${MONITOR_MS}ms`);
  const monUntil = Date.now() + MONITOR_MS;
  let visible = false;
  while (Date.now() < monUntil) {
    try {
      const l = await reader.waitLine(() => true, 1000);
      dump(l);
      if (l.includes("ESP32_VISIBLE")) {
        visible = true;
        // keep listening a bit for stability metrics
      }
    } catch {
      /* */
    }
  }
  console.log(visible ? "GALAXIAN_MARKER=YES" : "GALAXIAN_MARKER=NO");
  reader.dispose();
  await new Promise((r) => port.close(() => r()));
}

main().catch((err) => {
  console.error("FAIL", err.message || err);
  process.exit(1);
});
