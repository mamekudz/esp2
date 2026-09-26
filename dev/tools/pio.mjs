// ===========================================
// pio.mjs — PlatformIO helpers for ESP][ µGulp
// Pattern aligned with extended-watchy-starfield gulpfile
// ===========================================

import { spawn } from "node:child_process";
import { existsSync } from "node:fs";
import { join } from "node:path";
import { homedir } from "node:os";
import { Log, Warn, RequestForm } from "gulp-mu-gulp-api";

/** PlatformIO env from platformio.ini / ESP2_PIO_ENV (do not invent board config). */
export const PIO_ENV_DEFAULT = "bringup";

/** Known envs declared in platformio.ini (keep in sync when adding envs). */
export const PIO_ENVS = Object.freeze(["bringup", "core_smoke", "apple2_text"]);

/**
 * Resolve active PlatformIO env.
 * Priority: explicit arg → ESP2_PIO_ENV → default_envs (bringup).
 * @param {string} [_override]
 */
export function ResolvePioEnv(_override) {
  const fromArg = String(_override ?? "").trim();
  if (fromArg) return fromArg;
  const fromEnv = String(process.env.ESP2_PIO_ENV ?? "").trim();
  if (fromEnv) return fromEnv;
  return PIO_ENV_DEFAULT;
}

/** @deprecated use ResolvePioEnv() — kept for callers that import PIO_ENV */
export const PIO_ENV = PIO_ENV_DEFAULT;
export const MONITOR_BAUD = 115200;


/**
 * @returns {string}
 */
export function PioExecutable() {
  const fromEnv = String(process.env.PIO_PATH ?? "").trim();
  if (fromEnv) return fromEnv;
  const home = homedir();
  const candidates = [
    join(home, ".platformio", "penv", "Scripts", "pio.exe"),
    join(home, ".platformio", "penv", "Scripts", "platformio.exe"),
    join(home, ".platformio", "penv", "bin", "pio"),
  ];
  for (const c of candidates) {
    if (existsSync(c)) return c;
  }
  return "pio";
}

/**
 * @param {string} _command
 * @param {string[]} _args
 * @param {{ cwd: string, label?: string }} _opts
 */
export function RunCommand(_command, _args, _opts) {
  const label = _opts.label ?? `${_command} ${_args.join(" ")}`;
  return new Promise((_resolve, _reject) => {
    Log('Running <cmd/><context="task log"/>', { cmd: label });
    const child = spawn(_command, _args, {
      cwd: _opts.cwd,
      stdio: "inherit",
      windowsHide: true,
      env: process.env,
    });
    child.on("error", _reject);
    child.on("close", (_code) => {
      if (_code === 0) _resolve();
      else _reject(new Error(`${label} failed with exit code ${_code}`));
    });
  });
}

/**
 * @param {string} _cwd
 * @param {string[]} _args
 * @param {string} [_label]
 */
export function Pio(_cwd, _args, _label) {
  return RunCommand(PioExecutable(), _args, {
    cwd: _cwd,
    label: _label ?? `pio ${_args.join(" ")}`,
  });
}

/**
 * @param {string} _cwd
 * @returns {Promise<{ value: string, label: string }[]>}
 */
export function ListPorts(_cwd) {
  return new Promise((_resolve) => {
    const child = spawn(PioExecutable(), ["device", "list"], {
      cwd: _cwd,
      windowsHide: true,
      stdio: ["ignore", "pipe", "pipe"],
    });
    let out = "";
    child.stdout.on("data", (chunk) => {
      out += chunk.toString();
    });
    child.stderr.on("data", (chunk) => {
      out += chunk.toString();
    });
    child.on("close", () => {
      /** @type {{ id: string, label: string, usb: boolean }[]} */
      const ports = [];
      const blocks = out.split(/\r?\n\r?\n/);
      for (const block of blocks) {
        const portMatch = block.match(/^(COM\d+|\/dev\/tty\S+)/m);
        if (!portMatch) continue;
        const id = portMatch[1];
        const desc =
          block.match(/^Description:\s*(.+)$/m)?.[1]?.trim() ?? id;
        const hwid = block.match(/^Hardware ID:\s*(.+)$/m)?.[1] ?? "";
        const usb =
          /USB|VID:PID|CH9102|CP210|FTDI|Silicon|303A/i.test(hwid + desc) &&
          !/Bluetooth|BTHENUM/i.test(hwid + desc);
        ports.push({
          id,
          label: usb ? `${id} — ${desc}` : `${id} — ${desc}`,
          usb,
        });
      }
      ports.sort((a, b) => Number(b.usb) - Number(a.usb));
      _resolve(ports.map((p) => ({ value: p.id, label: p.label })));
    });
    child.on("error", () => _resolve([]));
  });
}

/**
 * Prefer ESP2_PORT / PLATFORMIO_UPLOAD_PORT, else form, else ini default hint.
 * @param {string} _cwd
 * @param {string} [_title]
 * @returns {Promise<string|null>}
 */
export async function AskPort(_cwd, _title = "ESP][ serial port") {
  const fromEnv =
    String(process.env.ESP2_PORT ?? process.env.PLATFORMIO_UPLOAD_PORT ?? "").trim();
  if (fromEnv) {
    Log('Using port from env: <port/><context="task log"/>', { port: fromEnv });
    return fromEnv;
  }

  const ports = await ListPorts(_cwd);
  if (ports.length === 0) {
    Warn(
      'No serial ports found — PlatformIO will use platformio.ini / auto-detect.<context="task log"/>'
    );
    return null;
  }

  const preferred = ports[0].value;
  const values = await RequestForm({
    title: _title,
    fields: [
      {
        id: "port",
        type: "select",
        label: "Serial port",
        default: preferred,
        options: [
          ...ports,
          { value: "auto", label: "Auto-detect (PlatformIO / ini)" },
        ],
      },
    ],
  });

  const chosen = String(values?.port ?? preferred).trim();
  if (chosen === "auto") return null;
  return chosen;
}

/**
 * @param {string|null} _port
 * @param {string} [_env]
 */
export function UploadArgs(_port, _env) {
  const env = ResolvePioEnv(_env);
  const args = ["run", "-e", env, "-t", "upload"];
  if (_port) args.push("--upload-port", _port);
  return args;
}

/**
 * @param {string|null} _port
 */
export function MonitorArgs(_port) {
  const args = ["device", "monitor", "-b", String(MONITOR_BAUD), "--filter", "time"];
  if (_port) args.push("--port", _port);
  return args;
}
