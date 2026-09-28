#!/usr/bin/env node
/**
 * Observe standalone config boot — NEVER sends MOUNT/BOOT/KEY.
 * Expects system.json + macros already on SD; firmware just flashed/reset.
 */
import { SerialPort } from "serialport";
import { createLineReader } from "./esp2-serial-framing.mjs";

const PORT = process.env.ESP2_PORT || process.argv[2] || "COM5";
const MAX_MS = Number(process.env.ESP2_STANDALONE_MS || 240000);

async function main() {
  const port = new SerialPort({ path: PORT, baudRate: 115200, autoOpen: false });
  await new Promise((res, rej) => port.open((e) => (e ? rej(e) : res())));
  // Do not toggle DTR/RTS — avoid extra resets; wait for natural boot after flash.
  const reader = createLineReader(port);
  const flags = {
    configOk: false,
    macrosOk: false,
    mount: false,
    boot: false,
    macroArmed: false,
    macroDone: false,
    presentLand: false,
    presentArt: false,
    visible: false,
  };
  console.log(`# observe standalone config boot on ${PORT} (no host commands) max=${MAX_MS}ms`);
  const until = Date.now() + MAX_MS;
  while (Date.now() < until) {
    try {
      const l = await reader.waitLine(() => true, 1000);
      if (
        /CONFIG|MACRO|DISK|PRESENT|GALAXIAN|LEVEL_5|SELFTEST|ACK|NAK|orient|color/.test(l)
      ) {
        console.log(l);
      }
      if (l.includes("system.json ok") || l.includes("CONFIG") && l.includes("drive1=")) {
        flags.configOk = true;
      }
      if (l.includes("macros.json count=")) flags.macrosOk = true;
      if (l.includes("#ACK DISK MOUNT") || l.includes("mount path=/esp2/disks/Galaxian")) {
        flags.mount = true;
      }
      if (l.includes("#ACK DISK BOOT") || l.includes("bootFromDisk ok")) flags.boot = true;
      if (l.includes("startup macro=") && l.includes("armed")) flags.macroArmed = true;
      if (l.includes("#ACK MACRO DONE") || l.includes("[MACRO] done")) flags.macroDone = true;
      if (l.includes("orient=LANDSCAPE") || l.includes("orient=landscape")) {
        flags.presentLand = true;
      }
      if (l.includes("color=ARTIFACT") || l.includes("color=artifact")) {
        flags.presentArt = true;
      }
      if (l.includes("ESP32_VISIBLE")) {
        flags.visible = true;
        console.log("# VISIBLE — stability 45s then leave running");
        const stab = Date.now() + 45000;
        while (Date.now() < stab) {
          try {
            const x = await reader.waitLine(() => true, 1000);
            if (/PERF|GALAXIAN|MACRO|DISPLAY-POWER|HB/.test(x)) console.log(x);
          } catch {
            /* */
          }
        }
        break;
      }
    } catch {
      /* timeout slice */
    }
  }
  console.log("# FLAGS " + JSON.stringify(flags));
  const pass =
    flags.configOk &&
    flags.mount &&
    flags.boot &&
    flags.macroArmed &&
    flags.visible &&
    flags.presentLand &&
    flags.presentArt;
  console.log(pass ? "STANDALONE_GALAXIAN=PASS" : "STANDALONE_GALAXIAN=FAIL");
  reader.dispose();
  // Leave port closed; device keeps running.
  await new Promise((r) => port.close(() => r()));
  process.exit(pass ? 0 : 1);
}

main().catch((e) => {
  console.error(e);
  process.exit(1);
});
