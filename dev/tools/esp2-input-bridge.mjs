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
 * Composition example: PDL0←dial, PDL1←gamepad Y, PB0←dial press|A, PB1←B
 *
 * Not the final controller architecture (see docs/architecture/input-providers.md).
 */

import { spawn } from "node:child_process";
import { createInterface } from "node:readline";
import { resolve, dirname } from "node:path";
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
    gamepad: null,
    gamepadId: "",
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
    /** pb0: dialPress | gamepadA | or */
    pb0: "auto",
    help: false,
  };
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    const next = () => argv[++i];
    if (a === "--port") out.port = next();
    else if (a === "--gamepad") out.gamepad = Number(next());
    else if (a === "--gamepad-id") out.gamepadId = String(next());
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

/**
 * Enumerate XInput slots 0..3 via PowerShell (no extra npm native deps).
 * @returns {Promise<Array<{index:number,name:string,axes:number,buttons:number,connected:boolean}>>}
 */
export async function listXInputGamepads() {
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
for ($i=0; $i -lt 4; $i++) {
  $st = New-Object Esp2XInput+XINPUT_STATE
  $r = [Esp2XInput]::XInputGetState($i, [ref]$st)
  if ($r -eq 0) {
    Write-Output ("{0}|XInput#{0}|6|16|1" -f $i)
  } else {
    Write-Output ("{0}|XInput#{0}|0|0|0" -f $i)
  }
}
`;
  return new Promise((resolveP, reject) => {
    const child = spawn(
      "powershell.exe",
      ["-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", ps],
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
    child.on("close", (code) => {
      if (code !== 0 && !out) {
        reject(new Error(err || `powershell exit ${code}`));
        return;
      }
      const pads = out
        .split(/\r?\n/)
        .map((l) => l.trim())
        .filter(Boolean)
        .map((l) => {
          const [index, name, axes, buttons, connected] = l.split("|");
          return {
            index: Number(index),
            name: name || `XInput#${index}`,
            id: name || `XInput#${index}`,
            axes: Number(axes),
            buttons: Number(buttons),
            connected: connected === "1",
          };
        });
      resolveP(pads);
    });
  });
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

function selectGamepad(pads, args) {
  const connected = pads.filter((p) => p.connected);
  if (args.gamepadId) {
    const re = new RegExp(args.gamepadId, "i");
    const hit = connected.find((p) => re.test(p.name) || re.test(p.id));
    if (!hit) {
      throw new Error(`no connected gamepad matching --gamepad-id ${args.gamepadId}`);
    }
    return hit;
  }
  if (args.gamepad != null && Number.isFinite(args.gamepad)) {
    const hit = pads.find((p) => p.index === args.gamepad);
    if (!hit || !hit.connected) {
      throw new Error(`gamepad index ${args.gamepad} not connected`);
    }
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
 * Raw Input mouse-wheel → relative dial deltas (Logitech Dial when mapped as wheel).
 * Emits lines: "delta <n>" and optional "press 1|0" if middle-button used as press.
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
  private const uint RIDEV_INPUTSINK = 0x00000100;
  [StructLayout(LayoutKind.Sequential)]
  struct RAWINPUTDEVICE { public ushort usUsagePage; public ushort usUsage; public uint dwFlags; public IntPtr hwndTarget; }
  [StructLayout(LayoutKind.Sequential)]
  struct RAWINPUTHEADER { public uint dwType; public uint dwSize; public IntPtr hDevice; public IntPtr wParam; }
  [StructLayout(LayoutKind.Sequential)]
  struct RAWMOUSE {
    public ushort usFlags; public ushort usButtonFlags; public ushort usButtonData;
    public uint ulRawButtons; public int lLastX; public int lLastY; public uint ulExtraInformation;
  }
  [DllImport("user32.dll")] static extern bool RegisterRawInputDevices(RAWINPUTDEVICE[] p, uint n, uint size);
  [DllImport("user32.dll")] static extern uint GetRawInputData(IntPtr h, uint ui, IntPtr p, ref uint pcb, uint cbh);
  public Esp2DialForm() {
    ShowInTaskbar = false; Opacity = 0; Width = 0; Height = 0;
    var rid = new RAWINPUTDEVICE[1];
    rid[0].usUsagePage = 0x01; rid[0].usUsage = 0x02;
    rid[0].dwFlags = RIDEV_INPUTSINK; rid[0].hwndTarget = this.Handle;
    RegisterRawInputDevices(rid, 1, (uint)Marshal.SizeOf(typeof(RAWINPUTDEVICE)));
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
            if ((mouse.usButtonFlags & 0x0400) != 0) { // RI_MOUSE_WHEEL
              short delta = unchecked((short)mouse.usButtonData);
              int notches = delta / 120;
              if (notches != 0) Console.WriteLine("delta " + notches);
            }
            if ((mouse.usButtonFlags & 0x0010) != 0) Console.WriteLine("press 1"); // middle down
            if ((mouse.usButtonFlags & 0x0020) != 0) Console.WriteLine("press 0"); // middle up
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

async function main() {
  const args = parseArgs(process.argv.slice(2));
  if (args.help) {
    console.log(`Usage:
  node dev/tools/esp2-input-bridge.mjs --port COM5 [--list-gamepads] [--list-dials]
  node dev/tools/esp2-input-bridge.mjs --port COM5 --diag
  node dev/tools/esp2-input-bridge.mjs --port COM5 --dial-diag
  node dev/tools/esp2-input-bridge.mjs --port COM5 --gamepad <index>|--gamepad-id <pattern>
  node dev/tools/esp2-input-bridge.mjs --port COM5 --dial [--dial-sensitivity ${DEFAULT_DIAL_SENSITIVITY}]
  node dev/tools/esp2-input-bridge.mjs --port COM5 --dial --gamepad-id 8BitDo

Composition:
  --pdl0 dial|gamepadX   --pdl1 gamepadY|none   --pb0 dialPress|gamepadA|or
Defaults:
  deadzone=${DEFAULT_DEADZONE}; dial sensitivity=${DEFAULT_DIAL_SENSITIVITY} (no wrap; clamp 0..255)
  dial API: Windows Raw Input mouse wheel (Logitech Dial when Options maps dial→wheel)
  gamepad: left stick→PDL, A→PB0 B→PB1 X→PB2`);
    process.exit(0);
  }

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
  const pb0Src =
    args.pb0 !== "auto"
      ? args.pb0
      : useDial && selected
        ? "or"
        : useDial
          ? "dialPress"
          : selected
            ? "gamepadA"
            : "none";

  if (args.gamepad != null || args.gamepadId) {
    if (!selected) throw new Error("gamepad selection failed");
    console.log(`selected gamepad index=${selected.index} name=${selected.name}`);
  }
  console.log(
    `composition: PDL0←${pdl0Src} PDL1←${pdl1Src} PB0←${pb0Src} PB1←gamepadB dial=${useDial} sensitivity=${args.dialSensitivity}`,
  );
  if (useDial) {
    console.log(
      `dial: provider=windows_rawinput_mouse_wheel initial=${PADDLE_CENTER} clamp=0..255 press=middle_button_if_mapped`,
    );
  }

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
  main().catch((err) => {
    console.error("FAIL", err.message || err);
    process.exit(1);
  });
}
