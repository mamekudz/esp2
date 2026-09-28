/**
 * ESP][ SD backup/restore µGulp UX helpers (forms + shared ownership session).
 *
 * Form builders return plain field descriptors — opening a form never enters
 * MSC or copies files. Device interaction starts only after submit.
 */
import { existsSync } from "node:fs";
import { join } from "node:path";
import {
  openAdvancedSdMount,
  openHostSdSession,
} from "./esp2-sd-ownership-session.mjs";

export const SD_UX_ACCESS_DEVICE = "device_msc";
export const SD_UX_ACCESS_ADVANCED = "advanced_manual";

/**
 * @param {{ defaultBackupRoot: string, defaultPort?: string }} opts
 */
export function buildSdBackupForm(opts) {
  const defaultPort = String(opts.defaultPort || "").trim();
  return {
    title: 'Back up ESP][ SD card<context="task parameter"/>',
    submitLabel: 'Start backup<context="button text"/>',
    fields: [
      {
        id: "port",
        type: "text",
        default: defaultPort,
        remember: false,
        required: false,
        label: 'ESP][ device / COM port<context="task parameter"/>',
        description:
          'Leave empty to use ESP2_PORT or the normal port picker. The SD currently in this ESP][ is backed up via USB Storage.<context="task parameter"/>',
      },
      {
        id: "destRoot",
        type: "folder",
        default: opts.defaultBackupRoot,
        remember: false,
        label: 'Backup destination<context="task parameter"/>',
        description:
          'Default: project local/sd-backups/. A new timestamped subdirectory is created. Read-only on the card.<context="task parameter"/>',
      },
      {
        id: "accessMode",
        type: "select",
        default: SD_UX_ACCESS_DEVICE,
        remember: false,
        label: 'Source<context="task parameter"/>',
        description:
          'Normal: SD card inserted in the connected ESP][. Advanced: only if the card is already mounted on the PC (card reader) or auto-detect failed.<context="task parameter"/>',
        options: [
          {
            value: SD_UX_ACCESS_DEVICE,
            label: 'ESP][ device (USB Storage)<context="task parameter"/>',
          },
          {
            value: SD_UX_ACCESS_ADVANCED,
            label: 'Advanced: manually select mounted SD<context="task parameter"/>',
          },
        ],
      },
      {
        id: "advancedSource",
        type: "folder",
        required: false,
        remember: false,
        label: 'Advanced: manually select mounted SD<context="task parameter"/>',
        description:
          'Fallback only — select a Windows mount that already contains esp2/ (e.g. card reader).<context="task parameter"/>',
        visibleWhen: { accessMode: SD_UX_ACCESS_ADVANCED },
      },
    ],
  };
}

/**
 * Initial Restore form — backup + device. Destination is detected after MSC;
 * confirmation is a second step once the volume is known.
 * @param {{
 *   defaultBackupRoot: string,
 *   defaultPort?: string,
 *   backups: Array<{ path: string, id: string, files?: number, bytes?: number }>,
 * }} opts
 */
export function buildSdRestoreForm(opts) {
  const defaultPort = String(opts.defaultPort || "").trim();
  const backups = opts.backups || [];
  const options =
    backups.length > 0
      ? backups.map((b) => ({
          value: b.path,
          label: `${b.id} · ${b.files ?? "?"} files · ${b.bytes ?? "?"} B`,
        }))
      : [{ value: "", label: "(no complete backups found — pick folder below)" }];
  return {
    title: 'Restore ESP][ SD card (destructive)<context="task parameter"/>',
    submitLabel: 'Continue<context="button text"/>',
    fields: [
      {
        id: "backup",
        type: "select",
        default: backups[0]?.path || "",
        remember: false,
        label: 'Backup<context="task parameter"/>',
        description:
          'Complete, hash-verified backups under local/sd-backups/. Or pick a folder below (NAS copy).<context="task parameter"/>',
        options,
      },
      {
        id: "backupFolder",
        type: "folder",
        default: opts.defaultBackupRoot,
        remember: false,
        label: 'Backup folder<context="task parameter"/>',
        description:
          'Select a backup directory (must contain manifest.json). Overrides the list when it points at a complete backup.<context="task parameter"/>',
      },
      {
        id: "port",
        type: "text",
        default: defaultPort,
        remember: false,
        required: false,
        label: 'ESP][ device / COM port<context="task parameter"/>',
        description:
          'Leave empty to use ESP2_PORT or the normal port picker. The card currently in this ESP][ receives the restore via USB Storage.<context="task parameter"/>',
      },
      {
        id: "accessMode",
        type: "select",
        default: SD_UX_ACCESS_DEVICE,
        remember: false,
        label: 'Destination<context="task parameter"/>',
        description:
          'Normal: SD card inserted in the connected ESP][. Advanced: only for an already-mounted card reader volume.<context="task parameter"/>',
        options: [
          {
            value: SD_UX_ACCESS_DEVICE,
            label: 'ESP][ device (USB Storage)<context="task parameter"/>',
          },
          {
            value: SD_UX_ACCESS_ADVANCED,
            label: 'Advanced: manually select mounted SD<context="task parameter"/>',
          },
        ],
      },
      {
        id: "advancedDest",
        type: "folder",
        required: false,
        remember: false,
        label: 'Advanced: manually select mounted SD<context="task parameter"/>',
        description:
          'Fallback only — select the Windows mount for the target card (esp2/ may be missing on a new card).<context="task parameter"/>',
        visibleWhen: { accessMode: SD_UX_ACCESS_ADVANCED },
      },
      {
        id: "dryRun",
        type: "boolean",
        default: false,
        remember: false,
        label: 'Dry run (no writes)<context="task parameter"/>',
      },
    ],
  };
}

/**
 * Post-detect restore confirmation (shows destination metadata).
 * @param {{ summaryText: string, backupLabel: string }} opts
 */
export function buildSdRestoreConfirmForm(opts) {
  return {
    title: 'Confirm restore to detected SD<context="task parameter"/>',
    submitLabel: 'Restore now<context="button text"/>',
    fields: [
      {
        id: "info",
        type: "text",
        default: opts.summaryText,
        remember: false,
        required: false,
        label: 'Detected destination SD (read-only)<context="task parameter"/>',
        description: `Backup: ${opts.backupLabel}`,
      },
      {
        id: "confirm",
        type: "boolean",
        default: false,
        remember: false,
        label: 'I confirm restore to this destination SD<context="task parameter"/>',
        description:
          'Writes/replaces the ESP][ /esp2 tree from the backup. Files outside esp2/ are left alone. Empty cards without esp2/ are OK.<context="task parameter"/>',
      },
    ],
  };
}

/**
 * Apply i18xRegister to every string field in a form descriptor (gulp context).
 * @param {object} form
 * @param {(s: string) => string} register
 */
export function registerFormI18x(form, register) {
  const out = {
    ...form,
    title: register(form.title),
    submitLabel: register(form.submitLabel),
    fields: (form.fields || []).map((f) => {
      const field = {
        ...f,
        label: register(f.label),
      };
      if (f.description) field.description = register(f.description);
      if (Array.isArray(f.options)) {
        field.options = f.options.map((o) => ({
          ...o,
          label: register(o.label),
        }));
      }
      return field;
    }),
  };
  return out;
}

/**
 * Resolve mount root for backup/restore after the user submits the form.
 * Uses the shared ownership session (same path as “SD card on computer”).
 *
 * @param {{
 *   mode: "backup" | "restore",
 *   accessMode: string,
 *   advancedPath?: string,
 *   port: string,
 *   projectRoot: string,
 *   log?: (msg: string, vars?: object) => void,
 *   requestAmbiguousPick?: (candidates: object[]) => Promise<string|null>,
 *   enumerate?: () => object[],
 *   enterMsc?: (port: string) => void,
 *   leaveMsc?: (port: string) => void,
 *   timeoutMs?: number,
 * }} opts
 */
export async function resolveSdMountViaUx(opts) {
  const log = opts.log || (() => {});
  const access = String(opts.accessMode || SD_UX_ACCESS_DEVICE).trim();

  if (access === SD_UX_ACCESS_ADVANCED) {
    const path = String(opts.advancedPath || "").trim();
    log('Using advanced manual SD mount <path/><context="task log"/>', { path });
    return openAdvancedSdMount(path, opts.mode);
  }

  return openHostSdSession({
    mode: opts.mode,
    port: opts.port,
    projectRoot: opts.projectRoot,
    log,
    requestAmbiguousPick: opts.requestAmbiguousPick,
    enumerate: opts.enumerate,
    enterMsc: opts.enterMsc,
    leaveMsc: opts.leaveMsc,
    timeoutMs: opts.timeoutMs,
    openExplorer: false,
  });
}

/**
 * Resolve backup dir from form values (list select or folder picker).
 * @param {{ backup?: string, backupFolder?: string }} values
 * @param {string} defaultRoot
 */
export function resolveBackupDirFromForm(values, defaultRoot) {
  void defaultRoot;
  const listed = String(values.backup || "").trim();
  const override = String(values.backupFolder || "").trim();
  if (override && existsSync(join(override, "manifest.json"))) {
    return override;
  }
  return listed;
}

/**
 * Form for “SD card on computer”.
 * @param {{ defaultPort?: string }} opts
 */
export function buildSdComputerForm(opts = {}) {
  const defaultPort = String(opts.defaultPort || "").trim();
  return {
    title: 'SD card on computer<context="task parameter"/>',
    submitLabel: 'Start<context="button text"/>',
    fields: [
      {
        id: "action",
        type: "select",
        default: "make_available",
        remember: false,
        label: 'Action<context="task parameter"/>',
        options: [
          {
            value: "make_available",
            label: 'Make available to computer<context="task parameter"/>',
          },
          {
            value: "return_esp",
            label: 'Return to ESP][<context="task parameter"/>',
          },
        ],
      },
      {
        id: "port",
        type: "text",
        default: defaultPort,
        remember: false,
        required: false,
        label: 'ESP][ device / COM port<context="task parameter"/>',
        description:
          'Leave empty to use ESP2_PORT or the normal port picker.<context="task parameter"/>',
      },
      {
        id: "openExplorer",
        type: "boolean",
        default: true,
        remember: false,
        label: 'Open in Explorer / Finder after mounting<context="task parameter"/>',
        visibleWhen: { action: "make_available" },
      },
    ],
  };
}
