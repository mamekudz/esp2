/**
 * Format recognition levels for ESP][ media tooling.
 *
 * IMPORT_RECOGNIZED  — import tool can identify/hash/copy the file
 * PARSER_SUPPORTED   — host parser can read structure (sector/track/etc.)
 * EMULATOR_SUPPORTED — Apple II core on ESP][ can use it (future)
 */

export const MEDIA_FORMAT = Object.freeze({
  Unknown: "unknown",
  Dsk: "dsk",
  Do: "do",
  Po: "po",
  Nib: "nib",
  Woz: "woz",
  TwoMg: "2mg",
  Hdv: "hdv",
});

export const SUPPORT_LEVEL = Object.freeze({
  ImportRecognized: "IMPORT_RECOGNIZED",
  ParserSupported: "PARSER_SUPPORTED",
  EmulatorSupported: "EMULATOR_SUPPORTED",
});

const DOS33 = 143360;
const NIB_35 = 35 * 0x1a00;

/** @type {Record<string, { levels: string[], notes: string }>} */
export const FORMAT_MATRIX = Object.freeze({
  [MEDIA_FORMAT.Dsk]: {
    levels: [
      SUPPORT_LEVEL.ImportRecognized,
      SUPPORT_LEVEL.ParserSupported,
    ],
    notes: "140K DOS-order floppy; emulator support planned",
  },
  [MEDIA_FORMAT.Do]: {
    levels: [
      SUPPORT_LEVEL.ImportRecognized,
      SUPPORT_LEVEL.ParserSupported,
    ],
    notes: "Alias of DSK (DOS order)",
  },
  [MEDIA_FORMAT.Po]: {
    levels: [
      SUPPORT_LEVEL.ImportRecognized,
      SUPPORT_LEVEL.ParserSupported,
    ],
    notes: "140K ProDOS-order floppy; emulator support planned",
  },
  [MEDIA_FORMAT.Nib]: {
    levels: [SUPPORT_LEVEL.ImportRecognized],
    notes: "Nibble image; parser/emulator later",
  },
  [MEDIA_FORMAT.Woz]: {
    levels: [SUPPORT_LEVEL.ImportRecognized],
    notes: "WOZ1/WOZ2; research-first for emulator",
  },
  [MEDIA_FORMAT.TwoMg]: {
    levels: [SUPPORT_LEVEL.ImportRecognized],
    notes: "2MG container; import recognizes header; full parse later",
  },
  [MEDIA_FORMAT.Hdv]: {
    levels: [SUPPORT_LEVEL.ImportRecognized],
    notes: "Hard-disk volume image; architecture only for ESP][ V1 floppies",
  },
  [MEDIA_FORMAT.Unknown]: {
    levels: [],
    notes: "Unrecognized",
  },
});

export function formatSupport(format) {
  return FORMAT_MATRIX[format] || FORMAT_MATRIX[MEDIA_FORMAT.Unknown];
}

export function hasSupportLevel(format, level) {
  return (formatSupport(format).levels || []).includes(level);
}

/**
 * @param {string} path
 * @param {Buffer} [buf]
 */
export function detectMediaFormat(path, buf) {
  const ext = String(path).toLowerCase().match(/(\.[a-z0-9]+)$/)?.[1] || "";
  if (ext === ".dsk") return MEDIA_FORMAT.Dsk;
  if (ext === ".do") return MEDIA_FORMAT.Do;
  if (ext === ".po") return MEDIA_FORMAT.Po;
  if (ext === ".nib") return MEDIA_FORMAT.Nib;
  if (ext === ".woz") return MEDIA_FORMAT.Woz;
  if (ext === ".2mg") return MEDIA_FORMAT.TwoMg;
  if (ext === ".hdv") return MEDIA_FORMAT.Hdv;

  if (buf) {
    if (buf.length >= 4) {
      const mag = buf.subarray(0, 4).toString("ascii");
      if (mag === "WOZ1" || mag === "WOZ2") return MEDIA_FORMAT.Woz;
      if (mag === "2IMG") return MEDIA_FORMAT.TwoMg;
    }
    if (buf.length === DOS33) return MEDIA_FORMAT.Dsk;
    if (buf.length === NIB_35) return MEDIA_FORMAT.Nib;
  }
  return MEDIA_FORMAT.Unknown;
}

/**
 * @param {string} format
 * @param {Buffer|Uint8Array} buf
 */
export function validateBasicMedia(format, buf) {
  const errors = [];
  if (!buf || buf.length === 0) {
    return { ok: false, errors: ["empty"] };
  }
  switch (format) {
    case MEDIA_FORMAT.Dsk:
    case MEDIA_FORMAT.Do:
    case MEDIA_FORMAT.Po:
      if (buf.length !== DOS33) {
        errors.push(`expected ${DOS33} bytes for dsk/do/po, got ${buf.length}`);
      }
      break;
    case MEDIA_FORMAT.Nib:
      if (buf.length !== NIB_35) {
        errors.push(`expected ${NIB_35} bytes for 35-track nib, got ${buf.length}`);
      }
      break;
    case MEDIA_FORMAT.Woz:
      if (buf.length < 12) errors.push("woz too short");
      else {
        const mag = Buffer.from(buf.subarray(0, 4)).toString("ascii");
        if (mag !== "WOZ1" && mag !== "WOZ2") errors.push(`bad woz magic ${mag}`);
      }
      break;
    case MEDIA_FORMAT.TwoMg:
      if (buf.length < 64) errors.push("2mg too short for header");
      else if (buf.subarray(0, 4).toString("ascii") !== "2IMG") {
        errors.push("2mg missing 2IMG magic");
      }
      break;
    case MEDIA_FORMAT.Hdv:
      if (buf.length < 512) errors.push("hdv too short");
      break;
    default:
      errors.push("unknown format");
  }
  return { ok: errors.length === 0, errors };
}

export { DOS33, NIB_35 };
