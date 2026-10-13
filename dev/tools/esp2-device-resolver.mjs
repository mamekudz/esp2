/**
 * Canonical physical-device resolver for ESP][ µGulp tooling.
 *
 * COM numbers are ephemeral runtime state, never device identity.
 * Identity = current USB evidence + firmware `#ESP2IDENT` handshake.
 *
 * Selection:
 *   0 matching ESP][ → fail
 *   1 matching ESP][ → auto
 *   2+ → do not guess (requestPick)
 */

import { SerialPort } from "serialport";
import { createLineReader } from "./esp2-serial-framing.mjs";
import { platformMeta } from "./esp2-platforms.mjs";

export const ESPRESSIF_VID = "303A";
export const PRODUCT_ESP2 = "ESP][";

const IDENT_TIMEOUT_MS = 1400;

/**
 * @param {string | undefined} hex
 */
export function normalizeUsbHex(hex) {
  return String(hex || "")
    .replace(/^0x/i, "")
    .trim()
    .toUpperCase();
}

export function sameComPort(a, b) {
  return String(a || "").trim().toUpperCase() === String(b || "").trim().toUpperCase();
}

/**
 * @param {string} line
 * @returns {{ product: string, platform: string, build: string, msc: boolean, usb: string } | null}
 */
export function parseIdentAck(line) {
  const s = String(line || "").trim();
  if (!/#ACK\s+IDENT\b/i.test(s)) return null;
  const field = (name) => {
    const m = s.match(new RegExp(`\\b${name}=(\\S+)`));
    return m ? m[1] : "";
  };
  const productRaw = field("product");
  const product = productRaw.replace(/_/g, " ");
  return {
    product,
    platform: field("platform"),
    build: field("build"),
    msc: field("msc") === "1",
    usb: field("usb"),
  };
}

/**
 * @param {string} line
 */
export function parseSdInfoAck(line) {
  const s = String(line || "").trim();
  if (!/#ACK\s+SDINFO\b/i.test(s)) return null;
  const field = (name) => {
    const m = s.match(new RegExp(`\\b${name}=(\\S+)`));
    return m ? m[1] : "";
  };
  return {
    present: field("present"),
    fs: field("fs"),
    sizeMb: field("size_mb"),
    esp2: field("esp2"),
    owner: field("owner"),
    msc: field("msc"),
  };
}

/**
 * @param {{ path?: string, vendorId?: string, productId?: string, serialNumber?: string, pnpId?: string, manufacturer?: string, friendlyName?: string }} info
 */
export function classifyUsbPort(info) {
  const path = String(info.path || "").trim();
  const vid = normalizeUsbHex(info.vendorId);
  const pid = normalizeUsbHex(info.productId);
  const pnp = `${info.pnpId || ""} ${info.friendlyName || ""} ${info.manufacturer || ""}`;
  if (!path) {
    return { skip: true, reason: "no_path" };
  }
  if (/BTHENUM|Bluetooth/i.test(pnp)) {
    return { skip: true, reason: "bluetooth", path, vid, pid };
  }
  if (!vid && !/USB|VID/i.test(pnp)) {
    return { skip: true, reason: "non_usb", path, vid, pid };
  }
  return {
    skip: false,
    path,
    vid,
    pid,
    serialNumber: String(info.serialNumber || "").trim(),
    manufacturer: String(info.manufacturer || "").trim(),
    friendlyName: String(info.friendlyName || info.manufacturer || path).trim(),
    espressif: vid === ESPRESSIF_VID,
  };
}

/**
 * Filter candidates to confirmed ESP][ devices for the Project Control target.
 * @param {Array<{ port: string, ident?: object | null, usb?: object, occupied?: boolean }>} probed
 * @param {{ expectedPlatform?: string, hintPort?: string }} opts
 */
export function selectEsp2Candidates(probed, opts = {}) {
  const expected = String(opts.expectedPlatform || "").trim();
  const confirmed = probed.filter(
    (c) => c.ident && c.ident.product === PRODUCT_ESP2 && !c.occupied,
  );
  let pool = confirmed;
  if (expected) {
    const platformHits = confirmed.filter(
      (c) => !c.ident.platform || c.ident.platform === expected,
    );
    if (platformHits.length) pool = platformHits;
  }
  const hint = String(opts.hintPort || "").trim();
  if (hint) {
    const hinted = pool.find((c) => sameComPort(c.port, hint));
    if (hinted) {
      return { kind: "auto", reason: "hint_verified", devices: [hinted] };
    }
  }
  if (pool.length === 0) {
    return { kind: "none", reason: "no_esp2", devices: [] };
  }
  if (pool.length === 1) {
    return { kind: "auto", reason: "single_match", devices: pool };
  }
  return { kind: "ambiguous", reason: "multiple_esp2", devices: pool };
}

/**
 * Compact label for pickers / logs. Port is informational only.
 * @param {{ port: string, ident?: object | null, usb?: object }} device
 * @param {string} [platformShort]
 */
export function formatDeviceLabel(device, platformShort) {
  const short =
    platformShort ||
    (device.ident?.platform === "diymore-es3c28p" ? "diymore" : "Waveshare");
  const sn = device.usb?.serialNumber ? ` SN ${device.usb.serialNumber}` : "";
  const build = device.ident?.build ? ` ${device.ident.build}` : "";
  return `${short} · ${device.port}${sn}${build}`;
}

export function formatDeviceStatus(device) {
  const short =
    device.ident?.platform === "diymore-es3c28p"
      ? "diymore"
      : device.shortName || "Waveshare";
  return { device: short, port: device.port };
}

/**
 * @param {string} path
 * @param {{ openPort?: Function, timeoutMs?: number }} [opts]
 */
export async function probeEsp2Ident(path, opts = {}) {
  const timeoutMs = opts.timeoutMs ?? IDENT_TIMEOUT_MS;
  if (typeof opts.openPort === "function") {
    return opts.openPort(path);
  }
  let port;
  try {
    port = new SerialPort({ path, baudRate: 115200, autoOpen: false });
    await new Promise((resolve, reject) => {
      port.open((err) => (err ? reject(err) : resolve()));
    });
    await new Promise((resolve) => {
      port.set({ dtr: false, rts: false }, () => resolve());
    });
  } catch (e) {
    const msg = String(e?.message || e);
    if (/Access denied|EACCES|EBUSY|in use/i.test(msg)) {
      return { occupied: true, ident: null, error: msg };
    }
    return { occupied: false, ident: null, error: msg };
  }
  const reader = createLineReader(port);
  try {
    const deadline = Date.now() + timeoutMs;
    let lastPing = 0;
    while (Date.now() < deadline) {
      try {
        const line = await reader.waitLine(
          (l) => /#ACK\s+IDENT\b/i.test(l),
          Math.min(350, deadline - Date.now()),
        );
        const ident = parseIdentAck(line);
        if (ident) return { occupied: false, ident, raw: line };
      } catch {
        if (Date.now() - lastPing > 280) {
          lastPing = Date.now();
          port.write("#ESP2IDENT\n");
        }
      }
    }
    return { occupied: false, ident: null };
  } finally {
    await new Promise((resolve) => {
      port.close(() => resolve());
    });
  }
}

/**
 * @param {{ listPorts?: () => Promise<object[]> }} [opts]
 */
export async function listCurrentUsbSerialPorts(opts = {}) {
  const raw =
    typeof opts.listPorts === "function"
      ? await opts.listPorts()
      : await SerialPort.list();
  return raw.map(classifyUsbPort).filter((p) => !p.skip);
}

function toCandidate(usb, probe) {
  return {
    port: usb.path,
    usb,
    ident: probe.ident || null,
    occupied: Boolean(probe.occupied),
    error: probe.error,
    label: "",
  };
}

/**
 * Resolve a live ESP][ serial device. Never returns a cached COM as identity.
 *
 * @param {{
 *   expectedPlatform?: string,
 *   hintPort?: string,
 *   requireIdent?: boolean,
 *   allowUnidentifiedPick?: boolean,
 *   operation?: string,
 *   requestPick?: (candidates: object[]) => Promise<string|null>,
 *   listPorts?: () => Promise<object[]>,
 *   probeIdent?: (path: string) => Promise<object>,
 * }} opts
 */
export async function resolveEsp2Device(opts = {}) {
  const expectedPlatform = String(opts.expectedPlatform || "").trim();
  const hintPort = String(opts.hintPort || "").trim();
  const requireIdent = opts.requireIdent !== false;
  const meta = platformMeta(expectedPlatform || undefined);
  const usbPorts = await listCurrentUsbSerialPorts({ listPorts: opts.listPorts });
  if (usbPorts.length === 0) {
    throw new Error(
      'No compatible ESP][ device detected.<context="task error"/>',
    );
  }

  const probeIdent = opts.probeIdent || ((path) => probeEsp2Ident(path));
  const probed = [];
  for (const usb of usbPorts) {
    const probe = await probeIdent(usb.path);
    const cand = toCandidate(usb, probe || {});
    cand.shortName = meta.shortName;
    cand.label = formatDeviceLabel(cand, meta.shortName);
    probed.push(cand);
  }

  const selected = selectEsp2Candidates(probed, { expectedPlatform, hintPort });
  if (selected.kind === "auto") {
    return decorateResolved(selected.devices[0], {
      source: selected.reason,
      expectedPlatform,
    });
  }
  if (selected.kind === "ambiguous") {
    const picked = await pickFrom(opts.requestPick, selected.devices);
    const match = selected.devices.find((d) => sameComPort(d.port, picked));
    if (!match) {
      throw new Error(
        'Multiple ESP][ devices connected — select one. Do not guess.<context="task error"/>',
      );
    }
    return decorateResolved(match, { source: "user_pick", expectedPlatform });
  }

  if (requireIdent && !opts.allowUnidentifiedPick) {
    throw new Error(
      'No compatible ESP][ device detected.<context="task error"/>',
    );
  }

  const unidentified = probed.filter((c) => c.usb?.espressif && !c.occupied);
  if (unidentified.length === 1 && opts.allowUnidentifiedPick && !opts.requestPick) {
    throw new Error(
      'Unidentified Espressif USB device — not confirmed as ESP][. Select explicitly.<context="task error"/>',
    );
  }
  if (unidentified.length >= 1 && typeof opts.requestPick === "function") {
    const labeled = unidentified.map((c) => ({
      ...c,
      label: `Unidentified Espressif USB · ${c.port} (not confirmed ESP][)`,
    }));
    const picked = await pickFrom(opts.requestPick, labeled);
    const match = labeled.find((d) => sameComPort(d.port, picked));
    if (!match) {
      throw new Error(
        'No compatible ESP][ device detected.<context="task error"/>',
      );
    }
    return decorateResolved(match, {
      source: "unidentified_pick",
      expectedPlatform,
    });
  }

  throw new Error(
    'No compatible ESP][ device detected.<context="task error"/>',
  );
}

function decorateResolved(device, extra) {
  return {
    ...device,
    shortName: device.shortName || extra.shortName || "Waveshare",
    identity: {
      product: device.ident?.product || "",
      platform: device.ident?.platform || extra.expectedPlatform || "",
      build: device.ident?.build || "",
      vid: device.usb?.vid || "",
      pid: device.usb?.pid || "",
      serialNumber: device.usb?.serialNumber || "",
    },
    source: extra.source,
  };
}

async function pickFrom(requestPick, devices) {
  if (typeof requestPick !== "function") {
    throw new Error(
      'Multiple ESP][ devices connected — select one. Do not guess.<context="task error"/>',
    );
  }
  return String((await requestPick(devices)) || "").trim();
}

/**
 * After flash / reset / CDC↔MSC, re-enumerate. Do not reuse the old COM number.
 *
 * @param {{
 *   previous?: { identity?: object, port?: string },
 *   timeoutMs?: number,
 *   sleep?: (ms: number) => Promise<void>,
 * } & Parameters<typeof resolveEsp2Device>[0]} opts
 */
export async function reResolveEsp2DeviceAfterTransition(opts = {}) {
  const timeoutMs = opts.timeoutMs ?? 20000;
  const sleep =
    opts.sleep || ((ms) => new Promise((r) => setTimeout(r, ms)));
  const deadline = Date.now() + timeoutMs;
  let lastErr = null;
  while (Date.now() < deadline) {
    try {
      const prevSn = String(opts.previous?.identity?.serialNumber || "").trim();
      const resolved = await resolveEsp2Device({
        ...opts,
        hintPort: "",
      });
      if (prevSn && resolved.identity?.serialNumber && resolved.identity.serialNumber !== prevSn) {
        lastErr = new Error("USB serial number mismatch after re-enumeration");
        await sleep(400);
        continue;
      }
      return resolved;
    } catch (e) {
      lastErr = e;
      await sleep(500);
    }
  }
  throw lastErr || new Error(
    'No compatible ESP][ device detected after USB re-enumeration.<context="task error"/>',
  );
}

/**
 * Env/form COM is a hint only — never trusted without current USB + identity.
 */
export function hintPortFromEnvAndForm({ formPort, envPort } = {}) {
  const form = String(formPort || "").trim();
  if (form) return form;
  return String(envPort || "").trim();
}
