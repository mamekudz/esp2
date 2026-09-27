/**
 * device:config form metadata + save/apply separation tests.
 * Opening/declaring the form must not upload or touch the device.
 */
import test from "node:test";
import assert from "node:assert/strict";
import { existsSync, readFileSync, rmSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import {
  configFromFormValues,
  formDefaultsFromConfig,
  formDefaultsFromPreset,
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
  assert.equal(n.config.presentation.monitor, "artifact");
  assert.equal(n.config.presentation.effect, "clean");
  assert.equal(n.config.presentation.color, "artifact");
  assert.equal(n.config.display.screensaverSeconds, 300);
  assert.equal(n.config.startup.macro, "galaxian-start");
});

test("formDefaultsFromPreset loads galaxian-demo into editable fields", () => {
  const d = formDefaultsFromPreset(ROOT, "galaxian-demo");
  assert.equal(d.preset, "galaxian-demo");
  assert.equal(d.rom, "/esp2/roms/system.rom");
  assert.equal(d.drive1, "/esp2/disks/Galaxian.dsk");
  assert.equal(d.bootFromDisk, true);
  assert.equal(d.startupMacro, "galaxian-start");
  assert.equal(d.orientation, "landscape");
  assert.equal(d.monitor, "artifact");
  assert.equal(d.effect, "clean");
  assert.equal(d.screensaverSeconds, 300);
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
    monitor: "artifact",
    effect: "crt",
    screensaverSeconds: 300,
  });
  assert.equal(ok.ok, true);
  assert.equal(ok.config.media.drive2, null);
  assert.equal(ok.config.presentation.monitor, "artifact");
  assert.equal(ok.config.presentation.effect, "crt");
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
    monitor: "green",
    effect: "clean",
    screensaverSeconds: 0,
  });
  assert.equal(cfg.ok, true);
  const saved = saveConfigLocal(ROOT, cfg.config, tmpName, "custom");
  assert.ok(existsSync(saved.localSystem));
  const written = JSON.parse(readFileSync(saved.localSystem, "utf8"));
  assert.equal(written.startup.bootFromDisk, false);
  assert.equal(written.presentation.orientation, "classic");
  assert.equal(written.presentation.monitor, "green");
  assert.equal(written.presentation.effect, "clean");
  rmSync(join(ROOT, "config/device/profiles", tmpName), { recursive: true, force: true });
});

test("device:config uses preset step; galaxian-demo fills edit defaults", async () => {
  const mod = await import(pathToFileURL(join(ROOT, "gulpfile.mjs")).href + `?t=${Date.now()}`);
  const task = Object.values(mod).find(
    (v) => typeof v === "function" && v.displayName === "device:config",
  );
  assert.ok(task, "device:config export");
  assert.equal(
    typeof mod.buildDeviceConfigEditForm,
    "undefined",
    "helper must not be a dashboard export",
  );
  const step1 = task["\u00b5Parameters"];
  assert.ok(step1 && Array.isArray(step1.fields));
  assert.equal(step1.fields.length, 1);
  assert.equal(step1.fields[0].id, "preset");
  assert.equal(step1.fields[0].remember, false);

  const d = formDefaultsFromPreset(ROOT, "galaxian-demo");
  assert.equal(d.rom, "/esp2/roms/system.rom");
  assert.equal(d.drive1, "/esp2/disks/Galaxian.dsk");
  assert.equal(d.orientation, "landscape");
  assert.equal(d.monitor, "artifact");
  assert.equal(d.effect, "clean");
  assert.equal(d.action, "save_local");

  const parser = await loadParseParameterDeclaration();
  if (parser?.ParseParameterDeclaration) {
    const parsed = parser.ParseParameterDeclaration(step1);
    assert.ok(parsed.parameters && parsed.parameters.length === 1);
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
  assert.equal(process.env.ESP2_PORT, undefined);
  if (before !== undefined) process.env.ESP2_PORT = before;
});

test("setup:first-run is tagged and attention-capable", async () => {
  const mod = await import(pathToFileURL(join(ROOT, "gulpfile.mjs")).href + `?t=${Date.now() + 2}`);
  const task = Object.values(mod).find(
    (v) => typeof v === "function" && v.displayName === "setup:first-run",
  );
  assert.ok(task, "setup:first-run export");
  assert.equal(typeof task["\u00b5Attention"], "function");
  assert.match(String(task["\u00b5Group"]), /Tools/);
});
