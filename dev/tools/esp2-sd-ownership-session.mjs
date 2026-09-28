#!/usr/bin/env node
/**
 * Shared ESP][ SD ownership session for:
 *   - SD card on computer (manual MSC)
 *   - SD backup
 *   - SD restore
 *
 * ONE enter/detect/leave path — no duplicated MSC logic per feature.
 */
import { existsSync } from "node:fs";
import { join } from "node:path";
import { spawnSync } from "node:child_process";
import {
  detectMountedVolumes,
  openFileManager,
  requestSafeEject,
} from "./esp2-host-volume.mjs";
import {
  DEFAULT_MSC_MOUNT_TIMEOUT_MS,
  formatVolumeSummary,
  hasEsp2Tree,
  waitForMscVolume,
} from "./esp2-sd-msc-volume.mjs";

export { hasEsp2Tree, formatVolumeSummary };

/**
 * Enter or leave MSC via existing esp2-upload CLI (#ESP2USBMSC).
 * @param {string} projectRoot
 * @param {string} port
 * @param {"enter"|"leave"} action
 * @param {{ listenMs?: number, spawn?: typeof spawnSync }} [opts]
 */
export function runUsbStorageCommand(projectRoot, port, action, opts = {}) {
  const spawn = opts.spawn || spawnSync;
  const listenMs = opts.listenMs ?? 25000;
  const flag = action === "leave" ? "--leave-usb-storage" : "--enter-usb-storage";
  const script = join(projectRoot, "dev/tools/esp2-upload.mjs");
  const r = spawn(
    process.execPath,
    [script, "--port", port, flag, "--listen-ms", String(listenMs)],
    { encoding: "utf8", windowsHide: true, cwd: projectRoot },
  );
  if (r.status !== 0) {
    const err =
      String(r.stderr || r.stdout || "").trim() || `usb-storage ${action} failed`;
    throw new Error(err);
  }
  return { ok: true, stdout: String(r.stdout || "") };
}

/**
 * Open a HOST ownership session: DEVICE→HOST via MSC, detect volume, return release().
 *
 * @param {{
 *   mode: "backup" | "restore" | "browse",
 *   port: string,
 *   projectRoot: string,
 *   log?: (msg: string, vars?: object) => void,
 *   requestAmbiguousPick?: (candidates: object[]) => Promise<string|null>,
 *   enumerate?: () => object[],
 *   enterMsc?: (port: string) => void,
 *   leaveMsc?: (port: string) => void,
 *   timeoutMs?: number,
 *   openExplorer?: boolean,
 * }} opts
 *
 * mode:
 *   backup  — require /esp2 on candidate
 *   restore — allow empty card (no /esp2)
 *   browse  — prefer /esp2 when present among new volumes; else any single new volume
 */
export async function openHostSdSession(opts) {
  const log = opts.log || (() => {});
  const port = String(opts.port || "").trim();
  if (!port) {
    throw new Error('ESP][ COM port is required for USB Storage mode.<context="task error"/>');
  }

  const enumerate = opts.enumerate || detectMountedVolumes;
  const enterMsc =
    opts.enterMsc ||
    ((p) => {
      runUsbStorageCommand(opts.projectRoot, p, "enter");
    });
  const leaveMsc =
    opts.leaveMsc ||
    ((p) => {
      runUsbStorageCommand(opts.projectRoot, p, "leave");
    });

  const selectMode = opts.mode === "browse" ? "restore" : opts.mode;

  log('Connecting to ESP][ on <port/><context="task log"/>', { port });
  log('SD owned by ESP][ — switching to computer…<context="task log"/>');
  const before = await enumerate();
  enterMsc(port);

  let left = false;
  /** @type {string | null} */
  let mountRoot = null;

  const release = async ({ skipLeave = false, tryEject = true } = {}) => {
    if (left) return;
    left = true;
    if (tryEject && mountRoot) {
      const ej = requestSafeEject(mountRoot);
      if (!ej.ok && ej.instruction) {
        log('<hint/><context="task log"/>', { hint: ej.instruction });
      }
    }
    if (skipLeave) return;
    try {
      log('Returning SD to ESP][…<context="task log"/>');
      leaveMsc(port);
      log('SD returned to ESP][.<context="task log"/>');
    } catch (e) {
      log(
        'USB storage leave failed — eject the SD in Explorer/Finder, then retry Return to ESP][: <err/><context="task log"/>',
        { err: String(e?.message || e) },
      );
    }
  };

  try {
    log('Waiting for Windows/macOS to mount the ESP][ SD…<context="task log"/>');
    let detected = await waitForMscVolume({
      before,
      mode: selectMode,
      enumerate,
      timeoutMs: opts.timeoutMs ?? DEFAULT_MSC_MOUNT_TIMEOUT_MS,
      onStatus: (msg) => log(`${msg}<context="task log"/>`),
    });

    // Browse: if restore-mode found volumes but we prefer esp2 when multiple,
    // filter when possible.
    if (opts.mode === "browse" && detected.status === "ambiguous") {
      const withEsp2 = detected.candidates.filter((c) => hasEsp2Tree(c.root));
      if (withEsp2.length === 1) {
        detected = { status: "auto", candidates: withEsp2, selected: withEsp2[0] };
      } else if (withEsp2.length > 1) {
        detected = { status: "ambiguous", candidates: withEsp2, selected: null };
      }
    }
    if (opts.mode === "browse" && detected.status === "auto" && detected.selected) {
      // ok
    }

    if (detected.status === "none") {
      await release({ tryEject: false });
      throw new Error(
        'Timed out waiting for the ESP][ SD to appear after USB Storage. MSC requires TinyUSB (USB_MODE=0) on device, or use Advanced card-reader fallback for backup/restore.<context="task error"/>',
      );
    }

    let selected = detected.selected;
    if (detected.status === "ambiguous") {
      const pick =
        (opts.requestAmbiguousPick &&
          (await opts.requestAmbiguousPick(detected.candidates))) ||
        null;
      if (!pick) {
        await release({ tryEject: false });
        throw new Error(
          'Multiple new volumes appeared — select one candidate explicitly.<context="task error"/>',
        );
      }
      selected =
        detected.candidates.find((c) => c.root === pick) ||
        detected.candidates.find(
          (c) =>
            c.root.replace(/\\/g, "/").toUpperCase() ===
            pick.replace(/\\/g, "/").toUpperCase(),
        );
      if (!selected) {
        await release({ tryEject: false });
        throw new Error('Selected volume is not among detected candidates.<context="task error"/>');
      }
    }

    if (opts.mode === "backup" && selected && !hasEsp2Tree(selected.root)) {
      await release({ tryEject: false });
      throw new Error(
        'Detected volume has no /esp2 tree — not an ESP][ SD backup source.<context="task error"/>',
      );
    }

    mountRoot = selected.root;
    const summary = formatVolumeSummary(selected);
    log('SD mounted on computer: <info/><context="task log"/>', { info: summary.text });

    if (opts.openExplorer) {
      try {
        openFileManager(mountRoot);
        log('Opened file manager.<context="task log"/>');
      } catch (e) {
        log('Could not open file manager: <err/><context="task log"/>', {
          err: String(e?.message || e),
        });
      }
    }

    return {
      mountRoot,
      port,
      via: "device_msc",
      volume: selected,
      summary,
      release,
    };
  } catch (e) {
    await release({ tryEject: false });
    throw e;
  }
}

/**
 * Advanced path: already-mounted host folder (card reader). No MSC.
 * @param {string} path
 * @param {"backup"|"restore"|"browse"} mode
 */
export function openAdvancedSdMount(path, mode) {
  const root = String(path || "").trim();
  if (!root) {
    throw new Error(
      'Advanced mode requires manually selecting a mounted SD.<context="task error"/>',
    );
  }
  if (!existsSync(root)) {
    throw new Error(`Path does not exist: ${root}`);
  }
  if (mode === "backup" && !hasEsp2Tree(root)) {
    throw new Error('Selected mount has no /esp2 tree.<context="task error"/>');
  }
  return {
    mountRoot: root,
    via: "advanced_manual",
    volume: null,
    summary: formatVolumeSummary({ root }),
    release: async () => {},
  };
}
