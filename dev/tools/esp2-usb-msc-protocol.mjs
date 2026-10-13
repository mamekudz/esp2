/**
 * Host parser for firmware USB-MSC protocol lines.
 *
 * Contract: every `#ESP2USBMSC` command must yield READY, LEFT, or ERROR <CODE>
 * without relying on a long host timeout as the failure signal.
 */

export const USB_MSC_ERROR = Object.freeze({
  UNSUPPORTED: "UNSUPPORTED",
  NO_SD: "NO_SD",
  SD_INIT: "SD_INIT",
  BUSY: "BUSY",
  MSC_INIT: "MSC_INIT",
  FAILED: "FAILED",
});

/**
 * @param {string} line
 * @returns {{ ok: true, state: "READY"|"LEFT" } | { ok: false, code: string } | null}
 */
export function parseUsbMscLine(line) {
  const s = String(line || "").trim();
  if (!s.includes("#ESP2USBMSC")) return null;
  const err = s.match(/#ESP2USBMSC\s+ERROR\s+([A-Za-z0-9_]+)/i);
  if (err) {
    return { ok: false, code: String(err[1]).toUpperCase() };
  }
  if (/#ESP2USBMSC\s+READY\b/i.test(s)) {
    return { ok: true, state: "READY" };
  }
  if (/#ESP2USBMSC\s+LEFT\b/i.test(s)) {
    return { ok: true, state: "LEFT" };
  }
  return null;
}

/**
 * Language-neutral host message (logs). UI may wrap with i18x.
 * @param {string} code
 */
export function usbMscHostMessage(code) {
  const c = String(code || USB_MSC_ERROR.FAILED).toUpperCase();
  switch (c) {
    case USB_MSC_ERROR.UNSUPPORTED:
      return (
        "USB Mass Storage is not available on this firmware (hardware CDC). " +
        "The SD stays on ESP][. Computer SD access needs a TinyUSB build (USB_MODE=0)."
      );
    case USB_MSC_ERROR.NO_SD:
      return "No SD card block device detected. Insert a card; empty/unformatted cards are allowed, missing cards are not.";
    case USB_MSC_ERROR.SD_INIT:
      return "SD card was detected but block geometry/init failed. The card was not formatted by ESP][.";
    case USB_MSC_ERROR.BUSY:
      return "SD ownership is busy or already transitioning. Wait or return the card to ESP][ first.";
    case USB_MSC_ERROR.MSC_INIT:
      return "USB Mass Storage failed to start on the device.";
    default:
      return `USB Mass Storage failed (${c}).`;
  }
}

/**
 * MSC enter must not require `/esp2` or a mounted FAT volume.
 * Missing card → NO_SD. Unformatted/empty card with a block device → allowed.
 * @param {{ cardPresent: boolean, fatMounted?: boolean, hasEsp2Tree?: boolean }} state
 */
export function mscEnterAllowedForCard(state) {
  return Boolean(state && state.cardPresent);
}
