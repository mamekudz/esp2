#!/usr/bin/env node
/**
 * ESP][ Windows development input bridge (temporary).
 *
 * Composes Windows keyboard + XInput gamepad + Logitech Dial (relative→absolute)
 * → USB CDC → #ESP2INPUT → Apple II latch / PDL / PB.
 *
 * Usage:
 *   node dev/tools/esp2-input-bridge.mjs --port COM5 --list-gamepads
 *   node dev/tools/esp2-input-bridge.mjs --port COM5 --list-dials
 *   node dev/tools/esp2-input-bridge.mjs --port COM5 --diag
 *   node dev/tools/esp2-input-bridge.mjs --port COM5 --gamepad-id 8BitDo
 *   node dev/tools/esp2-input-bridge.mjs --port COM5 --dial --dial-sensitivity 2
 *   node dev/tools/esp2-input-bridge.mjs --port COM5 --dial --gamepad-id 8BitDo
 *
 * Composition example: PDL0←dial, PDL1←gamepad Y, PB0←gamepad A
 * (MX Dial is rotation-only — no physical press switch.)
 *
 * Not the final controller architecture (see docs/architecture/input-providers.md).
 */

import { spawn } from "node:child_process";
import { createInterface } from "node:readline";
import { resolve, dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { SerialPort } from "serialport";
import { createLineReader, DevCommand } from "./esp2-serial-framing.mjs";
import {
  VirtualPaddleAccumulator,
  DEFAULT_DIAL_SENSITIVITY,
  PADDLE_CENTER,
} from "./esp2-paddle-accumulator.mjs";

const __dirname = dirname(fileURLToPath(import.meta.url));

/** Default deadzone on normalized −1..+1 stick (documented). */
export const DEFAULT_DEADZONE = 0.08;

/** PAD refresh Hz when axes/buttons held (bounded CDC). */
const PAD_HZ = 30;

function parseArgs(argv) {
  const out = {
    port: process.env.ESP2_PORT || "",
    /** null | "none" | "auto" | 0..3 */
    gamepad: null,
    gamepadId: "",
    profile: "",
    configPath: "",
    listGamepads: false,
    listDials: false,
    diag: false,
    dialDiag: false,
    deadzone: DEFAULT_DEADZONE,
    keyboard: true,
    dial: false,
    dialSensitivity: DEFAULT_DIAL_SENSITIVITY,
    /** pdl0 source: dial | gamepadX | none */
    pdl0: "auto",
    /** pdl1 source: gamepadY | none */
    pdl1: "auto",
    /** pb0: gamepadA | none (legacy dialPress/or still accepted) */
    pb0: "auto",
    help: false,
  };
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    const next = () => argv[++i];
    if (a === "--port") out.port = next();
    else if (a === "--gamepad") {
      const v = String(next()).trim().toLowerCase();
      if (v === "none" || v === "auto") out.gamepad = v;
      else out.gamepad = Number(v);
    } else if (a === "--gamepad-id") out.gamepadId = String(next());
    else if (a === "--profile") out.profile = String(next());
    else if (a === "--config") out.configPath = String(next());
    else if (a === "--list-gamepads") out.listGamepads = true;
    else if (a === "--list-dials") out.listDials = true;
    else if (a === "--diag") out.diag = true;
    else if (a === "--dial-diag") out.dialDiag = true;
    else if (a === "--dial") out.dial = true;
    else if (a === "--dial-sensitivity") out.dialSensitivity = Number(next());
    else if (a === "--pdl0") out.pdl0 = String(next());
    else if (a === "--pdl1") out.pdl1 = String(next());
    else if (a === "--pb0") out.pb0 = String(next());
    else if (a === "--deadzone") out.deadzone = Number(next());
    else if (a === "--no-keyboard") out.keyboard = false;
    else if (a === "--help" || a === "-h") out.help = true;
  }
  if (out.dialDiag) {
    out.dial = true;
    out.diag = true;
  }
  return out;
}

/**
 * Apply system.json → input.hostBridge defaults onto bridge CLI args.
 * Explicit CLI flags keep precedence (caller merges after).
 * @param {object} hostBridge
 * @param {ReturnType<typeof parseArgs>} args
 */
export function applyHostBridgeToArgs(hostBridge, args) {
  if (!hostBridge || typeof hostBridge !== "object") return args;
  const hb = hostBridge;
  if (args.gamepad == null && hb.gamepad != null && hb.gamepad !== "") {
    const g = String(hb.gamepad).trim().toLowerCase();
    if (g === "none" || g === "auto") args.gamepad = g;
    else if (Number.isFinite(Number(g))) args.gamepad = Number(g);
  }
  if (!args.gamepadId && hb.gamepadId) args.gamepadId = String(hb.gamepadId);
  if (args.pdl0 === "auto" && hb.pdl0 && hb.pdl0 !== "auto") args.pdl0 = String(hb.pdl0);
  if (args.pdl1 === "auto" && hb.pdl1 && hb.pdl1 !== "auto") args.pdl1 = String(hb.pdl1);
  if (args.pb0 === "auto" && hb.pb0 && hb.pb0 !== "auto") args.pb0 = String(hb.pb0);
  if (!args.dial && hb.dial) args.dial = true;
  if (hb.keyboard === false) args.keyboard = false;
  if (Number.isFinite(Number(hb.deadzone))) args.deadzone = Number(hb.deadzone);
  return args;
}

/**
 * Resolve --profile / --config into hostBridge and merge into args.
 * @param {ReturnType<typeof parseArgs>} args
 * @param {string} [repoRoot]
 */
export async function loadHostBridgeIntoArgs(args, repoRoot) {
  const { readFileSync, existsSync } = await import("node:fs");
  const { join } = await import("node:path");
  const {
    normalizeSystemConfig,
    profilePaths,
    localConfigPaths,
  } = await import("./device-config-form.mjs");

  const root = repoRoot || join(__dirname, "../..");
  let path = args.configPath ? String(args.configPath) : "";
  if (!path && args.profile) {
    path = profilePaths(root, String(args.profile)).system;
  }
  if (!path) {
    const local = localConfigPaths(root).system;
    if (existsSync(local)) path = local;
  }
  if (!path || !existsSync(path)) return args;

  const raw = JSON.parse(readFileSync(path, "utf8"));
  const n = normalizeSystemConfig(raw);
  if (!n.ok) {
    throw new Error(`config invalid (${path}): ${n.errors.join("; ")}`);
  }
  console.log(`hostBridge config: ${path}`);
  return applyHostBridgeToArgs(n.config.input.hostBridge, args);
}

/**
 * Map Windows key name / sequence → Apple II 7-bit ASCII.
 * Subset only — not a full international keyboard.
 */
export function mapWindowsKeyToApple7(seq, { ctrl = false, shift = false } = {}) {
  if (!seq) return null;
  if (seq === "return" || seq === "enter") return 0x0d;
  if (seq === "escape" || seq === "esc") return 0x1b;
  if (seq === "space") return 0x20;
  if (seq === "backspace") return 0x08;
  if (seq === "tab") return 0x09;
  if (seq === "left") return 0x08;
  if (seq === "right") return 0x15;
  if (seq === "up") return 0x0b;
  if (seq === "down") return 0x0a;
  if (seq.length === 1) {
    let c = seq.charCodeAt(0);
    if (ctrl) {
      const u = seq.toUpperCase().charCodeAt(0);
      if (u >= 65 && u <= 90) return u - 64;
      return null;
    }
    // Apple II soft-switch software usually wants uppercase letters.
    if (c >= 97 && c <= 122) c -= 32;
    if (c >= 32 && c < 127) return c;
  }
  void shift;
  return null;
}

/** Normalize XInput short (−32768..32767) → −1..+1 with deadzone → paddle 0..255. */
export function axisToPaddle(norm, deadzone = DEFAULT_DEADZONE) {
  let v = Number(norm);
  if (!Number.isFinite(v)) v = 0;
  if (v > 1) v = 1;
  if (v < -1) v = -1;
  const dz = Math.max(0, Math.min(0.5, deadzone));
  if (Math.abs(v) < dz) return 128;
  const sign = v < 0 ? -1 : 1;
  const mag = (Math.abs(v) - dz) / (1 - dz);
  const scaled = sign * mag;
  const p = Math.round((scaled + 1) * 127.5);
  return Math.max(0, Math.min(255, p));
}

async function openPort(path) {
  const port = new SerialPort({ path, baudRate: 115200, autoOpen: false });
  await new Promise((resolveP, reject) => {
    port.open((err) => (err ? reject(err) : resolveP()));
  });
  await new Promise((resolveP) => {
    port.set({ dtr: false, rts: false }, () => resolveP());
  });
  await new Promise((r) => setTimeout(r, 200));
  return port;
}

function writeLine(port, line) {
  return new Promise((resolveP, reject) => {
    port.write(`${line}\n`, (err) => (err ? reject(err) : resolveP()));
  });
}

const XINPUT_HOST_PS1 = join(__dirname, "esp2-xinput-host.ps1");

/**
 * Run the host PowerShell gamepad helper.
 * @param {string[]} args
 * @returns {Promise<{ code: number, out: string, err: string }>}
 */
function runXInputHost(args) {
  return new Promise((resolveP, reject) => {
    const child = spawn(
      "powershell.exe",
      ["-NoProfile", "-ExecutionPolicy", "Bypass", "-File", XINPUT_HOST_PS1, ...args],
      { windowsHide: true },
    );
    let out = "";
    let err = "";
    child.stdout.on("data", (d) => {
      out += d.toString();
    });
    child.stderr.on("data", (d) => {
      err += d.toString();
    });
    child.on("error", reject);
    child.on("close", (code) => {
      resolveP({ code: code ?? 1, out, err });
    });
  });
}

/**
 * Enumerate XInput slots (+ PnP / RawGameController hints).
 * @param {{ enrichNames?: boolean }} [opts]
 */
export async function listXInputGamepads(opts = {}) {
  void opts;
  let best = [];
  let pnpNames = [];
  let rawPads = [];
  for (let pass = 0; pass < 3; pass++) {
    const { code, out, err } = await runXInputHost(["-Mode", "list"]);
    if (code !== 0 && !out) {
      throw new Error(err || `esp2-xinput-host list exit ${code}`);
    }
    const pads = [];
    pnpNames = [];
    rawPads = [];
    for (const line of out.split(/\r?\n/).map((l) => l.trim()).filter(Boolean)) {
      if (line.startsWith("pnp|")) {
        pnpNames.push(line.slice(4));
        continue;
      }
      if (line.startsWith("raw|")) {
        const parts = line.split("|");
        rawPads.push({
          index: Number(parts[1]),
          name: parts[2] || `Raw#${parts[1]}`,
          buttonCount: Number(parts[3]) || 0,
          pressed: parts[4] === "1",
        });
        continue;
      }
      const parts = line.startsWith("xi|") ? line.slice(3).split("|") : line.split("|");
      if (parts.length < 5) continue;
      const index = Number(parts[0]);
      if (!Number.isFinite(index)) continue;
      const product = parts[1] || `XInput#${index}`;
      const connected = parts[4] === "1";
      pads.push({
        index,
        name: product,
        id: product,
        label: connected ? `#${index} · ${product}` : `#${index} · asleep/offline`,
        axes: Number(parts[2]) || 0,
        buttons: Number(parts[3]) || 0,
        connected,
      });
    }
    if (pads.length === 4) {
      best = pads;
      if (pads.some((p) => p.connected) || rawPads.length) break;
    }
    if (pass < 2) await new Promise((r) => setTimeout(r, 200));
  }
  if (!best.length) {
    best = [0, 1, 2, 3].map((index) => ({
      index,
      name: `XInput#${index}`,
      id: `XInput#${index}`,
      label: `#${index} · asleep/offline`,
      axes: 0,
      buttons: 0,
      connected: false,
    }));
  }
  best.pnpNames = pnpNames;
  best.rawPads = rawPads;
  return best;
}

/**
 * Wait until any XInput or RawGameController shows a button press.
 * @param {{ timeoutMs?: number }} [opts]
 */
export async function waitForXInputButtonPress(opts = {}) {
  const timeoutMs = Math.max(1000, Number(opts.timeoutMs ?? 25000));
  const { code, out, err } = await runXInputHost([
    "-Mode",
    "wait",
    "-TimeoutMs",
    String(timeoutMs),
  ]);
  if (code !== 0 && !out) {
    throw new Error(err || `esp2-xinput-host wait exit ${code}`);
  }
  const line = out
    .split(/\r?\n/)
    .map((l) => l.trim())
    .filter(Boolean)
    .pop();
  if (!line || line === "timeout") return null;
  const m = /^press\s+(\d+)\s+(\d+)(?:\s+(\w+)(?:\s+(.*))?)?$/.exec(line);
  if (!m) return null;
  return {
    index: Number(m[1]),
    buttons: Number(m[2]),
    api: m[3] || "xinput",
    name: m[4] ? String(m[4]).trim() : undefined,
  };
}

/**
 * Brief rumble so the user can feel which pad was picked.
 * @param {number} index
 * @param {number} [ms]
 */
export async function pulseXInputRumble(index, ms = 350) {
  const idx = Number(index);
  if (!Number.isFinite(idx) || idx < 0 || idx > 3) return;
  await runXInputHost([
    "-Mode",
    "rumble",
    "-Index",
    String(idx),
    "-RumbleMs",
    String(Math.max(80, Math.min(2000, Number(ms) || 350))),
  ]);
}

/**
 * Build select options for hostBridge.gamepad from a live pad list.
 * @param {Awaited<ReturnType<typeof listXInputGamepads>>} pads
 * @param {{ includePress?: boolean }} [opts]
 */
export function buildGamepadSelectOptions(pads, opts = {}) {
  /** @type {{ value: string, label: string }[]} */
  const options = [
    { value: "none", label: "None" },
    { value: "auto", label: "Auto (first connected)" },
  ];
  if (opts.includePress !== false) {
    options.push({
      value: "press",
      label: "▶ Press any button on the desired pad…",
    });
  }
  for (const p of pads || []) {
    options.push({
      value: String(p.index),
      label: p.connected
        ? p.label || `#${p.index} · ${p.name}`
        : `#${p.index} · asleep/offline (still selectable)`,
    });
  }
  // PnP-only names (e.g. 8BitDo in D-input) — not selectable as XInput yet.
  const used = new Set((pads || []).filter((p) => p.connected).map((p) => p.name.toLowerCase()));
  for (const name of pads?.pnpNames || []) {
    if (used.has(String(name).toLowerCase())) continue;
    options.push({
      value: "none",
      label: `⚠ ${name} (seen, not XInput — switch pad to X-input mode)`,
    });
  }
  return options;
}

/**
 * Poll one XInput index forever; emit JSON lines on stdout of child.
 */
function startXInputPoller(index, hz) {
  const intervalMs = Math.max(16, Math.floor(1000 / hz));
  const ps = `
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class Esp2XInput {
  [StructLayout(LayoutKind.Sequential)]
  public struct XINPUT_GAMEPAD {
    public ushort wButtons;
    public byte bLeftTrigger;
    public byte bRightTrigger;
    public short sThumbLX;
    public short sThumbLY;
    public short sThumbRX;
    public short sThumbRY;
  }
  [StructLayout(LayoutKind.Sequential)]
  public struct XINPUT_STATE {
    public uint dwPacketNumber;
    public XINPUT_GAMEPAD Gamepad;
  }
  [DllImport("xinput1_4.dll")]
  public static extern int XInputGetState(int dwUserIndex, out XINPUT_STATE pState);
}
"@
$idx = ${Number(index)}
while ($true) {
  $st = New-Object Esp2XInput+XINPUT_STATE
  $r = [Esp2XInput]::XInputGetState($idx, [ref]$st)
  if ($r -eq 0) {
    $g = $st.Gamepad
    $lx = [math]::Round($g.sThumbLX / 32767.0, 4)
    $ly = [math]::Round(-$g.sThumbLY / 32767.0, 4)
    $b = $g.wButtons
    $a = if ($b -band 0x1000) {1} else {0}
    $bb = if ($b -band 0x2000) {1} else {0}
    $x = if ($b -band 0x4000) {1} else {0}
    $y = if ($b -band 0x8000) {1} else {0}
    Write-Output ("{0} {1} {2} {3} {4} {5}" -f $lx,$ly,$a,$bb,$x,$y)
  } else {
    Write-Output "0 0 0 0 0 0"
  }
  Start-Sleep -Milliseconds ${intervalMs}
}
`;
  return spawn(
    "powershell.exe",
    ["-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", ps],
    { windowsHide: true },
  );
}

export function selectGamepad(pads, args) {
  const connected = pads.filter((p) => p.connected);
  if (args.gamepadId) {
    try {
      const re = new RegExp(args.gamepadId, "i");
      const hit =
        connected.find((p) => re.test(p.name) || re.test(p.id)) ||
        pads.find((p) => re.test(p.name) || re.test(p.id));
      if (hit) return hit;
      console.log(
        `WARNING: no gamepad matching --gamepad-id ${args.gamepadId} — falling back to --gamepad`,
      );
    } catch (err) {
      console.log(
        `WARNING: invalid --gamepad-id regex (${err?.message || err}) — falling back to --gamepad`,
      );
    }
  }
  if (args.gamepad === "none" || args.gamepad === null || args.gamepad === undefined) {
    return null;
  }
  if (args.gamepad === "auto") {
    return connected[0] || null;
  }
  if (Number.isFinite(args.gamepad)) {
    const hit = pads.find((p) => p.index === args.gamepad);
    if (!hit) {
      throw new Error(`gamepad index ${args.gamepad} not found`);
    }
    // Allow asleep/offline slots — Bluetooth pads often wake on first poll.
    return hit;
  }
  return null;
}

/**
 * Enumerate Logitech-ish HID devices (reuse host PnP — no second stack).
 * Dial rotation uses Raw Input mouse-wheel channel when Options maps the dial as a wheel.
 */
export async function listLogitechDialCandidates() {
  const ps = `
Get-CimInstance Win32_PnPEntity |
  Where-Object { $_.Name -match 'Logitech|Craft|Dial|Options|MX' -or $_.PNPDeviceID -match 'VID_046D' } |
  Select-Object -First 40 Name, PNPDeviceID |
  ForEach-Object { '{0}|{1}' -f $_.Name, $_.PNPDeviceID }
`;
  return new Promise((resolveP, reject) => {
    const child = spawn(
      "powershell.exe",
      ["-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", ps],
      { windowsHide: true },
    );
    let out = "";
    child.stdout.on("data", (d) => {
      out += d.toString();
    });
    child.on("close", (code) => {
      if (code !== 0 && !out) {
        reject(new Error(`dial enumerate exit ${code}`));
        return;
      }
      const devices = out
        .split(/\r?\n/)
        .map((l) => l.trim())
        .filter(Boolean)
        .map((l, i) => {
          const [name, id] = l.split("|");
          return {
            index: i,
            name: name || "unknown",
            id: id || "",
            provider: "windows_pnp_hid",
            relativeApi: "rawinput_mouse_wheel",
          };
        });
      resolveP(devices);
    });
  });
}

/**
 * Raw Input dial capture:
 *  - mouse wheel / HWHEEL (when Options+ maps Dial→scroll for this app)
 *  - Consumer Control volume up/down (when Options+ uses contextual volume —
 *    we observe HID and optionally also AudioEndpoint steps)
 * Emits: "delta <n>" / "press 1|0"
 */
function startDialWheelPoller() {
  const ps = `
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Windows.Forms;

public class Esp2DialForm : Form {
  private const int WM_INPUT = 0x00FF;
  private const int RID_INPUT = 0x10000003;
  private const int RIM_TYPEMOUSE = 0;
  private const int RIM_TYPEHID = 2;
  private const uint RIDEV_INPUTSINK = 0x00000100;
  private const uint RIDEV_PAGEONLY = 0x00000020;
  [StructLayout(LayoutKind.Sequential)]
  struct RAWINPUTDEVICE { public ushort usUsagePage; public ushort usUsage; public uint dwFlags; public IntPtr hwndTarget; }
  [StructLayout(LayoutKind.Sequential)]
  struct RAWINPUTHEADER { public uint dwType; public uint dwSize; public IntPtr hDevice; public IntPtr wParam; }
  [StructLayout(LayoutKind.Sequential)]
  struct RAWMOUSE {
    public ushort usFlags; public ushort usButtonFlags; public ushort usButtonData;
    public uint ulRawButtons; public int lLastX; public int lLastY; public uint ulExtraInformation;
  }
  [StructLayout(LayoutKind.Sequential)]
  struct RAWHID { public uint dwSizeHid; public uint dwCount; /* bRawData follows */ }
  [DllImport("user32.dll")] static extern bool RegisterRawInputDevices(RAWINPUTDEVICE[] p, uint n, uint size);
  [DllImport("user32.dll")] static extern uint GetRawInputData(IntPtr h, uint ui, IntPtr p, ref uint pcb, uint cbh);
  public Esp2DialForm() {
    ShowInTaskbar = false; Opacity = 0; Width = 0; Height = 0;
    // Mouse (wheel) + entire Consumer Control page (Dial→volume via Options+).
    var rid = new RAWINPUTDEVICE[2];
    rid[0].usUsagePage = 0x01; rid[0].usUsage = 0x02;
    rid[0].dwFlags = RIDEV_INPUTSINK; rid[0].hwndTarget = this.Handle;
    rid[1].usUsagePage = 0x0C; rid[1].usUsage = 0x00;
    rid[1].dwFlags = RIDEV_INPUTSINK | RIDEV_PAGEONLY; rid[1].hwndTarget = this.Handle;
    RegisterRawInputDevices(rid, 2, (uint)Marshal.SizeOf(typeof(RAWINPUTDEVICE)));
  }
  static void EmitDelta(int notches) {
    if (notches != 0) Console.WriteLine("delta " + notches);
  }
  protected override void WndProc(ref Message m) {
    if (m.Msg == WM_INPUT) {
      uint dwSize = 0;
      GetRawInputData(m.LParam, RID_INPUT, IntPtr.Zero, ref dwSize, (uint)Marshal.SizeOf(typeof(RAWINPUTHEADER)));
      IntPtr buffer = Marshal.AllocHGlobal((int)dwSize);
      try {
        if (GetRawInputData(m.LParam, RID_INPUT, buffer, ref dwSize, (uint)Marshal.SizeOf(typeof(RAWINPUTHEADER))) == dwSize) {
          var header = Marshal.PtrToStructure<RAWINPUTHEADER>(buffer);
          if (header.dwType == RIM_TYPEMOUSE) {
            var mouse = Marshal.PtrToStructure<RAWMOUSE>(IntPtr.Add(buffer, Marshal.SizeOf(typeof(RAWINPUTHEADER))));
            bool wheel = (mouse.usButtonFlags & 0x0400) != 0;
            bool hwheel = (mouse.usButtonFlags & 0x0800) != 0;
            if (wheel || hwheel) {
              short delta = unchecked((short)mouse.usButtonData);
              int notches = delta / 120;
              if (notches == 0 && delta != 0) notches = delta > 0 ? 1 : -1;
              EmitDelta(notches);
            }
            if ((mouse.usButtonFlags & 0x0010) != 0) Console.WriteLine("press 1");
            if ((mouse.usButtonFlags & 0x0020) != 0) Console.WriteLine("press 0");
            if ((mouse.usButtonFlags & 0x0001) != 0) Console.WriteLine("press 1");
            if ((mouse.usButtonFlags & 0x0002) != 0) Console.WriteLine("press 0");
          } else if (header.dwType == RIM_TYPEHID) {
            // Consumer HID: look for Volume Increment (0xE9) / Decrement (0xEA) in raw bytes.
            int hdr = Marshal.SizeOf(typeof(RAWINPUTHEADER));
            var hid = Marshal.PtrToStructure<RAWHID>(IntPtr.Add(buffer, hdr));
            int dataOff = hdr + Marshal.SizeOf(typeof(RAWHID));
            int n = (int)(hid.dwSizeHid * hid.dwCount);
            if (n > 0 && dataOff + n <= (int)dwSize) {
              byte[] raw = new byte[n];
              Marshal.Copy(IntPtr.Add(buffer, dataOff), raw, 0, n);
              for (int i = 0; i < raw.Length; ++i) {
                if (raw[i] == 0xE9) EmitDelta(1);   // Vol+
                else if (raw[i] == 0xEA) EmitDelta(-1); // Vol-
              }
            }
          }
        }
      } finally { Marshal.FreeHGlobal(buffer); }
    }
    base.WndProc(ref m);
  }
  [STAThread] public static void Main() { Application.Run(new Esp2DialForm()); }
}
"@ -ReferencedAssemblies System.Windows.Forms
[Esp2DialForm]::Main()
`;
  return spawn(
    "powershell.exe",
    ["-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", ps],
    { windowsHide: true },
  );
}

/**
 * Fallback when Options+ keeps Dial on contextual volume and HID is not shared:
 * watch default endpoint volume steps, emit dial deltas, restore prior level so
 * the user does not hear the volume change.
 */
function startDialVolumePoller() {
  const ps = `
\$ErrorActionPreference = 'Stop'
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class Esp2Vol {
  [ComImport, Guid("BCDE0395-E52F-467C-8E3D-C4579291692E")] class MMDeviceEnumerator {}
  [Guid("A95664D2-9614-4F35-A746-DE8DB8007E4"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
  interface IMMDeviceEnumerator {
    int NotImpl1();
    [PreserveSig] int GetDefaultAudioEndpoint(int dataFlow, int role, out IMMDevice endpoint);
  }
  [Guid("D666063F-1587-4E43-81F1-B948E807363F"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
  interface IMMDevice {
    [PreserveSig] int Activate(ref Guid iid, int dwClsCtx, IntPtr pActivationParams, [MarshalAs(UnmanagedType.IUnknown)] out object ppInterface);
  }
  [Guid("5CDF2C82-841E-4546-9722-0CF74078229A"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
  interface IAudioEndpointVolume {
    int NotImpl1(); int NotImpl2(); int NotImpl3();
    [PreserveSig] int GetMasterVolumeLevelScalar(out float level);
    [PreserveSig] int SetMasterVolumeLevelScalar(float level, [MarshalAs(UnmanagedType.LPStruct)] Guid eventContext);
  }
  public static float Get() {
    var en = (IMMDeviceEnumerator)(object)new MMDeviceEnumerator();
    IMMDevice dev; en.GetDefaultAudioEndpoint(0 /*eRender*/, 1 /*eMultimedia*/, out dev);
    Guid iid = typeof(IAudioEndpointVolume).GUID;
    object o; dev.Activate(ref iid, 0, IntPtr.Zero, out o);
    var vol = (IAudioEndpointVolume)o;
    float v; vol.GetMasterVolumeLevelScalar(out v); return v;
  }
  public static void Set(float v) {
    var en = (IMMDeviceEnumerator)(object)new MMDeviceEnumerator();
    IMMDevice dev; en.GetDefaultAudioEndpoint(0, 1, out dev);
    Guid iid = typeof(IAudioEndpointVolume).GUID;
    object o; dev.Activate(ref iid, 0, IntPtr.Zero, out o);
    var vol = (IAudioEndpointVolume)o;
    vol.SetMasterVolumeLevelScalar(v, Guid.Empty);
  }
}
"@
\$prev = [Esp2Vol]::Get()
while (\$true) {
  Start-Sleep -Milliseconds 40
  try {
    \$cur = [Esp2Vol]::Get()
    \$d = \$cur - \$prev
    if ([Math]::Abs(\$d) -ge 0.008) {
      \$notches = [Math]::Max(1, [Math]::Min(6, [Math]::Round([Math]::Abs(\$d) / 0.02)))
      if (\$d -lt 0) { \$notches = -\$notches }
      Write-Output ("delta " + \$notches)
      [Esp2Vol]::Set(\$prev) | Out-Null
    } else {
      \$prev = \$cur
    }
  } catch { }
}
`;
  return spawn(
    "powershell.exe",
    ["-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", ps],
    { windowsHide: true },
  );
}

async function main() {
  let args = parseArgs(process.argv.slice(2));
  if (args.help) {
    console.log(`Usage:
  node dev/tools/esp2-input-bridge.mjs --port COM5 [--list-gamepads] [--list-dials]
  node dev/tools/esp2-input-bridge.mjs --port COM5 --diag
  node dev/tools/esp2-input-bridge.mjs --port COM5 --dial-diag
  node dev/tools/esp2-input-bridge.mjs --port COM5 --gamepad <none|auto|index>|--gamepad-id <pattern>
  node dev/tools/esp2-input-bridge.mjs --port COM5 --profile little-brick-out
  node dev/tools/esp2-input-bridge.mjs --port COM5 --config local/device/config/system.json
  node dev/tools/esp2-input-bridge.mjs --port COM5 --dial [--dial-sensitivity ${DEFAULT_DIAL_SENSITIVITY}]
  node dev/tools/esp2-input-bridge.mjs --port COM5 --dial --gamepad-id 8BitDo

Composition:
  --pdl0 dial|gamepadX   --pdl1 gamepadY|none   --pb0 gamepadA|none
Defaults:
  deadzone=${DEFAULT_DEADZONE}; dial sensitivity=${DEFAULT_DIAL_SENSITIVITY} (no wrap; clamp 0..255)
  dial API: Windows Raw Input mouse wheel (MX Dial rotation only — no press switch)
  gamepad: left stick→PDL, A→PB0 B→PB1 X→PB2
  profile/config: reads system.json input.hostBridge (CLI flags override)`);
    process.exit(0);
  }

  args = await loadHostBridgeIntoArgs(args);

  const pads = await listXInputGamepads();
  console.log("gamepads:");
  for (const p of pads) {
    console.log(
      `  index=${p.index} name=${p.name} axes=${p.axes} buttons=${p.buttons} connected=${p.connected}`,
    );
  }
  const dials = await listLogitechDialCandidates();
  console.log("logitech_dial_candidates:");
  if (dials.length === 0) {
    console.log("  (none via PnP name/VID_046D — dial may still work as mouse wheel)");
  }
  for (const d of dials) {
    console.log(`  index=${d.index} name=${d.name} id=${d.id} api=${d.relativeApi}`);
  }
  if (args.listGamepads || args.listDials) {
    process.exit(0);
  }

  const selected = selectGamepad(pads, args);
  const useDial = args.dial || args.pdl0 === "dial";
  const pdl0Src =
    args.pdl0 !== "auto"
      ? args.pdl0
      : useDial
        ? "dial"
        : selected
          ? "gamepadX"
          : "none";
  const pdl1Src =
    args.pdl1 !== "auto" ? args.pdl1 : selected ? "gamepadY" : "none";
  // MX Dial has no physical press — never default PB0 to dialPress.
  let pb0Src = args.pb0 !== "auto" ? args.pb0 : selected ? "gamepadA" : "none";
  if (pb0Src === "dialPress" || pb0Src === "or") {
    console.log("NOTE: MX Dial has no press switch — remapping PB0 dialPress/or → gamepadA");
    pb0Src = selected ? "gamepadA" : "none";
  }
  if (args.gamepad != null && args.gamepad !== "none") {
    if (!selected) {
      console.log("WARNING: gamepad requested but none connected — keyboard/dial only");
    } else {
      console.log(`selected gamepad index=${selected.index} name=${selected.name}`);
    }
  }
  console.log(
    `composition: PDL0←${pdl0Src} PDL1←${pdl1Src} PB0←${pb0Src} PB1←gamepadB dial=${useDial} sensitivity=${args.dialSensitivity}`,
  );
  if (useDial) {
    console.log(
      `dial: provider=windows_rawinput_mouse_wheel initial=${PADDLE_CENTER} clamp=0..255 press=none (MX Dial has no switch)`,
    );
    console.log(
      "dial NOTE: MX Dial is rotation-only. PB0 must come from gamepad A / keyboard. Options+ must not steal Dial→volume.",
    );
  }
  console.log(
    "input NOTE: this process IS the Windows→ESP][ forwarder. Keep this terminal focused for keyboard; gamepad uses XInput (no focus). Dial rotation uses Raw Input when Options+ does not intercept.",
  );

  if (!args.port && !args.dialDiag) {
    console.error("ERROR: --port required");
    process.exit(2);
  }

  let port = null;
  let reader = null;
  if (args.port) {
    port = await openPort(args.port);
    reader = createLineReader(port);
    reader.waitLine(() => false, 50).catch(() => {});
    await writeLine(port, DevCommand.InputLive);
    console.log("sent", DevCommand.InputLive);
  } else {
    console.log("dial-diag local-only (no CDC)");
  }

  const composed = {
    p0: PADDLE_CENTER,
    p1: PADDLE_CENTER,
    pb0: 0,
    pb1: 0,
    pb2: 0,
    gamepadA: 0,
    dialPress: 0,
  };
  const dialAcc = new VirtualPaddleAccumulator({
    sensitivity: args.dialSensitivity,
  });

  let lastPadPayload = "";
  let lastPadSentAt = 0;
  let poller = null;
  let rlPoll = null;
  let dialPoller = null;
  let dialRl = null;

  const flushPad = async (force = false) => {
    if (pdl0Src === "dial") composed.p0 = dialAcc.paddle();
    if (pb0Src === "dialPress") composed.pb0 = composed.dialPress;
    else if (pb0Src === "gamepadA") composed.pb0 = composed.gamepadA;
    else if (pb0Src === "or") {
      composed.pb0 = composed.dialPress || composed.gamepadA ? 1 : 0;
    }
    const payload = `${composed.p0} ${composed.p1} ${composed.pb0} ${composed.pb1} ${composed.pb2}`;
    const now = Date.now();
    if (!force && payload === lastPadPayload && now - lastPadSentAt < 250) {
      return;
    }
    lastPadPayload = payload;
    lastPadSentAt = now;
    if (args.diag || args.dialDiag) {
      console.log(
        `pad p0=${composed.p0} p1=${composed.p1} pb0=${composed.pb0} pb1=${composed.pb1} pb2=${composed.pb2}`,
      );
    }
    if (port) {
      await writeLine(port, `${DevCommand.InputPad} ${payload}`);
    }
  };

  if (selected) {
    poller = startXInputPoller(selected.index, PAD_HZ);
    rlPoll = createInterface({ input: poller.stdout });
    rlPoll.on("line", (raw) => {
      const parts = raw.trim().split(/\s+/).map(Number);
      if (parts.length < 6) return;
      const [nx, ny, a, b, x] = parts;
      if (pdl0Src === "gamepadX") composed.p0 = axisToPaddle(nx, args.deadzone);
      if (pdl1Src === "gamepadY") composed.p1 = axisToPaddle(ny, args.deadzone);
      composed.gamepadA = a ? 1 : 0;
      composed.pb1 = b ? 1 : 0;
      composed.pb2 = x ? 1 : 0;
      flushPad().catch((e) => console.error("pad send", e.message));
    });
  }

  if (useDial) {
    dialPoller = startDialWheelPoller();
    dialRl = createInterface({ input: dialPoller.stdout });
    dialRl.on("line", (raw) => {
      const t = raw.trim();
      if (t.startsWith("delta ")) {
        const d = Number(t.slice(6));
        const before = dialAcc.paddle();
        dialAcc.applyDelta(d);
        const after = dialAcc.paddle();
        if (args.diag || args.dialDiag) {
          console.log(`dial delta=${d} pdl0 ${before}->${after}`);
        }
        flushPad(true).catch(() => {});
      } else if (t.startsWith("press ")) {
        composed.dialPress = Number(t.slice(6)) ? 1 : 0;
        if (args.diag || args.dialDiag) {
          console.log(`dial press=${composed.dialPress} (middle-button channel)`);
        }
        flushPad(true).catch(() => {});
      }
    });
    dialPoller.stderr.on("data", (d) => {
      if (args.diag) process.stderr.write(d);
    });
  }

  if (args.keyboard && process.stdin.isTTY && port) {
    const readline = await import("node:readline");
    readline.emitKeypressEvents(process.stdin);
    process.stdin.setRawMode(true);
    console.log("keyboard: raw TTY → Apple II (Ctrl+C quits bridge)");
    process.stdin.on("keypress", (str, key) => {
      if (!key) return;
      if (key.ctrl && key.name === "c") {
        shutdown(0);
        return;
      }
      const apple = mapWindowsKeyToApple7(key.name === "return" ? "return" : str || key.name, {
        ctrl: !!key.ctrl,
        shift: !!key.shift,
      });
      if (apple == null) return;
      const line = `${DevCommand.InputKey} ${apple.toString(16)} 1`;
      writeLine(port, line).catch(() => {});
      if (args.diag) {
        console.log(`key apple=$${apple.toString(16).padStart(2, "0")} name=${key.name}`);
      }
    });
  }

  async function shutdown(code) {
    try {
      if (port) await writeLine(port, DevCommand.InputIdle);
    } catch {
      /* ignore */
    }
    if (rlPoll) rlPoll.close();
    if (poller) poller.kill();
    if (dialRl) dialRl.close();
    if (dialPoller) dialPoller.kill();
    if (reader) reader.dispose();
    if (port) await new Promise((r) => port.close(() => r()));
    process.exit(code);
  }

  process.on("SIGINT", () => shutdown(0));
  console.log("bridge live — Ctrl+C to stop");
  if (args.dialDiag) {
    console.log(
      "DIAL DIAG: rotate left→PDL0 down, right→up, stop→stable, ends clamp; press if middle mapped→PB0",
    );
  }
}

const isMain =
  process.argv[1] && resolve(process.argv[1]).endsWith("esp2-input-bridge.mjs");
if (isMain) {
  // Visible in Task Manager Details → Command line / Description column.
  process.title = "esp2-input-bridge";
  main().catch((err) => {
    console.error("FAIL", err.message || err);
    process.exit(1);
  });
}
