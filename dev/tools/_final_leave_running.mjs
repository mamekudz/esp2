import { SerialPort } from "serialport";
import { createLineReader } from "./esp2-serial-framing.mjs";

const p = new SerialPort({ path: "COM5", baudRate: 115200, autoOpen: false });
await new Promise((r, j) => p.open((e) => (e ? j(e) : r())));
await new Promise((r) => p.set({ dtr: false, rts: false }, () => r()));
const reader = createLineReader(p);
let ok = false;
const until = Date.now() + 240000;
while (Date.now() < until) {
  try {
    const l = await reader.waitLine(() => true, 1000);
    if (
      /ss_s=300|bootFromDisk ok|MACRO DONE|ESP32_VISIBLE|color=ARTIFACT orient=LANDSCAPE/.test(
        l,
      )
    ) {
      console.log(l);
    }
    if (l.includes("ESP32_VISIBLE")) {
      ok = true;
      break;
    }
  } catch {
    /* */
  }
}
console.log(ok ? "FINAL_LEAVE_RUNNING=PASS" : "FINAL_LEAVE_RUNNING=FAIL");
reader.dispose();
await new Promise((r) => p.close(() => r()));
console.log("# device left running on Galaxian — serial closed");
process.exit(ok ? 0 : 1);
