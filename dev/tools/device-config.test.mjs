/**
 * Host tests for ESP][ system.json / macros.json parsers (via node mirrors).
 * Firmware C++ parsers are exercised on-device; this locks the schema contract.
 */
import test from "node:test";
import assert from "node:assert/strict";
import { readFileSync, existsSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "../..");
const DEMO = join(ROOT, "config/device/profiles/galaxian-demo");

test("galaxian-demo system.json has expected demo fields", () => {
  const raw = JSON.parse(readFileSync(join(DEMO, "system.json"), "utf8"));
  assert.equal(raw.schemaVersion, 1);
  assert.equal(raw.machine.rom, "/esp2/roms/system.rom");
  assert.equal(raw.media.drive1, "/esp2/disks/Galaxian.dsk");
  assert.equal(raw.media.drive2, null);
  assert.equal(raw.startup.bootFromDisk, true);
  assert.equal(raw.startup.macro, "galaxian-start");
  assert.equal(raw.presentation.orientation, "landscape");
  assert.equal(raw.presentation.color, "artifact");
  assert.equal(raw.display.screensaverSeconds, 300);
});

test("galaxian-start macro uses generic waitHires + key A", () => {
  const raw = JSON.parse(readFileSync(join(DEMO, "macros.json"), "utf8"));
  assert.equal(raw.schemaVersion, 1);
  const m = raw.macros.find((x) => x.id === "galaxian-start");
  assert.ok(m);
  assert.ok(m.actions.some((a) => a.op === "waitHires"));
  assert.equal(m.actions.at(-1).op, "key");
  assert.equal(m.actions.at(-1).code, "A");
  for (const a of m.actions) {
    assert.ok(["wait", "waitHires", "key"].includes(a.op), a.op);
  }
});

test("example configs exist and are schemaVersion 1", () => {
  for (const f of ["system.example.json", "macros.example.json"]) {
    const p = join(ROOT, "config/device", f);
    assert.ok(existsSync(p), p);
    const j = JSON.parse(readFileSync(p, "utf8"));
    assert.equal(j.schemaVersion, 1);
  }
});

test("docs mention standalone config paths", () => {
  const doc = readFileSync(join(ROOT, "docs/architecture/device-config.md"), "utf8");
  assert.match(doc, /\/esp2\/config\/system\.json/);
  assert.match(doc, /waitHires/);
  assert.match(doc, /screensaverSeconds/);
});
