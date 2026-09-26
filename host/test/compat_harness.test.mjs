import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import {
  applyCliOverrides,
  loadMachineConfig,
  identifyAsset,
} from "../tools/machine_config.mjs";
import {
  listCompatTests,
  loadCompatTest,
  evaluateAssetGate,
  runCompatTest,
} from "../tools/compat_runner.mjs";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../..");

export function testCompatDefinitionsPresent() {
  const ids = ["esp2-boot-test", "choplifter", "night-mission", "star-blazer"];
  const listed = listCompatTests().map((t) => t.id).sort();
  for (const id of ids) {
    assert.ok(listed.includes(id), `missing test ${id}`);
    const t = loadCompatTest(id);
    assert.equal(t.schemaVersion, 1);
    assert.ok(t.hostStatus);
    assert.ok(t.esp32Status);
    assert.ok(typeof t.hostStatus === "string");
  }
  const star = loadCompatTest("star-blazer");
  assert.equal(star.audio.status, "SUBJECTIVE_REFERENCE");
  assert.equal(loadCompatTest("choplifter").redistribution, "USER_SUPPLIED_ONLY");
}

export function testCompatSkipsWithoutAssets() {
  const cfg = applyCliOverrides(loadMachineConfig(), {
    rom: null,
    slot6Rom: null,
    disk1: null,
  });
  cfg.rom = null;
  cfg.disk1 = null;
  const chop = loadCompatTest("choplifter");
  const report = evaluateAssetGate(chop, cfg);
  assert.equal(report.status, "SKIPPED_NO_SYSTEM_ROM");
  assert.equal(report.hostStatus, "BLOCKED_MISSING_ASSET");
}

export function testCompatProjectOwnedGate() {
  const t = loadCompatTest("esp2-boot-test");
  const cfg = loadMachineConfig();
  const report = evaluateAssetGate(t, cfg);
  assert.equal(report.status, "PASS");
  assert.ok(report.assets.disk1.present);
}

export function testCompatTitlesMatrix() {
  const p = path.join(root, "docs/compatibility/titles.json");
  const j = JSON.parse(fs.readFileSync(p, "utf8"));
  assert.equal(j.schemaVersion, 1);
  assert.ok(j.titles.find((t) => t.id === "choplifter"));
  assert.ok(j.statuses.includes("BLOCKED_MISSING_ASSET"));
}

export function testMachineConfigExample() {
  const p = path.join(root, "config/apple2.local.example.json");
  const j = JSON.parse(fs.readFileSync(p, "utf8"));
  assert.equal(j.schemaVersion, 1);
  assert.ok(j.machine);
  const cfg = loadMachineConfig(p);
  assert.equal(cfg.machine, "AppleIIPlus");
  const ov = applyCliOverrides(cfg, { disk1: "Esp2BootTest" });
  assert.equal(ov.disk1, "Esp2BootTest");
}

export function testIdentifyMissingAsset() {
  const id = identifyAsset(path.join(root, "local/roms/definitely-missing.rom"));
  assert.equal(id.present, false);
}

export function testCompatRunnerSkippedExit() {
  const report = runCompatTest("choplifter", { run: false });
  assert.ok(
    report.status === "SKIPPED_NO_SYSTEM_ROM" ||
      report.status === "SKIPPED_NO_MEDIA" ||
      report.status === "PASS",
  );
}
