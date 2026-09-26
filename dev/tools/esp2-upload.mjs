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
import { readFileSync, existsSync, statSync } from "node:fs";
import { resolve } from "node:path";
import { SerialPort } from "serialport";

export const PROTOCOL_VERSION = 1;
export const MAGIC = 0x55505345; // 'ESPU' LE
export const DEFAULT_CHUNK = 2048;
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
    reset: false, // mid-run upload preferred; use --reset for boot-window only
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
    else if (a === "--listen-ms") out.listenMs = Number(next());
    else if (a === "--help" || a === "-h") out.help = true;
  }
  return out;
}

async function openPort(path) {
  const port = new SerialPort({
    path,
    baudRate: 115200,
    autoOpen: false,
  });
  await new Promise((resolve, reject) => {
    port.open((err) => (err ? reject(err) : resolve()));
  });
  return port;
}

function pulseReset(port) {
  // ESP32-S3 USB-Serial/JTAG: classic RTS-high sequences enter DOWNLOAD mode.
  // For app restart use a brief DTR toggle only (best-effort); prefer mid-run upload.
  return new Promise((resolve) => {
    port.set({ dtr: true, rts: false }, () => {
      setTimeout(() => {
        port.set({ dtr: false, rts: false }, () => resolve());
      }, 80);
    });
  });
}

/**
 * @param {import('serialport').SerialPort} port
 * @param {number} listenMs
 * @param {string} enterLine  e.g. #ESP2UPLOAD
 */
async function waitReady(port, listenMs, enterLine, readyNeedle) {
  let buf = "";
  const deadline = Date.now() + listenMs;
  let lastPing = 0;

  const onData = (d) => {
    buf += d.toString("utf8");
  };
  port.on("data", onData);

  try {
    while (Date.now() < deadline) {
      if (buf.includes(readyNeedle) || buf.includes("#ESP2UPLOAD READY")) {
        return buf;
      }
      if (Date.now() - lastPing > 400) {
        lastPing = Date.now();
        port.write(`${enterLine}\n`);
      }
      await new Promise((r) => setTimeout(r, 50));
    }
    throw new Error(`timeout waiting for ${readyNeedle}`);
  } finally {
    port.off("data", onData);
  }
}

async function readUntilAck(port, timeoutMs, prefix = "#ACK") {
  let buf = "";
  const deadline = Date.now() + timeoutMs;
  return await new Promise((resolve, reject) => {
    const onData = (d) => {
      buf += d.toString("utf8");
      const lines = buf.split(/\r?\n/);
      for (const line of lines) {
        if (line.startsWith("#NAK")) {
          cleanup();
          reject(new Error(line));
          return;
        }
        if (line.startsWith(prefix) || line.startsWith("#ESP2UPLOAD DONE")) {
          cleanup();
          resolve(line);
          return;
        }
      }
      if (Date.now() > deadline) {
        cleanup();
        reject(new Error("ack timeout"));
      }
    };
    const timer = setInterval(() => {
      if (Date.now() > deadline) {
        cleanup();
        reject(new Error("ack timeout"));
      }
    }, 200);
    const cleanup = () => {
      clearInterval(timer);
      port.off("data", onData);
    };
    port.on("data", onData);
  });
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

async function uploadFile(port, filePath, targetPath, chunkSize) {
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
  await writeAndDrain(port, begin);
  const beginAck = await readUntilAck(port, 15000, "#ACK BEGIN");
  console.log(beginAck);
  // Give FAT/SD staging time before binary DATA frames (CDC drop risk).
  await new Promise((r) => setTimeout(r, 400));

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
    const ack = await readUntilAck(port, 30000, "#ACK DATA");
    if (!ack.includes(`seq=${seq}`)) {
      throw new Error(`unexpected data ack: ${ack}`);
    }
    offset += slice.length;
    seq++;
    await new Promise((r) => setTimeout(r, 20));
  }

  await writeAndDrain(port, frameHeader(FrameType.End));
  const endAck = await readUntilAck(port, 30000, "#ACK END");
  console.log(endAck);
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

async function verifyRemote(port, targetPath) {
  const target = sanitizeEsp2Path(targetPath);
  const frame = Buffer.concat([
    frameHeader(FrameType.Verify),
    u32(Buffer.byteLength(target, "utf8")),
    Buffer.from(target, "utf8"),
  ]);
  port.write(frame);
  const ack = await readUntilAck(port, 20000, "#ACK VERIFY");
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

  const port = await openPort(args.port);
  try {
    if (args.reset) {
      await pulseReset(port);
      await new Promise((r) => setTimeout(r, 400));
    }

    if (args.enterUsb) {
      await waitReady(port, args.listenMs, "#ESP2USBMSC", "#ESP2USBMSC READY");
      console.log("USB storage mode requested");
      return;
    }
    if (args.leaveUsb) {
      await waitReady(port, args.listenMs, "#ESP2USBMSC LEAVE", "#ESP2USBMSC LEFT");
      console.log("USB storage leave requested");
      return;
    }

    await waitReady(port, args.listenMs, "#ESP2UPLOAD", "#ESP2UPLOAD READY");
    await new Promise((r) => setTimeout(r, 400));
    port.removeAllListeners("data");
    try {
      while (port.readableLength > 0) {
        port.read(port.readableLength);
      }
    } catch (_) {
      /* ignore */
    }
    await new Promise((r) => setTimeout(r, 100));

    if (args.verify) {
      await verifyRemote(port, args.verify);
      return;
    }
    if (!args.file || !args.target) {
      throw new Error("--file and --target required for upload");
    }
    const result = await uploadFile(
      port,
      args.file,
      args.target,
      args.chunk || 512,
    );
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
