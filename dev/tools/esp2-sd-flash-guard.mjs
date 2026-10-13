/**
 * Host-side SD ownership helpers for µGulp flash guard + Information.
 *
 * Firmware remains the authority (`SdOwnership` / `#ESP2SDOWNER`).
 * Conceptual states map 1:1 to firmware enum names.
 */
import { spawnSync } from "node:child_process";
import { join } from "node:path";

/** @typedef {"DEVICE"|"TO_HOST"|"HOST"|"TO_DEVICE"|"ERROR"|"UNKNOWN"} SdOwnerUi */

export const SD_OWNER = Object.freeze({
  DEVICE: "DEVICE",
  TO_HOST: "TO_HOST",
  HOST: "HOST",
  TO_DEVICE: "TO_DEVICE",
  ERROR: "ERROR",
  UNKNOWN: "UNKNOWN",
});

/**
 * Map firmware `#ACK SDOWNER state=…` token → UI owner.
 * @param {string} firmwareState
 * @returns {SdOwnerUi}
 */
export function mapFirmwareOwnerState(firmwareState) {
  const s = String(firmwareState || "").trim().toUpperCase();
  switch (s) {
    case "ESP2_OWNS_SD":
    case "DEVICE":
      return SD_OWNER.DEVICE;
    case "TRANSITION_TO_USB":
    case "TO_HOST":
      return SD_OWNER.TO_HOST;
    case "USB_OWNS_SD":
    case "HOST":
      return SD_OWNER.HOST;
    case "TRANSITION_TO_ESP2":
    case "TO_DEVICE":
      return SD_OWNER.TO_DEVICE;
    case "ERROR":
      return SD_OWNER.ERROR;
    default:
      return SD_OWNER.UNKNOWN;
  }
}

/**
 * Whether flash/upload may proceed for this owner.
 * @param {SdOwnerUi} owner
 */
export function flashAllowedForOwner(owner) {
  return owner === SD_OWNER.DEVICE || owner === SD_OWNER.UNKNOWN;
}

/**
 * User-facing flash abort reason (en-US canonical key text).
 * @param {SdOwnerUi} owner
 */
export function flashBlockedMessage(owner) {
  if (owner === SD_OWNER.HOST || owner === SD_OWNER.TO_HOST) {
    return (
      "SD card is currently mounted on the computer. " +
      "Unmount it (Device → SD Card → Unmount from computer) before flashing ESP][."
    );
  }
  if (owner === SD_OWNER.TO_DEVICE) {
    return (
      "SD ownership is returning to ESP][. Wait until the card is back on the device, then flash."
    );
  }
  if (owner === SD_OWNER.ERROR) {
    return (
      "SD ownership is in an error state. Return the SD to ESP][ before flashing."
    );
  }
  return "Cannot flash while the computer owns the SD card.";
}

/**
 * Parse `#ACK SDOWNER state=ESP2_OWNS_SD msc=0 …` line.
 * @param {string} line
 * @returns {{ owner: SdOwnerUi, firmwareState: string, msc: boolean } | null}
 */
export function parseSdOwnerAck(line) {
  const text = String(line || "");
  const m = text.match(/#ACK\s+SDOWNER\s+state=([A-Za-z0-9_]+)/i);
  if (!m) return null;
  const firmwareState = m[1];
  const mscMatch = text.match(/\bmsc=([01])/i);
  return {
    owner: mapFirmwareOwnerState(firmwareState),
    firmwareState,
    msc: mscMatch ? mscMatch[1] === "1" : false,
  };
}

/**
 * Query live device ownership over CDC (best-effort).
 * Returns UNKNOWN when port missing / device unreachable — flash may proceed.
 *
 * @param {string} projectRoot
 * @param {string} port
 * @param {{ spawn?: typeof spawnSync, listenMs?: number }} [opts]
 * @returns {{ owner: SdOwnerUi, firmwareState?: string, msc?: boolean, raw?: string }}
 */
export function querySdOwner(projectRoot, port, opts = {}) {
  const p = String(port || "").trim();
  if (!p) {
    return { owner: SD_OWNER.UNKNOWN };
  }
  const spawn = opts.spawn || spawnSync;
  const listenMs = opts.listenMs ?? 4000;
  const script = join(projectRoot, "dev/tools/esp2-upload.mjs");
  const r = spawn(
    process.execPath,
    [
      script,
      "--port",
      p,
      "--sd-owner",
      "--listen-ms",
      String(listenMs),
    ],
    { encoding: "utf8", windowsHide: true, cwd: projectRoot },
  );
  const out = `${r.stdout || ""}\n${r.stderr || ""}`;
  for (const line of out.split(/\r?\n/)) {
    const parsed = parseSdOwnerAck(line);
    if (parsed) {
      return { ...parsed, raw: line };
    }
  }
  if (r.status !== 0) {
    return { owner: SD_OWNER.UNKNOWN, raw: out.trim() };
  }
  return { owner: SD_OWNER.UNKNOWN, raw: out.trim() };
}

/**
 * Guard flash/upload: abort when HOST owns SD.
 * @param {string} projectRoot
 * @param {string|null|undefined} port
 * @param {{
 *   query?: typeof querySdOwner,
 *   tryReturn?: (port: string) => boolean,
 *   log?: (msg: string) => void,
 * }} [opts]
 */
export function assertFlashSdSafe(projectRoot, port, opts = {}) {
  const query = opts.query || querySdOwner;
  const log = opts.log || (() => {});
  const p = String(port || "").trim();
  if (!p) {
    // No port yet — AskPort happens later; caller should re-check after port known.
    return { ok: true, owner: SD_OWNER.UNKNOWN };
  }
  let status = query(projectRoot, p);
  if (status.owner === SD_OWNER.HOST || status.owner === SD_OWNER.TO_HOST) {
    if (typeof opts.tryReturn === "function") {
      log("SD is on the computer — attempting safe return to ESP][ before flash…");
      const returned = opts.tryReturn(p);
      if (returned) {
        status = query(projectRoot, p);
      }
    }
  }
  if (!flashAllowedForOwner(status.owner)) {
    throw new Error(flashBlockedMessage(status.owner));
  }
  return { ok: true, owner: status.owner };
}

/**
 * Valid ownership transitions (host conceptual model).
 * @param {SdOwnerUi} from
 * @param {SdOwnerUi} to
 */
export function isValidOwnerTransition(from, to) {
  const f = from;
  const t = to;
  if (f === t) return true;
  const allowed = {
    [SD_OWNER.DEVICE]: [SD_OWNER.TO_HOST, SD_OWNER.ERROR],
    [SD_OWNER.TO_HOST]: [SD_OWNER.HOST, SD_OWNER.DEVICE, SD_OWNER.ERROR],
    [SD_OWNER.HOST]: [SD_OWNER.TO_DEVICE, SD_OWNER.ERROR],
    [SD_OWNER.TO_DEVICE]: [SD_OWNER.DEVICE, SD_OWNER.HOST, SD_OWNER.ERROR],
    [SD_OWNER.ERROR]: [SD_OWNER.DEVICE, SD_OWNER.HOST],
    [SD_OWNER.UNKNOWN]: [
      SD_OWNER.DEVICE,
      SD_OWNER.HOST,
      SD_OWNER.TO_HOST,
      SD_OWNER.TO_DEVICE,
      SD_OWNER.ERROR,
    ],
  };
  return (allowed[f] || []).includes(t);
}
