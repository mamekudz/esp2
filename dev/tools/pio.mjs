// ===========================================
// pio.mjs — PlatformIO helpers for ESP][ µGulp
// Pattern aligned with extended-watchy-starfield gulpfile
// ===========================================

import { spawn } from "node:child_process";
import { existsSync } from "node:fs";
import { join } from "node:path";
import { homedir } from "node:os";
import { Log, Warn, RequestForm } from "gulp-mu-gulp-api";

/** Production Apple II firmware env on Waveshare (bringup remains for HW diagnostics). */
export const PIO_ENV_DEFAULT = "apple2_text";

/** All known envs declared in platformio.ini (keep in sync when adding envs). */
export const PIO_ENVS = Object.freeze([
  "apple2_text",
  "bringup",
  "core_smoke",
  "diymore_bringup",
  "diymore_display_probe",
  "diymore_apple2",
]);

/**
 * Resolve active PlatformIO env.
 * Priority: explicit arg → ESP2_PIO_ENV → platform default → production default.
 * Invalid names throw — never silently fall back to an unintended env.
 * @param {string} [_override]
 * @param {{ platformId?: string, allowedEnvs?: readonly string[] }} [_opts]
 * @returns {string}
 */
export function ResolvePioEnv(_override, _opts = {}) {
  const allowed = _opts.allowedEnvs
    ? [..._opts.allowedEnvs]
    : [...PIO_ENVS];
  const fromArg = String(_override ?? "").trim();
  if (fromArg) {
    if (!allowed.includes(fromArg)) {
      throw new Error(
        `Unknown PlatformIO env '${fromArg}' for this target. Valid: ${allowed.join(", ")}`,
      );
    }
    return fromArg;
  }
  const fromEnv = String(process.env.ESP2_PIO_ENV ?? "").trim();
  if (fromEnv) {
    if (!allowed.includes(fromEnv)) {
      throw new Error(
        `Unknown ESP2_PIO_ENV='${fromEnv}' for this target. Valid: ${allowed.join(", ")}`,
      );
    }
    return fromEnv;
  }
  if (_opts.defaultEnv && allowed.includes(String(_opts.defaultEnv))) {
    return String(_opts.defaultEnv);
  }
  return PIO_ENV_DEFAULT;
}

/** @deprecated use ResolvePioEnv() — kept for callers that import PIO_ENV */
export const PIO_ENV = PIO_ENV_DEFAULT;
export const MONITOR_BAUD = 115200;

/**
 * Build a structured task Error with enumerable message/code/detail.
 * Complements µGulp diagnosis; primary footer formatting is handled upstream
 * by microGulp RunLogger.FormatErrorText.
 *
 * @param {string} message
 * @param {{ cause?: unknown, code?: number|null, detail?: string }} [extra]
 * @returns {Error}
 */
export function makeTaskError(message, extra = {}) {
  const msg = String(message || "unknown error").trim() || "unknown error";
  const err = new Error(msg);
  err.name = "Error";
  // Enumerable so JSON.stringify / IPC keep the useful text.
  Object.defineProperty(err, "message", {
    value: msg,
    enumerable: true,
    writable: true,
    configurable: true,
  });
  if (extra.code != null) {
    Object.defineProperty(err, "code", {
      value: extra.code,
      enumerable: true,
      writable: true,
      configurable: true,
    });
  }
  if (extra.detail) {
    Object.defineProperty(err, "detail", {
      value: String(extra.detail),
      enumerable: true,
      writable: true,
      configurable: true,
    });
  }
  if (extra.cause !== undefined) {
    try {
      err.cause = extra.cause;
    } catch {
      /* ignore */
    }
  }
  err.toJSON = () => ({
    name: err.name,
    message: err.message,
    ...(err.code != null ? { code: err.code } : {}),
    ...(err.detail ? { detail: err.detail } : {}),
  });
  err[Symbol.toPrimitive] = (hint) =>
    hint === "number" ? Number.NaN : err.message;
  return err;
}

/**
 * Human-readable text from any thrown value (Error, plain object, string).
 * @param {unknown} err
 * @returns {string}
 */
export function formatThrownError(err) {
  if (err == null) return "unknown error";
  if (typeof err === "string") return err.trim() || "unknown error";
  if (err instanceof Error) {
    const parts = [err.message || err.name || "Error"];
    if (err.detail) parts.push(String(err.detail));
    return parts.filter(Boolean).join(" — ");
  }
  if (typeof err === "object") {
    const o = /** @type {Record<string, unknown>} */ (err);
    if (typeof o.message === "string" && o.message.trim()) return o.message.trim();
    if (typeof o.detail === "string" && o.detail.trim()) return o.detail.trim();
    if (typeof o.error === "string" && o.error.trim()) return o.error.trim();
    try {
      const json = JSON.stringify(o);
      if (json && json !== "{}") return json;
    } catch {
      /* ignore */
    }
  }
  const s = String(err);
  return s === "[object Object]" ? "unknown error (non-enumerable object)" : s;
}

/**
 * Pull the first useful compiler/linker line from PlatformIO output.
 * @param {string} text
 * @returns {string}
 */
export function firstPioFailureLine(text) {
  const lines = String(text || "").split(/\r?\n/);
  const fatal = lines.find((l) => /fatal error:/i.test(l));
  if (fatal) return fatal.trim();
  const locked = lines.find(
    (l) =>
      /cannot access the file|von einem anderen Prozess verwendet|Sharing violation|Access is denied/i.test(
        l,
      ),
  );
  if (locked) return locked.trim().replace(/\s+/g, " ");
  const error = lines.find((l) => /\berror:/i.test(l) && !/warning:/i.test(l));
  if (error) return error.trim();
  const failed = lines.find((l) => /\[FAILED\]/i.test(l));
  if (failed) return failed.trim();
  return "";
}

/**
 * UTF-8-safe env for PlatformIO / esptool progress bars on Windows cp1252.
 * @returns {NodeJS.ProcessEnv}
 */
function pioChildEnv() {
  return {
    ...process.env,
    PYTHONUTF8: "1",
    PYTHONIOENCODING: "utf-8",
  };
}

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
 * @param {{ cwd: string, label?: string, onProgress?: (p: { fraction: number, label?: string, raw?: string }) => void }} _opts
 */
export function RunCommand(_command, _args, _opts) {
  const label = _opts.label ?? `${_command} ${_args.join(" ")}`;
  const onProgress = typeof _opts.onProgress === "function" ? _opts.onProgress : null;
  return new Promise((_resolve, _reject) => {
    Log('Running <cmd/><context="task log"/>', { cmd: label });
    const child = spawn(_command, _args, {
      cwd: _opts.cwd,
      // Always pipe so we can attach the first compiler error to the thrown Error
      // (µGulp footer previously showed only "[object Object]" / bare exit code).
      stdio: ["ignore", "pipe", "pipe"],
      windowsHide: true,
      env: pioChildEnv(),
    });
    /** @type {string} */
    let carry = "";
    /** @type {string} */
    let captured = "";
    const MAX_CAPTURE = 256 * 1024;
    const feed = (chunk, stream) => {
      const text = chunk.toString();
      stream.write(text);
      if (captured.length < MAX_CAPTURE) {
        captured += text.slice(0, MAX_CAPTURE - captured.length);
      }
      if (!onProgress) return;
      carry += text;
      const parts = carry.split(/\r?\n/);
      carry = parts.pop() ?? "";
      for (const line of parts) {
        const parsed = parsePioUploadProgress(line);
        if (parsed != null) {
          onProgress({ fraction: parsed, label: "upload", raw: line.trim() });
        }
      }
      // esptool often updates the same line with CR only
      const crParts = text.split(/\r/);
      for (const part of crParts) {
        const parsed = parsePioUploadProgress(part);
        if (parsed != null) {
          onProgress({ fraction: parsed, label: "upload", raw: part.trim() });
        }
      }
    };
    child.stdout?.on("data", (c) => feed(c, process.stdout));
    child.stderr?.on("data", (c) => feed(c, process.stderr));
    child.on("error", (spawnErr) => {
      _reject(
        makeTaskError(`${label} spawn failed: ${formatThrownError(spawnErr)}`, {
          cause: spawnErr,
        }),
      );
    });
    child.on("close", (_code) => {
      if (_code === 0) {
        _resolve();
        return;
      }
      const first = firstPioFailureLine(captured);
      const detail = first || undefined;
      const msg = detail
        ? `${label} failed with exit code ${_code}: ${detail}`
        : `${label} failed with exit code ${_code}`;
      Warn(msg);
      _reject(makeTaskError(msg, { code: _code, detail }));
    });
  });
}

/**
 * Parse esptool / PlatformIO upload percentage from a console line.
 * Returns 0..1 or null if the line has no progress.
 * @param {string} line
 * @returns {number|null}
 */
export function parsePioUploadProgress(line) {
  const s = String(line || "");
  // esptool v4/v5: "Writing at 0x00010000... (42 %)" or "... 42.5%"
  let m = s.match(/Writing at\s+0x[0-9a-fA-F]+[^\d%]*\(?\s*(\d+(?:\.\d+)?)\s*%\)?/i);
  if (m) {
    const pct = Number(m[1]);
    if (Number.isFinite(pct)) return Math.min(1, Math.max(0, pct / 100));
  }
  // Bracket form: "[==============================] 100.0%"
  m = s.match(/\]\s*(\d+(?:\.\d+)?)\s*%/);
  if (m && /Writing at|Hash of data verified|Compressed/i.test(s)) {
    const pct = Number(m[1]);
    if (Number.isFinite(pct)) return Math.min(1, Math.max(0, pct / 100));
  }
  // Generic "Progress 50%" / "Upload 50%"
  m = s.match(/\b(?:Progress|Uploading|Upload)\b[^\d%]*(\d+(?:\.\d+)?)\s*%/i);
  if (m) {
    const pct = Number(m[1]);
    if (Number.isFinite(pct)) return Math.min(1, Math.max(0, pct / 100));
  }
  return null;
}

/**
 * @param {string} _cwd
 * @param {string[]} _args
 * @param {string} [_label]
 * @param {{ onProgress?: (p: { fraction: number, label?: string, raw?: string }) => void }} [_extra]
 */
export function Pio(_cwd, _args, _label, _extra = {}) {
  return RunCommand(PioExecutable(), _args, {
    cwd: _cwd,
    label: _label ?? `pio ${_args.join(" ")}`,
    onProgress: _extra.onProgress,
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
      env: pioChildEnv(),
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
 * Lists current USB serial ports for a picker. Does not skip to ESP2_PORT.
 * ESP][ flash/monitor/SD use `esp2-device-resolver.mjs` instead.
 * @param {string} _cwd
 * @param {string} [_title]
 * @returns {Promise<string|null>}
 */
export async function AskPort(_cwd, _title = "ESP][ serial port") {
  // ESP2_PORT is only a form default if that port exists *now*.
  // It is not device identity and must not skip enumeration.
  const fromEnv =
    String(process.env.ESP2_PORT ?? process.env.PLATFORMIO_UPLOAD_PORT ?? "").trim();

  const ports = await ListPorts(_cwd);
  if (ports.length === 0) {
    Warn(
      'No serial ports found — PlatformIO will use platformio.ini / auto-detect.<context="task log"/>'
    );
    return null;
  }

  const envListed = fromEnv && ports.some((p) => p.value === fromEnv);
  const preferred = envListed ? fromEnv : ports[0].value;
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
