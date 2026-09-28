/**
 * Generate ESP][ device profiles from the apple2js catalog audit.
 *
 * Writes config/device/profiles/<id>/{system.json,macros.json}.
 * Does NOT download media. Does NOT overwrite hand-maintained profiles
 * listed in PROTECTED_PROFILE_IDS unless --force.
 *
 * Disk paths are metadata only — commercial images stay user-supplied /
 * LOCAL_TEST_ONLY under gitignored local/apple2/.
 */
import { existsSync, mkdirSync, writeFileSync, readFileSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "../..");
const AUDIT_PATH = join(ROOT, "docs/media/generated/apple2js-catalog-audit.json");
const PROFILES_DIR = join(ROOT, "config/device/profiles");

/** Hand-tuned demos — generator must not clobber without --force. */
export const PROTECTED_PROFILE_IDS = Object.freeze([
  "galaxian-demo",
  "little-brick-out",
  "star-blazer",
]);

/** Categories skipped entirely (not useful as boot profiles). */
const SKIP_CATEGORIES = new Set(["Blank"]);

/**
 * @param {string} title
 * @param {string} ext
 */
export function deviceDiskFileName(title, ext = ".dsk") {
  const base = String(title || "disk")
    .replace(/[^A-Za-z0-9]+/g, "")
    .replace(/^(\d)/, "T$1");
  return `${base || "Disk"}${ext.startsWith(".") ? ext : `.${ext}`}`;
}

/**
 * @param {object} entry audit entry
 */
export function profileIdFromEntry(entry) {
  return String(entry.id || entry.basename || "untitled")
    .trim()
    .toLowerCase()
    .replace(/[^a-z0-9._-]+/g, "-")
    .replace(/^-+|-+$/g, "");
}

/**
 * Presentation defaults by catalog category.
 * @param {string} category
 */
export function presentationForCategory(category) {
  const c = String(category || "");
  if (c === "Game" || c === "Demo" || c === "Music Video") {
    return {
      orientation: "landscape",
      monitor: "artifact",
      effect: "clean",
      color: "artifact",
    };
  }
  return {
    orientation: "classic",
    monitor: "white",
    effect: "clean",
    color: "sharp",
  };
}

/**
 * @param {object} entry
 */
export function buildProfileSystemJson(entry) {
  const title = entry.title || entry.id;
  const diskName = deviceDiskFileName(title, ".dsk");
  const presentation = presentationForCategory(entry.category);
  return {
    schemaVersion: 1,
    machine: {
      rom: "/esp2/roms/system.rom",
    },
    media: {
      drive1: `/esp2/disks/${diskName}`,
      drive2: null,
    },
    startup: {
      bootFromDisk: true,
      macro: "",
    },
    presentation,
    display: {
      screensaverSeconds: 300,
    },
    usb: {
      storageMode: "normal",
    },
    input: {
      hostBridge: {
        gamepad: "auto",
        gamepadId: "",
        pdl0: "gamepadX",
        pdl1: "gamepadY",
        pb0: "gamepadA",
        dial: false,
        keyboard: true,
        deadzone: 0.08,
      },
    },
    meta: {
      title,
      category: entry.category || "",
      apple2jsBasename: entry.basename || "",
      apple2jsFilename: entry.upstreamFilename || "",
      redistribution: entry.redistribution?.status || "UNKNOWN",
      source: "apple2js-catalog",
    },
  };
}

export function buildEmptyMacrosJson() {
  return {
    schemaVersion: 1,
    macros: [],
  };
}

/**
 * @param {{ force?: boolean, categories?: string[]|null, dryRun?: boolean }} [opts]
 */
export function generateProfilesFromApple2jsAudit(opts = {}) {
  if (!existsSync(AUDIT_PATH)) {
    throw new Error(
      `Missing ${AUDIT_PATH}. Run: gulp apple2js:sync && gulp apple2js:audit`,
    );
  }
  const audit = JSON.parse(readFileSync(AUDIT_PATH, "utf8"));
  const entries = Array.isArray(audit.entries) ? audit.entries : [];
  const categoryFilter = opts.categories
    ? new Set(opts.categories.map((c) => String(c)))
    : null;

  const results = {
    written: [],
    skippedProtected: [],
    skippedCategory: [],
    skippedInvalid: [],
  };

  for (const entry of entries) {
    const category = entry.category || "";
    if (SKIP_CATEGORIES.has(category)) {
      results.skippedCategory.push(entry.id || entry.basename);
      continue;
    }
    if (categoryFilter && !categoryFilter.has(category)) {
      results.skippedCategory.push(entry.id || entry.basename);
      continue;
    }
    const id = profileIdFromEntry(entry);
    if (!id) {
      results.skippedInvalid.push(entry);
      continue;
    }
    if (PROTECTED_PROFILE_IDS.includes(id) && !opts.force) {
      results.skippedProtected.push(id);
      continue;
    }
    const dir = join(PROFILES_DIR, id);
    if (!opts.dryRun) {
      mkdirSync(dir, { recursive: true });
      writeFileSync(
        join(dir, "system.json"),
        `${JSON.stringify(buildProfileSystemJson(entry), null, 2)}\n`,
        "utf8",
      );
      writeFileSync(
        join(dir, "macros.json"),
        `${JSON.stringify(buildEmptyMacrosJson(), null, 2)}\n`,
        "utf8",
      );
    }
    results.written.push(id);
  }
  return results;
}

const isMain =
  process.argv[1] &&
  fileURLToPath(import.meta.url) === join(process.argv[1]);

if (isMain) {
  const force = process.argv.includes("--force");
  const dryRun = process.argv.includes("--dry-run");
  const catIdx = process.argv.indexOf("--category");
  const categories =
    catIdx >= 0 && process.argv[catIdx + 1]
      ? process.argv[catIdx + 1].split(",").map((s) => s.trim())
      : null;
  const r = generateProfilesFromApple2jsAudit({ force, dryRun, categories });
  console.log(
    JSON.stringify(
      {
        ok: true,
        dryRun,
        written: r.written.length,
        skippedProtected: r.skippedProtected,
        sample: r.written.slice(0, 12),
      },
      null,
      2,
    ),
  );
}
