/**
 * Load ESP][ Apple II host machine configuration.
 * Local file config/apple2.local.json is gitignored; example is committed.
 */
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { createHash } from "node:crypto";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../..");

export const DEFAULT_CONFIG = {
  schemaVersion: 1,
  machine: "AppleIIPlus",
  rom: null,
  slot6Rom: null,
  slot6Mode: "none",
  disk1: null,
  disk2: null,
  videoMode: "CompositeColor",
  displayEffect: "Sharp",
  inputProfile: "default",
  audio: "host",
};

export function projectRoot() {
  return root;
}

export function loadMachineConfig(configPath) {
  const resolved =
    configPath ||
    path.join(root, "config/apple2.local.json");
  const example = path.join(root, "config/apple2.local.example.json");
  let base = { ...DEFAULT_CONFIG };
  if (fs.existsSync(example)) {
    base = { ...base, ...JSON.parse(fs.readFileSync(example, "utf8")) };
  }
  if (fs.existsSync(resolved)) {
    base = { ...base, ...JSON.parse(fs.readFileSync(resolved, "utf8")), _configPath: resolved };
  } else {
    base._configPath = null;
    base._configMissing = true;
  }
  return base;
}

export function applyCliOverrides(cfg, opts = {}) {
  const out = { ...cfg };
  if (opts.rom) out.rom = opts.rom;
  if (opts.slot6Rom) out.slot6Rom = opts.slot6Rom;
  if (opts.slot6Mode) out.slot6Mode = opts.slot6Mode;
  if (opts.disk1) out.disk1 = opts.disk1;
  if (opts.disk2) out.disk2 = opts.disk2;
  if (opts.machine) out.machine = opts.machine;
  if (opts.videoMode) out.videoMode = opts.videoMode;
  if (opts.displayEffect) out.displayEffect = opts.displayEffect;
  return out;
}

export function sha256File(filePath) {
  const h = createHash("sha256");
  h.update(fs.readFileSync(filePath));
  return h.digest("hex");
}

export function identifyAsset(filePath) {
  if (!filePath || !fs.existsSync(filePath)) {
    return { present: false, path: filePath || null };
  }
  const st = fs.statSync(filePath);
  const sha256 = sha256File(filePath);
  const ext = path.extname(filePath).toLowerCase();
  let type = "unknown";
  let detectedFormat = ext.replace(".", "") || "unknown";
  if (st.size === 12288) type = "motherboard_rom_12k";
  else if (st.size === 256) type = "slot6_prom_256";
  else if (st.size === 143360) type = "disk_16sector_35track";
  else if (ext === ".dsk" || ext === ".do") type = "disk_image";
  else if (ext === ".po") type = "disk_image_po";
  else if (ext === ".nib") type = "disk_image_nib";
  else if (ext === ".woz") type = "disk_image_woz";
  else if (ext === ".rom") type = "rom_image";
  return {
    present: true,
    path: filePath,
    type,
    size: st.size,
    sha256,
    detectedFormat,
  };
}

export function resolveUserMediaPath(catalogFile) {
  if (!catalogFile) return null;
  const candidates = [
    path.join(root, "library/user", catalogFile),
    path.join(root, "local/media", catalogFile),
    catalogFile,
  ];
  for (const c of candidates) {
    if (fs.existsSync(c)) return c;
  }
  return null;
}
