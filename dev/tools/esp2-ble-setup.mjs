/**
 * ESP][ BLE setup CDC client + form model helpers.
 * BlueShift device names are preserved as ordinary strings (no special-case).
 */
import { SerialPort } from "serialport";
import { createLineReader } from "./esp2-serial-framing.mjs";

export const BLE_CONFIG_SCHEMA = 1;

/** Default local control bindings (human-readable). */
export function defaultControlBindings() {
  return {
    resetAppleIi: {
      keyboard: { ctrl: true, alt: true, shift: false, meta: false, key: "R" },
      gamepad: { buttons: ["start", "b"], holdMs: 1500 },
    },
    openSettings: {
      keyboard: { ctrl: true, alt: true, shift: false, meta: false, key: "S" },
      gamepad: { buttons: ["start", "select"], holdMs: 1500 },
    },
  };
}

export function formatKeyboardBinding(b) {
  if (!b || !b.key) return "(none)";
  return `${b.ctrl ? "Ctrl+" : ""}${b.alt ? "Alt+" : ""}${b.shift ? "Shift+" : ""}${
    b.meta ? "GUI+" : ""
  }${String(b.key).toUpperCase()}`;
}

export function formatGamepadChord(b) {
  if (!b || !Array.isArray(b.buttons) || !b.buttons.length) return "(none)";
  const keys = b.buttons.map((x) => String(x)).join("+");
  const hold = Number(b.holdMs) > 0 ? Number(b.holdMs) : 1500;
  return `${keys} (hold ${(hold / 1000).toFixed(1)} s)`;
}

/**
 * Parse `#BLE DEV ...` / `#BLE KNOWN ...` lines from firmware.
 * @param {string} line
 */
export function parseBleDeviceLine(line) {
  const s = String(line || "").trim();
  const known = s.startsWith("#BLE KNOWN ");
  const disc = s.startsWith("#BLE DEV ");
  if (!known && !disc) return null;
  const get = (key) => {
    const m = s.match(new RegExp(`${key}=(?:"([^"]*)"|(\\S+))`));
    return m ? (m[1] != null ? m[1] : m[2]) : "";
  };
  return {
    known,
    index: Number(get("index") || 0),
    name: get("name") || "(unnamed)",
    type: get("type") || "unknown",
    address: get("addr") || "",
    addrType: Number(get("addrType") || 0),
    rssi: Number(get("rssi") || 0),
    isKnown: get("known") === "1" || known,
    connected: get("connected") === "1",
    autoReconnect: get("auto") === "1",
    bonded: get("bonded") === "1",
  };
}

/** Transparent name rule: keep "(BlueShift)" suffix as display text only. */
export function displayNameIsTransparent(name) {
  return String(name || "").includes("(BlueShift)");
}

/**
 * Build µGulp form for BLE management (one workflow).
 * @param {{
 *   port?: string,
 *   portOptions?: { value: string, label: string }[],
 *   discovered?: object[],
 *   known?: object[],
 *   control?: object,
 * }} state
 */
export function buildBleSetupForm(state = {}) {
  const discovered = state.discovered || [];
  const known = state.known || [];
  const control = state.control || defaultControlBindings();

  const discOpts =
    discovered.length > 0
      ? discovered.map((d) => ({
          value: d.address,
          label: `${d.name} · ${d.type} · ${d.rssi} dBm${d.isKnown ? " · known" : ""}`,
        }))
      : [{ value: "", label: "(scan first — no devices yet)" }];

  const knownOpts =
    known.length > 0
      ? known.map((d) => ({
          value: d.address,
          label: `${d.name} · ${d.type} · ${d.address}`,
        }))
      : [{ value: "", label: "(no known devices)" }];

  const portOptions =
    Array.isArray(state.portOptions) && state.portOptions.length > 0
      ? state.portOptions
      : [{ value: "", label: "(no serial ports found)" }];
  const preferredPort = String(state.port || "").trim();
  const portDefault = portOptions.some((o) => o.value === preferredPort)
    ? preferredPort
    : portOptions[0]?.value || "";

  return {
    title: 'Set up BLE devices — scan / pair / manage / hotkeys<context="task parameter"/>',
    submitLabel: 'Run action<context="button text"/>',
    fields: [
      {
        id: "port",
        type: "select",
        default: portDefault,
        remember: false,
        required: true,
        label: 'ESP][ COM port<context="task parameter"/>',
        description:
          'USB CDC port for BLE setup. Do not enter USB Storage mode during this workflow.<context="task parameter"/>',
        options: portOptions,
      },
      {
        id: "action",
        type: "select",
        default: "scan",
        remember: false,
        label: 'Action<context="task parameter"/>',
        options: [
          { value: "scan", label: 'Scan for BLE devices<context="task parameter"/>' },
          { value: "pair", label: 'Pair selected discovered device<context="task parameter"/>' },
          { value: "connect", label: 'Connect / reconnect known device<context="task parameter"/>' },
          { value: "disconnect", label: 'Disconnect active BLE HID<context="task parameter"/>' },
          { value: "forget", label: 'Forget selected known device<context="task parameter"/>' },
          { value: "status", label: 'Show BLE status + bindings<context="task parameter"/>' },
          {
            value: "save_hotkeys",
            label: 'Save hotkey bindings to device<context="task parameter"/>',
          },
        ],
      },
      {
        id: "scanSeconds",
        type: "number",
        default: 8,
        remember: false,
        label: 'Scan duration (seconds)<context="task parameter"/>',
        visibleWhen: { action: "scan" },
      },
      {
        id: "discoveredAddress",
        type: "select",
        default: discOpts[0]?.value || "",
        remember: false,
        label: 'Discovered device<context="task parameter"/>',
        options: discOpts,
        visibleWhen: { action: "pair" },
      },
      {
        id: "knownAddress",
        type: "select",
        default: knownOpts[0]?.value || "",
        remember: false,
        label: 'Known device<context="task parameter"/>',
        options: knownOpts,
        visibleWhen: { action: ["connect", "forget"] },
      },
      {
        id: "resetKeyboard",
        type: "text",
        default: formatKeyboardBinding(control.resetAppleIi.keyboard),
        remember: false,
        label: 'Apple II Reset — keyboard<context="task parameter"/>',
        description: 'Example: Ctrl+Alt+R. Binding capture is future work.<context="task parameter"/>',
        visibleWhen: { action: "save_hotkeys" },
      },
      {
        id: "resetGamepad",
        type: "text",
        default: formatGamepadChord(control.resetAppleIi.gamepad),
        remember: false,
        label: 'Apple II Reset — gamepad chord<context="task parameter"/>',
        description: 'Example: start+b (hold 1.5 s)<context="task parameter"/>',
        visibleWhen: { action: "save_hotkeys" },
      },
      {
        id: "settingsKeyboard",
        type: "text",
        default: formatKeyboardBinding(control.openSettings.keyboard),
        remember: false,
        label: 'ESP][ Settings — keyboard<context="task parameter"/>',
        visibleWhen: { action: "save_hotkeys" },
      },
      {
        id: "settingsGamepad",
        type: "text",
        default: formatGamepadChord(control.openSettings.gamepad),
        remember: false,
        label: 'ESP][ Settings — gamepad chord<context="task parameter"/>',
        description: 'Example: start+select (hold 1.5 s)<context="task parameter"/>',
        visibleWhen: { action: "save_hotkeys" },
      },
      {
        id: "holdMs",
        type: "number",
        default: control.openSettings.gamepad.holdMs || 1500,
        remember: false,
        label: 'Gamepad hold time (ms)<context="task parameter"/>',
        visibleWhen: { action: "save_hotkeys" },
      },
    ],
  };
}

/**
 * Parse human keyboard string "Ctrl+Alt+R" → binding object.
 * @param {string} text
 */
export function parseKeyboardBindingText(text) {
  const raw = String(text || "").trim();
  if (!raw || raw === "(none)") return null;
  const parts = raw.split("+").map((p) => p.trim());
  if (!parts.length) return null;
  const key = parts[parts.length - 1];
  const flags = parts.slice(0, -1).map((p) => p.toLowerCase());
  return {
    ctrl: flags.includes("ctrl") || flags.includes("control"),
    alt: flags.includes("alt"),
    shift: flags.includes("shift"),
    meta: flags.includes("gui") || flags.includes("meta") || flags.includes("win"),
    key: key.length === 1 ? key.toUpperCase() : key,
  };
}

/**
 * Parse "start+select (hold 1.5 s)" or "start+b".
 * @param {string} text
 * @param {number} [holdMs]
 */
export function parseGamepadChordText(text, holdMs) {
  const raw = String(text || "").trim();
  if (!raw || raw === "(none)") return null;
  const holdMatch = raw.match(/hold\s+([0-9.]+)\s*s/i);
  const hold =
    holdMs != null && Number(holdMs) > 0
      ? Number(holdMs)
      : holdMatch
        ? Math.round(Number(holdMatch[1]) * 1000)
        : 1500;
  const head = raw.replace(/\(hold[^)]*\)/i, "").trim();
  const buttons = head
    .split(/[+/,]/)
    .map((s) => s.trim().toLowerCase())
    .filter(Boolean);
  if (!buttons.length) return null;
  return { buttons, holdMs: hold };
}

/**
 * Open CDC session and run BLE commands.
 * @param {string} portPath
 * @param {{ baudRate?: number }} [opts]
 */
export async function openBleSession(portPath, opts = {}) {
  const port = new SerialPort({
    path: portPath,
    baudRate: opts.baudRate || 115200,
    autoOpen: false,
  });
  await new Promise((resolve, reject) => {
    port.open((err) => (err ? reject(err) : resolve()));
  });
  // Mid-run: avoid DTR/RTS reset (would drop the live apple2_text session).
  await new Promise((resolve) => {
    port.set({ dtr: false, rts: false }, () => resolve());
  });
  await new Promise((r) => setTimeout(r, 250));
  const lines = createLineReader(port);
  lines.clear();

  async function writeCmd(cmd) {
    await new Promise((resolve, reject) => {
      port.write(`${cmd}\n`, (err) => (err ? reject(err) : resolve()));
    });
    await new Promise((resolve, reject) => {
      port.drain((err) => (err ? reject(err) : resolve()));
    });
  }

  async function send(cmd, ackPrefix, timeoutMs = 20000) {
    lines.clear();
    await writeCmd(cmd);
    const line = await lines.waitLine(
      (l) => l.startsWith(ackPrefix) || l.startsWith("#NAK BLE"),
      timeoutMs,
    );
    if (line.startsWith("#NAK BLE")) {
      throw new Error(line);
    }
    return line;
  }

  /**
   * Confirm the CDC path speaks ESP][ before BLE commands.
   * @returns {Promise<boolean>}
   */
  async function probeEsp2() {
    lines.clear();
    await writeCmd("#ESP2CONFIG STATUS");
    try {
      const line = await lines.waitLine(
        (l) => l.startsWith("#ACK CONFIG") || l.startsWith("#NAK"),
        4000,
      );
      return line.startsWith("#ACK CONFIG");
    } catch {
      return false;
    }
  }

  return {
    port,
    lines,
    async init() {
      const alive = await probeEsp2();
      if (!alive) {
        throw new Error(
          `No ESP][ CDC reply on ${portPath}. Free the port (close monitor/bridge), ` +
            `confirm apple2_text is running, then retry.`,
        );
      }
      let lastErr = null;
      for (let attempt = 1; attempt <= 3; ++attempt) {
        try {
          return await send("#ESP2BLE INIT", "#ACK BLE INIT", 10000);
        } catch (e) {
          lastErr = e;
          const msg = String(e && e.message ? e.message : e);
          if (msg.startsWith("#NAK BLE")) {
            throw e;
          }
          await new Promise((r) => setTimeout(r, 400));
        }
      }
      throw new Error(
        `BLE INIT ack timeout on ${portPath}. Flash env apple2_text with ESP2_BLE_HID=1 ` +
          `(pio run -e apple2_text -t upload), then retry. (${lastErr && lastErr.message ? lastErr.message : lastErr})`,
      );
    },
    async scan(sec = 8) {
      const collected = [];
      lines.clear();
      await writeCmd(`#ESP2BLE SCAN ${sec}`);
      const deadline = Date.now() + (Number(sec) + 5) * 1000;
      let done = null;
      while (Date.now() < deadline) {
        let line;
        try {
          line = await lines.waitLine(() => true, Math.max(200, deadline - Date.now()));
        } catch {
          break;
        }
        const d = parseBleDeviceLine(line);
        if (d && !d.known) collected.push(d);
        if (line.startsWith("#ACK BLE SCAN done") || line.startsWith("#NAK BLE")) {
          done = line;
          break;
        }
      }
      return { line: done, devices: collected };
    },
    async listKnown() {
      const collected = [];
      lines.clear();
      await writeCmd("#ESP2BLE LIST KNOWN");
      const deadline = Date.now() + 5000;
      let done = null;
      while (Date.now() < deadline) {
        let line;
        try {
          line = await lines.waitLine(() => true, Math.max(200, deadline - Date.now()));
        } catch {
          break;
        }
        const d = parseBleDeviceLine(line);
        if (d && d.known) collected.push(d);
        if (line.startsWith("#ACK BLE LIST KNOWN") || line.startsWith("#NAK BLE")) {
          done = line;
          break;
        }
      }
      return { line: done, devices: collected };
    },
    async status() {
      return send("#ESP2BLE STATUS", "#ACK BLE STATUS", 5000);
    },
    async bindings() {
      return send("#ESP2BLE BINDINGS", "#ACK BLE BINDINGS", 5000);
    },
    async pair(address) {
      return send(`#ESP2BLE PAIR ${address}`, "#ACK BLE PAIR", 30000);
    },
    async connect(address) {
      return send(`#ESP2BLE CONNECT ${address}`, "#ACK BLE CONNECT", 30000);
    },
    async disconnect() {
      return send("#ESP2BLE DISCONNECT", "#ACK BLE DISCONNECT", 5000);
    },
    async forget(address) {
      return send(`#ESP2BLE FORGET ${address}`, "#ACK BLE FORGET", 8000);
    },
    async close() {
      lines.dispose();
      await new Promise((resolve) => port.close(() => resolve()));
    },
  };
}
