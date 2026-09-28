#!/usr/bin/env node
/**
 * ESP][ SD MSC volume detection (host-agnostic selection logic).
 *
 * Physical enumeration delegates to esp2-host-volume.mjs.
 * Identity is based on newly appeared mount roots after ENTER MSC — never
 * on nominal card capacity alone.
 */
import { existsSync, statSync } from "node:fs";
import { join, resolve } from "node:path";
import { detectMountedVolumes } from "./esp2-host-volume.mjs";

export const SD_TREE_DIR = "esp2";
export const DEFAULT_MSC_MOUNT_TIMEOUT_MS = 45000;
export const DEFAULT_MSC_POLL_MS = 1000;

/**
 * @typedef {{
 *   root: string,
 *   letter?: string,
 *   filesystem?: string,
 *   label?: string,
 *   capacityBytes?: number,
 *   freeBytes?: number,
 *   driveType?: string,
 * }} VolumeInfo
 */

/**
 * Stable identity key for before/after volume sets.
 * @param {VolumeInfo} v
 */
export function volumeIdentityKey(v) {
  const root = String(v.root || "")
    .replace(/\\/g, "/")
    .replace(/\/+$/, "")
    .toUpperCase();
  return root;
}

/**
 * @param {VolumeInfo[]} list
 * @returns {Map<string, VolumeInfo>}
 */
export function volumeMap(list) {
  const m = new Map();
  for (const v of list || []) {
    m.set(volumeIdentityKey(v), v);
  }
  return m;
}

/**
 * Volumes present in `after` but not in `before`.
 * @param {VolumeInfo[]} before
 * @param {VolumeInfo[]} after
 */
export function diffNewVolumes(before, after) {
  const prev = volumeMap(before);
  return (after || []).filter((v) => !prev.has(volumeIdentityKey(v)));
}

/**
 * True when <root>/esp2 exists as a directory.
 * @param {string} root
 */
export function hasEsp2Tree(root) {
  const abs = resolve(root);
  const esp2 = join(abs, SD_TREE_DIR);
  try {
    return existsSync(esp2) && statSync(esp2).isDirectory();
  } catch {
    return false;
  }
}

/**
 * Optional structure hint (never required as a set).
 * @param {string} root
 */
export function esp2StructureHints(root) {
  const esp2 = join(resolve(root), SD_TREE_DIR);
  return {
    hasEsp2: hasEsp2Tree(root),
    hasConfig: existsSync(join(esp2, "config")),
    hasDisks: existsSync(join(esp2, "disks")),
    hasRoms: existsSync(join(esp2, "roms")),
  };
}

/**
 * Capacity/size must never be used as sole identity.
 * @param {VolumeInfo[]} candidates
 * @param {number} [_wantedBytes]
 */
export function rejectSizeOnlyIdentity(candidates, _wantedBytes) {
  void _wantedBytes;
  // Explicit no-op guard for tests / callers: size is metadata only.
  return candidates;
}

/**
 * Classify new MSC volumes for backup (must contain esp2/) or restore
 * (empty compatible volume allowed).
 *
 * @param {{
 *   before: VolumeInfo[],
 *   after: VolumeInfo[],
 *   mode: "backup" | "restore",
 * }} opts
 * @returns {{
 *   status: "auto" | "ambiguous" | "none",
 *   candidates: VolumeInfo[],
 *   selected: VolumeInfo | null,
 *   reason?: string,
 * }}
 */
export function selectMscVolume(opts) {
  const mode = opts.mode;
  const fresh = diffNewVolumes(opts.before || [], opts.after || []);
  rejectSizeOnlyIdentity(fresh);

  /** @type {VolumeInfo[]} */
  let candidates = [];
  if (mode === "backup") {
    candidates = fresh.filter((v) => hasEsp2Tree(v.root));
    if (fresh.length > 0 && candidates.length === 0) {
      return {
        status: "none",
        candidates: [],
        selected: null,
        reason: "new_volume_without_esp2",
      };
    }
  } else {
    // Restore: any newly mounted volume is a candidate (esp2 optional).
    candidates = fresh.slice();
  }

  if (candidates.length === 0) {
    return { status: "none", candidates: [], selected: null, reason: "no_new_volume" };
  }
  if (candidates.length === 1) {
    return { status: "auto", candidates, selected: candidates[0] };
  }
  return { status: "ambiguous", candidates, selected: null, reason: "multiple_new_volumes" };
}

/**
 * Poll until a selectable volume appears or timeout.
 * @param {{
 *   before: VolumeInfo[],
 *   mode: "backup" | "restore",
 *   enumerate: () => VolumeInfo[] | Promise<VolumeInfo[]>,
 *   timeoutMs?: number,
 *   pollMs?: number,
 *   sleep?: (ms: number) => Promise<void>,
 *   now?: () => number,
 *   onStatus?: (msg: string) => void,
 * }} opts
 */
export async function waitForMscVolume(opts) {
  const timeoutMs = opts.timeoutMs ?? DEFAULT_MSC_MOUNT_TIMEOUT_MS;
  const pollMs = opts.pollMs ?? DEFAULT_MSC_POLL_MS;
  const sleep =
    opts.sleep ||
    ((ms) => new Promise((r) => setTimeout(r, ms)));
  const now = opts.now || (() => Date.now());
  const t0 = now();
  opts.onStatus?.("Waiting for ESP][ SD card...");

  /** @type {ReturnType<typeof selectMscVolume> | null} */
  let last = null;
  while (now() - t0 <= timeoutMs) {
    const after = await opts.enumerate();
    last = selectMscVolume({ before: opts.before, after, mode: opts.mode });
    if (last.status === "auto" || last.status === "ambiguous") {
      opts.onStatus?.("SD detected");
      return last;
    }
    if (last.reason === "new_volume_without_esp2") {
      // Keep waiting — wrong transient volume may appear first.
    }
    await sleep(pollMs);
  }
  return (
    last || {
      status: "none",
      candidates: [],
      selected: null,
      reason: "timeout",
    }
  );
}

/**
 * Human-readable destination summary for confirm dialogs.
 * @param {VolumeInfo} vol
 */
export function formatVolumeSummary(vol) {
  const hints = esp2StructureHints(vol.root);
  const cap =
    vol.capacityBytes != null
      ? `${(vol.capacityBytes / (1024 ** 3)).toFixed(2)} GiB`
      : "?";
  const free =
    vol.freeBytes != null ? `${(vol.freeBytes / (1024 ** 3)).toFixed(2)} GiB` : "?";
  return {
    root: vol.root,
    letter: vol.letter || "",
    filesystem: vol.filesystem || "unknown",
    label: vol.label || "",
    capacity: cap,
    free,
    hasEsp2: hints.hasEsp2,
    text: [
      `path=${vol.root}`,
      `fs=${vol.filesystem || "?"}`,
      `capacity=${cap}`,
      `free=${free}`,
      `esp2=${hints.hasEsp2 ? "yes" : "no"}`,
    ].join(" · "),
  };
}

/**
 * Enumerate mounted host volumes (Windows / macOS / Linux).
 * @returns {VolumeInfo[]}
 */
export function enumerateWindowsVolumes() {
  return detectMountedVolumes();
}
