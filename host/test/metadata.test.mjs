import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../..");

export function testGameJson() {
  const p = path.join(
    root,
    "fixtures/synthetic/library/DemoCart/game.json",
  );
  const raw = fs.readFileSync(p, "utf8");
  const j = JSON.parse(raw);
  assert.equal(j.schemaVersion, 1);
  assert.ok(j.title);
  assert.ok(Array.isArray(j.disks) && j.disks.length >= 1);
  assert.ok(j.disks[0].file);
}

export function testCompatibilityDb() {
  const p = path.join(root, "docs/compatibility/controllers.json");
  const j = JSON.parse(fs.readFileSync(p, "utf8"));
  assert.equal(j.schemaVersion, 1);
  const vr = j.controllers.find((c) => c.id === "vr-park");
  assert.ok(vr);
  assert.equal(vr.directEspBracket, "experimental");
  const sn30 = j.controllers.find((c) => c.id === "8bitdo-sn30-pro");
  assert.equal(sn30.directEspBracket, "incompatible");
}
