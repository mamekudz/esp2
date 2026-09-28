/**
 * Wait until firmware is past boot WAIT / SCHED, then mid-run upload.
 */
import { spawnSync } from "node:child_process";
import { SerialPort } from "serialport";
import { createLineReader } from "./esp2-serial-framing.mjs";

const portPath = process.argv[2] || "COM5";
const file = process.argv[3];
const target = process.argv[4];
if (!file || !target) {
  console.error("Usage: node _midrun_upload.mjs COM5 <file> <target>");
  process.exit(2);
}

const port = new SerialPort({ path: portPath, baudRate: 115200, autoOpen: false });
await new Promise((r, j) => port.open((e) => (e ? j(e) : r())));
await new Promise((r) => port.set({ dtr: false, rts: false }, () => r()));
const reader = createLineReader(port);

const deadline = Date.now() + 120000;
let sawLive = false;
while (Date.now() < deadline) {
  const line = reader.shift();
  if (line) {
    console.log("L", line);
    if (line.includes("[SCHED]") || line.includes("[PERF]") || line.includes("READY")) {
      sawLive = true;
      break;
    }
  } else {
    await new Promise((r) => setTimeout(r, 50));
  }
}
reader.dispose();
await new Promise((r) => port.close(() => r()));

if (!sawLive) {
  console.error("FAIL no live firmware logs");
  process.exit(1);
}

await new Promise((r) => setTimeout(r, 500));
const r = spawnSync(
  process.execPath,
  [
    "dev/tools/esp2-upload.mjs",
    "--port",
    portPath,
    "--file",
    file,
    "--target",
    target,
    "--listen-ms",
    "20000",
  ],
  { encoding: "utf8", stdio: "inherit" },
);
process.exit(r.status ?? 1);
