#!/usr/bin/env node
/**
 * ESP][ development serial media upload (binary framed protocol v1).
 *
 * Usage:
 *   node dev/tools/esp2-upload.mjs --port COM5 --file path --target /esp2/roms/system.rom
 *   node dev/tools/esp2-upload.mjs --port COM5 --verify /esp2/roms/system.rom
 *   node dev/tools/esp2-upload.mjs --port COM5 --enter-usb-storage
 *   node dev/tools/esp2-upload.mjs --port COM5 --leave-usb-storage
 *
 * Does not download media. Never commits uploaded bytes.
 */

import { createHash } from "node:crypto";
import { readFileSync, existsSync } from "node:fs";
import { resolve } from "node:path";
import { SerialPort } from "serialport";
import { createLineReader } from "./esp2-serial-framing.mjs";

export const PROTOCOL_VERSION = 1;
export const MAGIC = 0x55505345; // 'ESPU' LE
export const DEFAULT_CHUNK = 256; // safer default on HW CDC (was 512/2048)
export const MAX_FILE_BYTES = 2 * 1024 * 1024;

export const FrameType = {
  Begin: 1,
  Data: 2,
  End: 3,
  Abort: 4,
  Verify: 5,
};

/** @param {string} p */
export function sanitizeEsp2Path(p) {
  if (typeof p !== "string" || !p.startsWith("/")) {
    throw new Error("path must be absolute under /esp2/");
  }
  if (p.includes("..") || p.includes("\\") || /[\x00-\x1f\x7f]/.test(p)) {
    throw new Error("path rejected (traversal/control)");
  }
  let out = "";
  let prevSlash = false;
  for (const c of p) {
    if (c === "/") {
      if (prevSlash) continue;
      prevSlash = true;
    } else {
      prevSlash = false;
    }
    out += c;
  }
  if (!out.startsWith("/esp2/") || out.length <= 6 || out.endsWith("/")) {
    throw new Error("path must be a file under /esp2/");
  }
  if (out.length > 120) {
    throw new Error("path too long");
  }
  return out;
}

function u32(n) {
  const b = Buffer.alloc(4);
  b.writeUInt32LE(n >>> 0, 0);
  return b;
}

function u64(n) {
  const b = Buffer.alloc(8);
  const big = BigInt(n);
  b.writeUInt32LE(Number(big & 0xffffffffn), 0);
  b.writeUInt32LE(Number(big >> 32n), 4);
  return b;
}

function frameHeader(type) {
  return Buffer.concat([
    u32(MAGIC),
    Buffer.from([PROTOCOL_VERSION, type, 0, 0]),
  ]);
}

function parseArgs(argv) {
  const out = {
    port: process.env.ESP2_PORT || "",
    file: "",
    target: "",
    verify: "",
    chunk: DEFAULT_CHUNK,
    enterUsb: false,
    leaveUsb: false,
    reset: false, // mid-run preferred; --boot-window uses firmware 12s WAIT
    bootWindow: false,
    listenMs: 15000,
  };
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    const next = () => argv[++i];
    if (a === "--port") out.port = next();
    else if (a === "--file") out.file = next();
    else if (a === "--target") out.target = next();
    else if (a === "--verify") out.verify = next();
    else if (a === "--chunk") out.chunk = Number(next());
    else if (a === "--enter-usb-storage") out.enterUsb = true;
    else if (a === "--leave-usb-storage") out.leaveUsb = true;
    else if (a === "--no-reset") out.reset = false;
    else if (a === "--reset") out.reset = true;
    else if (a === "--boot-window") out.bootWindow = true;
    else if (a === "--listen-ms") out.listenMs = Number(next());
    else if (a === "--help" || a === "-h") out.help = true;
  }
  return out;
}

/**
 * @param {string} path
 * @param {{ holdForBoot?: boolean }} [opts]
 *   holdForBoot: leave DTR alone so open can reset into #ESP2UPLOAD WAIT window.
 */
async function openPort(path, opts = {}) {
  const port = new SerialPort({
    path,
    baudRate: 115200,
    autoOpen: false,
  });
  await new Promise((resolve, reject) => {
    port.open((err) => (err ? reject(err) : resolve()));
  });
  if (!opts.holdForBoot) {
    // Mid-run: avoid an accidental reboot that drops LIVE_INPUT / running emu.
    await new Promise((resolve) => {
      port.set({ dtr: false, rts: false }, () => resolve());
    });
    await new Promise((r) => setTimeout(r, 200));
  }
  return port;
}

function pulseReset(port) {
  // ESP32-S3 USB-Serial/JTAG: brief RTS asserts EN/reset without the
  // DTR+RTS download recipe. Prefer mid-run upload when the app is live.
  return new Promise((resolve) => {
    port.set({ dtr: false, rts: true }, () => {
      setTimeout(() => {
        port.set({ dtr: false, rts: false }, () => resolve());
      }, 100);
    });
  });
}

/**
 * @param {ReturnType<typeof createLineReader>} reader
 * @param {import('serialport').SerialPort} port
 * @param {number} listenMs
 * @param {string} enterLine  e.g. #ESP2UPLOAD
 */
async function waitReady(reader, port, listenMs, enterLine, readyNeedle) {
  const deadline = Date.now() + listenMs;
  let lastPing = 0;
  while (Date.now() < deadline) {
    try {
      const line = await reader.waitLine(
        (l) =>
          l.includes(readyNeedle) ||
          l.includes("#ESP2UPLOAD READY") ||
          l.includes("#ESP2UPLOAD ENTER"),
        Math.min(450, deadline - Date.now()),
      );
      if (line.includes("#ESP2UPLOAD ENTER") && !line.includes("READY")) {
        // Mid-run ack — keep waiting for READY.
        continue;
      }
      return line;
    } catch {
      if (Date.now() - lastPing > 400) {
        lastPing = Date.now();
        port.write(`${enterLine}\n`);
      }
    }
  }
  throw new Error(`timeout waiting for ${readyNeedle}`);
}

function writeAndDrain(port, buf) {
  return new Promise((resolve, reject) => {
    port.write(buf, (err) => {
      if (err) {
        reject(err);
        return;
      }
      port.drain((err2) => (err2 ? reject(err2) : resolve()));
    });
  });
}

/**
 * @param {import('serialport').SerialPort} port
 * @param {ReturnType<typeof createLineReader>} reader
 */
async function uploadFile(port, reader, filePath, targetPath, chunkSize) {
  const abs = resolve(filePath);
  if (!existsSync(abs)) {
    throw new Error(`file not found: ${abs}`);
  }
  const data = readFileSync(abs);
  if (data.length === 0 || data.length > MAX_FILE_BYTES) {
    throw new Error(`file size ${data.length} out of range`);
  }
  const target = sanitizeEsp2Path(targetPath);
  const sha = createHash("sha256").update(data).digest();
  const shaHex = sha.toString("hex");
  const t0 = Date.now();

  const begin = Buffer.concat([
    frameHeader(FrameType.Begin),
    u32(Buffer.byteLength(target, "utf8")),
    Buffer.from(target, "utf8"),
    u64(data.length),
    sha,
    u32(chunkSize),
  ]);
  // Host→device binary does not go through the line reader (that is device→host only).
  // Do not pause the reader across writes — ACKs can arrive immediately and would be lost.
  await writeAndDrain(port, begin);
  const beginAck = await reader.waitLine((l) => l.startsWith("#ACK BEGIN"), 20000);
  console.log(beginAck);
  await new Promise((r) => setTimeout(r, 600));

  let offset = 0;
  let seq = 0;
  while (offset < data.length) {
    const slice = data.subarray(offset, Math.min(offset + chunkSize, data.length));
    const frame = Buffer.concat([
      frameHeader(FrameType.Data),
      u32(seq),
      u32(slice.length),
      slice,
    ]);
    await writeAndDrain(port, frame);
    const ack = await reader.waitLine(
      (l) => l.startsWith("#ACK DATA") && l.includes(`seq=${seq}`),
      30000,
    );
    if (!ack.includes(`seq=${seq}`)) {
      throw new Error(`unexpected data ack: ${ack}`);
    }
    offset += slice.length;
    seq++;
    await new Promise((r) => setTimeout(r, 15));
  }

  await writeAndDrain(port, frameHeader(FrameType.End));
  const endAck = await reader.waitLine((l) => l.startsWith("#ACK END"), 60000);
  console.log(endAck);
  // Wait for DONE so the device leaves MEDIA mode before next host command.
  try {
    await reader.waitLine((l) => l.startsWith("#ESP2UPLOAD DONE"), 10000);
  } catch {
    /* older firmware may omit DONE after END */
  }
  const ms = Date.now() - t0;
  const bps = ms > 0 ? (data.length * 1000) / ms : 0;
  return {
    path: target,
    size: data.length,
    sha256: shaHex,
    ms,
    bytesPerSec: bps,
    endAck,
  };
}

/**
 * @param {import('serialport').SerialPort} port
 * @param {ReturnType<typeof createLineReader>} reader
 */
async function verifyRemote(port, reader, targetPath) {
  const target = sanitizeEsp2Path(targetPath);
  const frame = Buffer.concat([
    frameHeader(FrameType.Verify),
    u32(Buffer.byteLength(target, "utf8")),
    Buffer.from(target, "utf8"),
  ]);
  await writeAndDrain(port, frame);
  const ack = await reader.waitLine((l) => l.startsWith("#ACK VERIFY"), 30000);
  console.log(ack);
  return ack;
}

async function main() {
  const args = parseArgs(process.argv.slice(2));
  if (args.help) {
    console.log(`Usage:
  node dev/tools/esp2-upload.mjs --port COM5 --file <local> --target /esp2/...
  node dev/tools/esp2-upload.mjs --port COM5 --verify /esp2/...
  node dev/tools/esp2-upload.mjs --port COM5 --enter-usb-storage
  node dev/tools/esp2-upload.mjs --port COM5 --leave-usb-storage`);
    process.exit(0);
  }
  if (!args.port) {
    console.error("ERROR: --port required (refusing auto-pick of arbitrary COM)");
    process.exit(2);
  }

  const useBootWindow = args.bootWindow || args.reset;
  const port = await openPort(args.port, { holdForBoot: useBootWindow });
  const reader = createLineReader(port);
  try {
    if (args.reset) {
      await pulseReset(port);
      await new Promise((r) => setTimeout(r, 600));
    }

    if (args.enterUsb) {
      await waitReady(reader, port, args.listenMs, "#ESP2USBMSC", "#ESP2USBMSC READY");
      console.log("USB storage mode requested");
      return;
    }
    if (args.leaveUsb) {
      await waitReady(reader, port, args.listenMs, "#ESP2USBMSC LEAVE", "#ESP2USBMSC LEFT");
      console.log("USB storage leave requested");
      return;
    }

    // Boot-window: firmware prints #ESP2UPLOAD WAIT for ~12s before display init.
    const listenMs = useBootWindow ? Math.max(args.listenMs, 20000) : args.listenMs;
    await waitReady(reader, port, listenMs, "#ESP2UPLOAD", "#ESP2UPLOAD READY");
    // Drop any WAIT/diagnostic lines queued before binary MEDIA phase.
    reader.clear();
    await new Promise((r) => setTimeout(r, 100));

    if (args.verify) {
      await verifyRemote(port, reader, args.verify);
      return;
    }
    if (!args.file || !args.target) {
      throw new Error("--file and --target required for upload");
    }
    const chunk = Number.isFinite(args.chunk) && args.chunk > 0 ? args.chunk : DEFAULT_CHUNK;
    const result = await uploadFile(port, reader, args.file, args.target, chunk);
    console.log(
      JSON.stringify(
        {
          ok: true,
          ...result,
        },
        null,
        2,
      ),
    );
  } finally {
    reader.dispose();
    await new Promise((r) => port.close(() => r()));
  }
}

const isMain = process.argv[1] && resolve(process.argv[1]).endsWith("esp2-upload.mjs");
if (isMain) {
  main().catch((err) => {
    console.error("FAIL", err.message || err);
    process.exit(1);
  });
}
