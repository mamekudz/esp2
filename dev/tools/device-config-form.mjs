/**
 * ESP][ device-config form helpers — local save / validate / explicit apply.
 * No firmware side effects. Media bytes are never uploaded here.
 */
import {
  existsSync,
  readFileSync,
  writeFileSync,
  mkdirSync,
  readdirSync,
  copyFileSync,
} from "node:fs";
import { join } from "node:path";
import { spawnSync } from "node:child_process";

export const DEVICE_CONFIG_SCHEMA = 1;
export const LOCAL_CONFIG_DIR = "local/device/config";
export const PROFILES_DIR = "config/device/profiles";

/**
 * @param {string} root
 */
export function listProfileIds(root) {
  const dir = join(root, PROFILES_DIR);
  if (!existsSync(dir)) return [];
  return readdirSync(dir, { withFileTypes: true })
    .filter((d) => d.isDirectory())
    .map((d) => d.name)
    .sort();
}

/**
 * @param {string} root
 * @param {string} profileId
 */
export function profilePaths(root, profileId) {
  const base = join(root, PROFILES_DIR, profileId);
  return {
    dir: base,
    system: join(base, "system.json"),
    macros: join(base, "macros.json"),
  };
}

/**
 * @param {string} root
 */
export function localConfigPaths(root) {
  const dir = join(root, LOCAL_CONFIG_DIR);
  return {
    dir,
    system: join(dir, "system.json"),
    macros: join(dir, "macros.json"),
  };
}

/**
 * @param {object} raw
 * @returns {{ ok: true, config: object } | { ok: false, errors: string[] }}
 */
export function normalizeSystemConfig(raw) {
  const errors = [];
  if (!raw || typeof raw !== "object") {
    return { ok: false, errors: ["config must be an object"] };
  }
  const schemaVersion = Number(raw.schemaVersion ?? raw.schema);
  if (schemaVersion !== DEVICE_CONFIG_SCHEMA) {
    errors.push(`schemaVersion must be ${DEVICE_CONFIG_SCHEMA}`);
  }
  const machine = raw.machine && typeof raw.machine === "object" ? raw.machine : {};
  const media = raw.media && typeof raw.media === "object" ? raw.media : {};
  const startup = raw.startup && typeof raw.startup === "object" ? raw.startup : {};
  const presentation =
    raw.presentation && typeof raw.presentation === "object" ? raw.presentation : {};
  const display = raw.display && typeof raw.display === "object" ? raw.display : {};

  const rom = String(machine.rom ?? "").trim();
  const drive1 = String(media.drive1 ?? "").trim();
  let drive2 = media.drive2 == null || media.drive2 === "" ? null : String(media.drive2).trim();
  const bootFromDisk = Boolean(startup.bootFromDisk);
  let macro = String(startup.macro ?? startup.startupMacro ?? "").trim();
  if (macro === "none") macro = "";
  const orientation = String(presentation.orientation ?? "classic").trim();
  let monitor = String(presentation.monitor ?? "").trim().toLowerCase();
  const legacyColor = String(presentation.color ?? "").trim().toLowerCase();
  if (!monitor) {
    if (legacyColor === "artifact" || legacyColor === "artifactcolor") monitor = "artifact";
    else if (legacyColor === "green") monitor = "green";
    else if (legacyColor === "amber") monitor = "amber";
    else monitor = "white"; // sharp / white / missing
  }
  if (monitor === "sharp" || monitor === "mono" || monitor === "monochrome") monitor = "white";
  if (monitor === "artifactcolor") monitor = "artifact";
  let effect = String(presentation.effect ?? "clean").trim().toLowerCase();
  if (effect === "crt_tv" || effect === "tv") effect = "crt";
  if (effect === "sharp" || effect === "off" || effect === "none") effect = "clean";
  const screensaverSeconds = Number(display.screensaverSeconds ?? 0);

  if (rom && !rom.startsWith("/esp2/")) errors.push("ROM path must be under /esp2/");
  if (drive1 && !drive1.startsWith("/esp2/")) errors.push("Drive 1 path must be under /esp2/");
  if (drive2 && !drive2.startsWith("/esp2/")) errors.push("Drive 2 path must be under /esp2/");
  if (orientation !== "classic" && orientation !== "landscape") {
    errors.push("orientation must be classic|landscape");
  }
  if (!["white", "green", "amber", "artifact"].includes(monitor)) {
    errors.push("monitor must be white|green|amber|artifact");
  }
  if (effect !== "clean" && effect !== "crt") {
    errors.push("effect must be clean|crt");
  }
  if (!Number.isFinite(screensaverSeconds) || screensaverSeconds < 0 || screensaverSeconds > 86400) {
    errors.push("screensaverSeconds must be 0..86400 (0 = disabled)");
  }
  if (macro && !/^[A-Za-z0-9_-]+$/.test(macro)) {
    errors.push("startup macro id is invalid");
  }

  if (errors.length) return { ok: false, errors };

  return {
    ok: true,
    config: {
      schemaVersion: DEVICE_CONFIG_SCHEMA,
      machine: { rom: rom || "/esp2/roms/system.rom" },
      media: {
        drive1: drive1 || null,
        drive2,
      },
      startup: {
        bootFromDisk,
        macro,
      },
      presentation: {
        orientation,
        monitor,
        effect,
        // Legacy alias for older tooling / docs (white→sharp).
        color: monitor === "artifact" ? "artifact" : monitor === "white" ? "sharp" : monitor,
      },
      display: {
        screensaverSeconds: Math.floor(screensaverSeconds),
      },
    },
  };
}

/**
 * @param {string} root
 * @param {string} profileId
 */
export function loadProfileSystemJson(root, profileId) {
  const p = profilePaths(root, profileId).system;
  if (!existsSync(p)) return null;
  return JSON.parse(readFileSync(p, "utf8"));
}

/**
 * @param {string} root
 */
export function loadLocalOrDemoSystemJson(root) {
  const local = localConfigPaths(root).system;
  if (existsSync(local)) {
    return JSON.parse(readFileSync(local, "utf8"));
  }
  const demo = loadProfileSystemJson(root, "galaxian-demo");
  if (demo) return demo;
  return {
    schemaVersion: 1,
    machine: { rom: "/esp2/roms/system.rom" },
    media: { drive1: null, drive2: null },
    startup: { bootFromDisk: false, macro: "" },
    presentation: { orientation: "classic", monitor: "white", effect: "clean", color: "sharp" },
    display: { screensaverSeconds: 0 },
  };
}

/**
 * @param {string} root
 * @returns {string[]}
 */
export function listMacroIds(root) {
  const ids = new Set(["none"]);
  const candidates = [
    localConfigPaths(root).macros,
    ...listProfileIds(root).map((id) => profilePaths(root, id).macros),
  ];
  for (const path of candidates) {
    if (!existsSync(path)) continue;
    try {
      const raw = JSON.parse(readFileSync(path, "utf8"));
      for (const m of raw.macros ?? []) {
        if (m?.id) ids.add(String(m.id));
      }
    } catch {
      /* ignore */
    }
  }
  return [...ids];
}

/**
 * Build editable form defaults from a named preset or current local/demo config.
 * µGulp forms cannot live-update fields when a select changes — callers should
 * show a preset step, then rebuild the edit form with these defaults.
 * @param {string} root
 * @param {string} [presetId] named profile under config/device/profiles/, or "custom"
 */
export function formDefaultsFromPreset(root, presetId = "custom") {
  const profiles = listProfileIds(root);
  const preset = String(presetId || "custom");
  let raw = null;
  if (preset && preset !== "custom") {
    raw = loadProfileSystemJson(root, preset);
  }
  if (!raw) {
    raw = loadLocalOrDemoSystemJson(root);
  }
  const n = normalizeSystemConfig(raw);
  const c = n.ok ? n.config : normalizeSystemConfig({ schemaVersion: 1 }).config;
  const usingNamed = Boolean(preset && preset !== "custom" && loadProfileSystemJson(root, preset));
  return {
    preset: usingNamed ? preset : "custom",
    profileName: usingNamed
      ? preset
      : existsSync(localConfigPaths(root).system)
        ? "local"
        : profiles.includes("galaxian-demo")
          ? "galaxian-demo"
          : "local",
    rom: c.machine.rom,
    drive1: c.media.drive1 ?? "",
    drive2: c.media.drive2 ?? "",
    bootFromDisk: c.startup.bootFromDisk,
    startupMacro: c.startup.macro || "none",
    orientation: c.presentation.orientation,
    monitor: c.presentation.monitor,
    effect: c.presentation.effect,
    /** @deprecated use monitor — kept for older form field ids */
    color: c.presentation.color,
    screensaverSeconds: c.display.screensaverSeconds,
    action: "save_local",
    port: process.env.ESP2_PORT || "",
    profiles,
    macros: listMacroIds(root),
  };
}

/**
 * Build editable form defaults from current local/demo config.
 * @param {string} root
 */
export function formDefaultsFromConfig(root) {
  const profiles = listProfileIds(root);
  const preferred =
    !existsSync(localConfigPaths(root).system) && profiles.includes("galaxian-demo")
      ? "galaxian-demo"
      : "custom";
  return formDefaultsFromPreset(root, preferred);
}

/**
 * Resolve form answers into a system.json object.
 * Field values are the source of truth after the edit step. A named preset may
 * still be forced via loadPreset=true (legacy / CLI).
 * @param {string} root
 * @param {Record<string, any>} values
 */
export function configFromFormValues(root, values) {
  const preset = String(values.preset ?? "custom");
  const loadPreset = values.loadPreset === true || values.loadPreset === "true";
  if (loadPreset && preset && preset !== "custom") {
    const base = loadProfileSystemJson(root, preset);
    if (!base) {
      return { ok: false, errors: [`unknown preset: ${preset}`] };
    }
    return normalizeSystemConfig(base);
  }
  return normalizeSystemConfig({
    schemaVersion: DEVICE_CONFIG_SCHEMA,
    machine: { rom: String(values.rom ?? "") },
    media: {
      drive1: values.drive1 === "" || values.drive1 == null ? null : String(values.drive1),
      drive2: values.drive2 === "" || values.drive2 == null ? null : String(values.drive2),
    },
    startup: {
      bootFromDisk: Boolean(values.bootFromDisk),
      macro: String(values.startupMacro ?? ""),
    },
    presentation: {
      orientation: String(values.orientation ?? "classic"),
      monitor: String(values.monitor ?? values.color ?? "white"),
      effect: String(values.effect ?? "clean"),
    },
    display: {
      screensaverSeconds: Number(values.screensaverSeconds ?? 0),
    },
  });
}

/**
 * Whether a fresh clone still needs the guided first-run setup
 * (free ROMs / title media / runtime disk / local device profile).
 * @param {string} root
 * @param {{ freeRomReady?: boolean, titleCached?: boolean, titlePrepared?: boolean }} [media]
 */
export function needsFirstRunSetup(root, media = {}) {
  const localCfg = localConfigPaths(root).system;
  if (!existsSync(localCfg) && !existsSync(join(root, PROFILES_DIR, "galaxian-demo", "system.json"))) {
    return true;
  }
  if (media.freeRomReady === false || media.titleCached === false || media.titlePrepared === false) {
    return true;
  }
  if (media.freeRomReady == null) {
    // Media not supplied — treat missing local config as first-run hint only.
    return !existsSync(localCfg);
  }
  return false;
}

/**
 * Save system.json (+ copy macros from preset/local) under local/device/config and optional profile.
 * @param {string} root
 * @param {object} config
 * @param {string} profileName
 * @param {string} [presetForMacros]
 */
export function saveConfigLocal(root, config, profileName, presetForMacros = "custom") {
  const local = localConfigPaths(root);
  mkdirSync(local.dir, { recursive: true });
  const text = `${JSON.stringify(config, null, 2)}\n`;
  writeFileSync(local.system, text, "utf8");

  // Preserve / seed macros.json (never invent Galaxian-only if missing).
  let macrosSrc = local.macros;
  if (presetForMacros && presetForMacros !== "custom") {
    const p = profilePaths(root, presetForMacros).macros;
    if (existsSync(p)) macrosSrc = p;
  } else if (!existsSync(local.macros)) {
    const demo = profilePaths(root, "galaxian-demo").macros;
    if (existsSync(demo)) macrosSrc = demo;
  }
  if (existsSync(macrosSrc) && macrosSrc !== local.macros) {
    copyFileSync(macrosSrc, local.macros);
  } else if (!existsSync(local.macros)) {
    writeFileSync(
      local.macros,
      `${JSON.stringify({ schemaVersion: 1, macros: [] }, null, 2)}\n`,
      "utf8",
    );
  }

  const name = String(profileName || "").trim();
  if (name && name !== "local") {
    const dest = profilePaths(root, name);
    mkdirSync(dest.dir, { recursive: true });
    writeFileSync(dest.system, text, "utf8");
    if (existsSync(local.macros)) {
      copyFileSync(local.macros, dest.macros);
    }
  }
  return { localSystem: local.system, localMacros: local.macros, profileName: name };
}

/**
 * Upload local system.json + macros.json only (paths). Never ROM/disk bytes.
 * @param {string} root
 * @param {string} port
 */
export function applyConfigToDevice(root, port) {
  const local = localConfigPaths(root);
  if (!existsSync(local.system) || !existsSync(local.macros)) {
    throw new Error("local config missing — Save locally before Apply");
  }
  if (!port) {
    throw new Error("Apply requires a serial port");
  }
  for (const [file, target] of [
    ["system.json", "/esp2/config/system.json"],
    ["macros.json", "/esp2/config/macros.json"],
  ]) {
    const r = spawnSync(
      process.execPath,
      [
        join(root, "dev/tools/esp2-upload.mjs"),
        "--port",
        port,
        "--file",
        join(local.dir, file),
        "--target",
        target,
      ],
      { stdio: "inherit", cwd: root },
    );
    if (r.status !== 0) {
      throw new Error(`upload failed for ${file} (exit ${r.status})`);
    }
  }
}
