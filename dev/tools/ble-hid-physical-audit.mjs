/**
 * Physical BLE HID audit helper for ESP][ (keyboard + integrated pad).
 *
 * Usage:
 *   node dev/tools/ble-hid-physical-audit.mjs COM5
 *   ESP2_BLE_PAIR_ADDR=aa:bb:… node dev/tools/ble-hid-physical-audit.mjs COM5
 *
 * Put the keyboard into pairing/discoverable mode first.
 * The PC must NOT remain the active BLE HID host for that device.
 */
import { openBleSession } from "./esp2-ble-setup.mjs";

const PORT = process.env.ESP2_PORT || process.argv[2] || "COM5";
const FORCE_ADDR = (process.env.ESP2_BLE_PAIR_ADDR || "").trim().toLowerCase();
const SCAN_SEC = Number(process.env.ESP2_BLE_SCAN_SEC || 20);
const MONITOR_MS = Number(process.env.ESP2_BLE_HIDMON_MS || 45000);

function pickTarget(devices) {
  if (FORCE_ADDR) {
    return (
      devices.find((d) => String(d.address).toLowerCase() === FORCE_ADDR) || {
        address: FORCE_ADDR,
        name: "(forced)",
        type: "unknown",
      }
    );
  }
  const scored = [...devices].sort((a, b) => {
    const score = (d) => {
      const n = String(d.name || "").toLowerCase();
      let s = 0;
      if (/keyboard|kbd|dialpad|keychron|logitech|mx /.test(n)) s += 50;
      if (d.type === "keyboard" || d.type === "composite") s += 40;
      if (d.type === "hid") s += 20;
      if (d.rssi > -70) s += 10;
      return s;
    };
    return score(b) - score(a);
  });
  return scored[0] || null;
}

async function sendRaw(session, cmd, waitMs = 3000) {
  const { port, lines } = session;
  lines.clear();
  await new Promise((resolve, reject) => {
    port.write(`${cmd}\n`, (err) => (err ? reject(err) : resolve()));
  });
  const end = Date.now() + waitMs;
  const out = [];
  while (Date.now() < end) {
    try {
      const l = await lines.waitLine(() => true, Math.min(400, end - Date.now()));
      out.push(l);
      if (l.startsWith("#ACK BLE") || l.startsWith("#NAK BLE")) break;
    } catch {
      break;
    }
  }
  return out;
}

async function main() {
  console.log(JSON.stringify({ type: "start", port: PORT, scanSec: SCAN_SEC }));
  const session = await openBleSession(PORT);
  try {
    await session.init();
    console.log(JSON.stringify({ type: "status", line: await session.status() }));
    const scan = await session.scan(SCAN_SEC);
    console.log(JSON.stringify({ type: "scan_done", line: scan.line, count: scan.devices.length }));
    for (const d of scan.devices) {
      console.log(JSON.stringify({ type: "discovered", ...d }));
    }
    const target = pickTarget(scan.devices);
    if (!target || !target.address) {
      console.log(
        JSON.stringify({
          type: "blocked",
          reason:
            "No BLE keyboard/HID candidate advertising. Put device in pairing mode and release any Windows BLE HID host bond.",
        }),
      );
      process.exitCode = 2;
      return;
    }
    console.log(JSON.stringify({ type: "pair_attempt", ...target }));
    try {
      const ack = await session.pair(target.address);
      console.log(JSON.stringify({ type: "pair_ok", line: ack }));
    } catch (e) {
      console.log(JSON.stringify({ type: "pair_fail", error: String(e.message || e) }));
      process.exitCode = 3;
      return;
    }
    console.log(JSON.stringify({ type: "status", line: await session.status() }));
    for (const cmd of [
      "#ESP2BLE HIDMON 1",
      "#ESP2BLE PADMODE joystick",
      "#ESP2BLE SENS 3",
      "#ESP2BLE RECENTER",
      "#ESP2BLE PADSTAT",
    ]) {
      const lines = await sendRaw(session, cmd);
      for (const l of lines) console.log(JSON.stringify({ type: "cmd", cmd, line: l }));
    }
    console.log(
      JSON.stringify({
        type: "monitor",
        ms: MONITOR_MS,
        hint: "Press keys and move/click the integrated pad now",
      }),
    );
    const end = Date.now() + MONITOR_MS;
    while (Date.now() < end) {
      try {
        const l = await session.lines.waitLine(
          (x) => /#BLE HID|#ACK BLE|\[BLE\]/.test(x),
          1000,
        );
        console.log(JSON.stringify({ type: "hid", line: l }));
      } catch {
        /* idle */
      }
    }
    await sendRaw(session, "#ESP2BLE HIDMON 0");
    const known = await session.listKnown();
    console.log(JSON.stringify({ type: "known", line: known.line, devices: known.devices }));
    console.log(JSON.stringify({ type: "done" }));
  } finally {
    await session.close();
  }
}

main().catch((e) => {
  console.error(JSON.stringify({ type: "fatal", error: String(e && e.message ? e.message : e) }));
  process.exit(1);
});
