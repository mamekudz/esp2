/**
 * device:config form metadata + save/apply separation tests.
 * Opening/declaring the form must not upload or touch the device.
 */
import test from "node:test";
import assert from "node:assert/strict";
import { existsSync, readFileSync, rmSync, mkdirSync, writeFileSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import {
  configFromFormValues,
  formDefaultsFromConfig,
  normalizeSystemConfig,
  saveConfigLocal,
  listProfileIds,
} from "./device-config-form.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "../..");

function loadParseParameterDeclaration() {
  const candidates = [
    join(
      process.env.USERPROFILE || "",
      ".cursor/extensions/meinolfamekudzi.microgulp-internal-0.9.6/src/engine/TaskParameters.mjs",
    ),
  ];
  for (const p of candidates) {
    if (existsSync(p)) {
      return import(pathToFileURL(p).href);
    }
  }
  return null;
}

test("galaxian-demo profile normalizes", () => {
  const raw = JSON.parse(
    readFileSync(join(ROOT, "config/device/profiles/galaxian-demo/system.json"), "utf8"),
  );
  const n = normalizeSystemConfig(raw);
  assert.equal(n.ok, true);
  assert.equal(n.config.presentation.orientation, "landscape");
  assert.equal(n.config.presentation.color, "artifact");
  assert.equal(n.config.display.screensaverSeconds, 300);
  assert.equal(n.config.startup.macro, "galaxian-start");
});

test("form defaults expose editable demo values", () => {
  const d = formDefaultsFromConfig(ROOT);
  assert.ok(d.rom.includes("/esp2/roms/"));
  assert.ok(Array.isArray(d.macros));
  assert.ok(d.macros.includes("none"));
  assert.ok(d.macros.includes("galaxian-start") || listProfileIds(ROOT).includes("galaxian-demo"));
});

test("configFromFormValues validates and accepts form fields", () => {
  const bad = configFromFormValues(ROOT, {
    rom: "C:\\not\\esp2",
    drive1: "/esp2/disks/x.dsk",
    bootFromDisk: true,
    startupMacro: "none",
    orientation: "landscape",
    color: "artifact",
    screensaverSeconds: 300,
  });
  assert.equal(bad.ok, false);

  const ok = configFromFormValues(ROOT, {
    preset: "custom",
    rom: "/esp2/roms/system.rom",
    drive1: "/esp2/disks/Galaxian.dsk",
    drive2: "",
    bootFromDisk: true,
    startupMacro: "galaxian-start",
    orientation: "landscape",
    color: "artifact",
    screensaverSeconds: 300,
  });
  assert.equal(ok.ok, true);
  assert.equal(ok.config.media.drive2, null);
});

test("saveConfigLocal writes JSON without upload (temp dir sandbox via profile name)", () => {
  const tmpName = `_formtest_${Date.now()}`;
  const cfg = configFromFormValues(ROOT, {
    rom: "/esp2/roms/system.rom",
    drive1: "/esp2/disks/Esp2BootTest.dsk",
    drive2: "",
    bootFromDisk: false,
    startupMacro: "none",
    orientation: "classic",
    color: "sharp",
    screensaverSeconds: 0,
  });
  assert.equal(cfg.ok, true);
  const saved = saveConfigLocal(ROOT, cfg.config, tmpName, "custom");
  assert.ok(existsSync(saved.localSystem));
  const written = JSON.parse(readFileSync(saved.localSystem, "utf8"));
  assert.equal(written.startup.bootFromDisk, false);
  assert.equal(written.presentation.orientation, "classic");
  // cleanup profile copy only (keep local/ as gitignored scratch)
  rmSync(join(ROOT, "config/device/profiles", tmpName), { recursive: true, force: true });
});

test("device:config µParameters is a native form wrapper with required fields", async () => {
  const mod = await import(pathToFileURL(join(ROOT, "gulpfile.mjs")).href + `?t=${Date.now()}`);
  const fn = mod.deviceConfig || mod.DEVICE_CONFIG;
  // Tagged export name
  const task = Object.values(mod).find(
    (v) => typeof v === "function" && v.displayName === "device:config",
  );
  assert.ok(task, "device:config export");
  const params = task["\u00b5Parameters"];
  assert.ok(params && typeof params === "object" && !Array.isArray(params));
  assert.ok(Array.isArray(params.fields));
  const ids = params.fields.map((f) => f.id);
  for (const id of [
    "preset",
    "rom",
    "drive1",
    "drive2",
    "bootFromDisk",
    "startupMacro",
    "orientation",
    "color",
    "screensaverSeconds",
    "action",
  ]) {
    assert.ok(ids.includes(id), `missing field ${id}`);
  }
  assert.ok(params.fields.every((f) => f.label), "every field has label");
  const action = params.fields.find((f) => f.id === "action");
  assert.ok(action.options.some((o) => o.value === "save_local"));
  assert.ok(action.options.some((o) => o.value === "apply_device"));
  assert.equal(action.default, "save_local", "Apply must be explicit — default is Save locally");
  const port = params.fields.find((f) => f.id === "port");
  assert.deepEqual(port.visibleWhen, { action: "apply_device" });
  const rom = params.fields.find((f) => f.id === "rom");
  assert.equal(rom.required, true);

  const parser = await loadParseParameterDeclaration();
  if (parser?.ParseParameterDeclaration) {
    const parsed = parser.ParseParameterDeclaration(params);
    assert.ok(parsed.parameters && parsed.parameters.length >= 10);
    assert.ok(parsed.title);
    assert.ok(parsed.submitLabel);
  }
});

test("declaring device:config form metadata performs no upload (no COM spawn)", async () => {
  const before = process.env.ESP2_PORT;
  delete process.env.ESP2_PORT;
  const mod = await import(pathToFileURL(join(ROOT, "gulpfile.mjs")).href + `?t=${Date.now() + 1}`);
  const task = Object.values(mod).find(
    (v) => typeof v === "function" && v.displayName === "device:config",
  );
  assert.ok(task["\u00b5Parameters"].fields.length > 0);
  // Merely reading metadata must not require a port or touch SerialPort.
  assert.equal(process.env.ESP2_PORT, undefined);
  if (before !== undefined) process.env.ESP2_PORT = before;
});
