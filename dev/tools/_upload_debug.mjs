import { SerialPort } from "serialport";
import { readFileSync } from "fs";
import { createHash } from "crypto";
import { createLineReader } from "./esp2-serial-framing.mjs";
import { MAGIC, PROTOCOL_VERSION, FrameType } from "./esp2-upload.mjs";

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
  return Buffer.concat([u32(MAGIC), Buffer.from([PROTOCOL_VERSION, type, 0, 0])]);
}

const port = new SerialPort({ path: "COM5", baudRate: 115200, autoOpen: false });
await new Promise((res, rej) => port.open((e) => (e ? rej(e) : res())));
await new Promise((res) => port.set({ dtr: false, rts: false }, () => res()));
await new Promise((r) => setTimeout(r, 300));
const reader = createLineReader(port);

let ready = false;
for (let i = 0; i < 40; i++) {
  port.write("#ESP2UPLOAD\n");
  try {
    const line = await reader.waitLine(
      (l) => l.includes("READY") || l.startsWith("#DIAG sd") || l.startsWith("#NAK"),
      500,
    );
    console.log("GOT", line);
    if (line.includes("READY")) {
      ready = true;
      break;
    }
    if (line.startsWith("#NAK")) break;
  } catch {
    /* retry */
  }
}
if (!ready) {
  console.log("FAIL no READY");
  await new Promise((r) => port.close(() => r()));
  process.exit(1);
}

// Keep DIAG lines; only clear after a short settle.
await new Promise((r) => setTimeout(r, 300));
while (reader.shift()) {
  /* drain stale */
}

const data = readFileSync("local/roms/system.rom.provenance.json");
const sha = createHash("sha256").update(data).digest();
const target = "/esp2/roms/_probe.json";
const begin = Buffer.concat([
  frameHeader(FrameType.Begin),
  u32(Buffer.byteLength(target, "utf8")),
  Buffer.from(target, "utf8"),
  u64(data.length),
  sha,
  u32(512),
]);
console.log("sending BEGIN", begin.length, "file", data.length);
await new Promise((res, rej) =>
  port.write(begin, (e) => (e ? rej(e) : port.drain((e2) => (e2 ? rej(e2) : res())))),
);

const deadline = Date.now() + 25000;
while (Date.now() < deadline) {
  const l = reader.shift();
  if (l) {
    console.log("LINE", JSON.stringify(l));
    if (l.startsWith("#ACK BEGIN") || l.startsWith("#ESP2UPLOAD DONE") || l.startsWith("#NAK")) {
      if (l.startsWith("#ACK BEGIN")) {
        console.log("SUCCESS got BEGIN ack");
      }
      // keep reading a bit more for DONE
      if (l.startsWith("#ESP2UPLOAD DONE") || l.startsWith("#ACK BEGIN")) {
        /* continue briefly */
      }
      if (l.startsWith("#NAK") || l.startsWith("#ESP2UPLOAD DONE")) break;
    }
  } else {
    await new Promise((r) => setTimeout(r, 20));
  }
}
await new Promise((r) => port.close(() => r()));
