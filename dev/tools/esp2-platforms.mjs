/**
 * ESP][ hardware platform IDs + Project Control helpers.
 *
 * Project control id `platform` is the single source of truth for:
 *   - µGulp task/group visibility (µVisible)
 *   - PlatformIO environment defaults
 *   - gulp-mu-build-filter `platform` option (exact free-text match)
 *
 * Do not invent a second parallel target state.
 */
import { createRequire } from "node:module";

/** Stable internal platform IDs (exact strings for gulp-mu-build-filter). */
export const PLATFORM_WAVESHARE = "waveshare-amoled-s3";
export const PLATFORM_DIYMORE = "diymore-es3c28p";
export const PLATFORM_RLCD42 = "waveshare-rlcd42";

export const PLATFORM_DEFAULT = PLATFORM_WAVESHARE;

export const PLATFORMS = Object.freeze([
  {
    id: PLATFORM_WAVESHARE,
    label: "Waveshare AMOLED",
    tooltip:
      "Waveshare ESP32-S3-Touch-AMOLED-1.64 (CO5300 / FT3168) — production ESP][ target",
    shortName: "Waveshare",
    /** PlatformIO envs allowed when this platform is selected. */
    pioEnvs: Object.freeze(["apple2_text", "bringup", "core_smoke"]),
    defaultPioEnv: "apple2_text",
  },
  {
    id: PLATFORM_DIYMORE,
    label: "diymore 2.8\"",
    tooltip:
      'diymore ESP32-S3 2.8" Touch · AM03-FBBA0125-AAA · ES3C28P — bring-up only',
    shortName: "diymore",
    partNumber: "AM03-FBBA0125-AAA",
    productCode: "ES3C28P",
    pioEnvs: Object.freeze([
      "diymore_bringup",
      "diymore_display_probe",
      "diymore_apple2",
    ]),
    defaultPioEnv: "diymore_apple2",
  },
  {
    id: PLATFORM_RLCD42,
    label: "RLCD 4.2",
    tooltip:
      "Waveshare/Luckfox ESP32-S3-RLCD-4.2 (ST7305 reflective, no touch) — bring-up",
    shortName: "RLCD42",
    productCode: "ESP32-S3-RLCD-4.2",
    pioEnvs: Object.freeze([
      "rlcd42_bringup",
      "rlcd42_display_probe",
      "rlcd42_apple2",
    ]),
    defaultPioEnv: "rlcd42_bringup",
  },
]);

export const PLATFORM_IDS = Object.freeze(PLATFORMS.map((p) => p.id));

/**
 * µGulp Project Control declaration (id must stay `platform`).
 * Prefer declaring in microgulp.config.json; gulpfile may re-export the same.
 */
export function buildPlatformProjectControl() {
  return {
    id: "platform",
    type: "radio",
    // GUI title (not the technical id). Phrase shape matches µGulp i18x keys.
    label: 'Target<context="project control"/>',
    default: PLATFORM_DEFAULT,
    tooltip: "Hardware target for build, upload, and board-specific tasks",
    icon: "build",
    options: PLATFORMS.map((p) => ({
      value: p.id,
      label: p.label,
      tooltip: p.tooltip,
    })),
  };
}

export function platformMeta(platformId) {
  const id = String(platformId || PLATFORM_DEFAULT);
  return PLATFORMS.find((p) => p.id === id) || PLATFORMS[0];
}

export function isKnownPlatform(platformId) {
  return PLATFORM_IDS.includes(String(platformId || ""));
}

/**
 * Read project-control values (µGulp injects MICROGULP_CONTROLS for workers).
 * Polyfill when installed gulp-mu-gulp-api lacks GetProjectControls.
 * @returns {Record<string, string|boolean>}
 */
export function getProjectControls() {
  try {
    const require = createRequire(import.meta.url);
    const api = require("gulp-mu-gulp-api");
    if (typeof api.GetProjectControls === "function") {
      const v = api.GetProjectControls();
      if (v && typeof v === "object") return v;
    }
  } catch {
    /* fall through */
  }
  const raw = process.env.MICROGULP_CONTROLS;
  if (!raw) return {};
  try {
    const parsed = JSON.parse(raw);
    if (parsed && typeof parsed === "object" && !Array.isArray(parsed)) return parsed;
  } catch {
    /* ignore */
  }
  return {};
}

/**
 * Active hardware platform id from Project Control (or default).
 * @returns {string}
 */
export function resolveActivePlatform() {
  const controls = getProjectControls();
  const fromControl = String(controls.platform ?? "").trim();
  if (isKnownPlatform(fromControl)) return fromControl;
  const fromEnv = String(process.env.ESP2_PLATFORM ?? "").trim();
  if (isKnownPlatform(fromEnv)) return fromEnv;
  return PLATFORM_DEFAULT;
}

/**
 * PlatformIO envs allowed for a platform.
 * @param {string} [_platformId]
 * @returns {readonly string[]}
 */
export function pioEnvsForPlatform(_platformId) {
  return platformMeta(_platformId ?? resolveActivePlatform()).pioEnvs;
}

/**
 * Default PlatformIO env for a platform.
 * @param {string} [_platformId]
 * @returns {string}
 */
export function defaultPioEnvForPlatform(_platformId) {
  return platformMeta(_platformId ?? resolveActivePlatform()).defaultPioEnv;
}

/**
 * Options bag for gulp-mu-build-filter (exact free-text `platform` match).
 * @param {string} [_platformId]
 */
export function buildFilterPlatformOptions(_platformId) {
  const platform = _platformId ?? resolveActivePlatform();
  return {
    platform,
    release: "*",
    client: "*",
    mode: "remove",
  };
}

/** µVisible helpers */
export function visibleOnWaveshare({ controls }) {
  const id = String(controls?.platform ?? PLATFORM_DEFAULT);
  return id === PLATFORM_WAVESHARE;
}

export function visibleOnDiymore({ controls }) {
  return String(controls?.platform ?? "") === PLATFORM_DIYMORE;
}

export function visibleOnRlcd42({ controls }) {
  return String(controls?.platform ?? "") === PLATFORM_RLCD42;
}

export function visibleOnAnyEsp2Platform({ controls }) {
  const id = String(controls?.platform ?? PLATFORM_DEFAULT);
  return isKnownPlatform(id) || id === "";
}
