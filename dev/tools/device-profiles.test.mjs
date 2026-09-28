/**
 * Unit tests for apple2js → device profile generation / selection.
 */
import test from "node:test";
import assert from "node:assert/strict";
import { existsSync, readFileSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import {
  deviceDiskFileName,
  profileIdFromEntry,
  buildProfileSystemJson,
  PROTECTED_PROFILE_IDS,
} from "./device-profiles-from-apple2js.mjs";
import {
  resolveProfileSelection,
  mediaTitleIdForProfile,
  DEMO_PROFILE_IDS,
} from "./device-profiles-activate.mjs";
import { normalizeSystemConfig } from "./device-config-form.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "../..");

test("deviceDiskFileName strips non-alnum", () => {
  assert.equal(deviceDiskFileName("Star Blazer"), "StarBlazer.dsk");
  assert.equal(deviceDiskFileName("Galaxian"), "Galaxian.dsk");
});

test("profileIdFromEntry uses audit id", () => {
  assert.equal(
    profileIdFromEntry({ id: "star-blazer", basename: "starblazer" }),
    "star-blazer",
  );
});

test("star-blazer hand profile validates", () => {
  const raw = JSON.parse(
    readFileSync(join(ROOT, "config/device/profiles/star-blazer/system.json"), "utf8"),
  );
  const n = normalizeSystemConfig(raw);
  assert.equal(n.ok, true, n.errors?.join("; "));
  assert.equal(n.config.media.drive1, "/esp2/disks/StarBlazer.dsk");
  assert.equal(n.config.presentation.monitor, "artifact");
});

test("buildProfileSystemJson is path-only and normalizable", () => {
  const sys = buildProfileSystemJson({
    id: "choplifter",
    title: "Choplifter",
    category: "Game",
    basename: "choplifter",
    upstreamFilename: "json/disks/choplifter.json",
    redistribution: { status: "USER_SUPPLIED_ONLY" },
  });
  const n = normalizeSystemConfig(sys);
  assert.equal(n.ok, true, n.errors?.join("; "));
  assert.equal(sys.meta.apple2jsBasename, "choplifter");
  assert.ok(PROTECTED_PROFILE_IDS.includes("star-blazer"));
});

test("mediaTitleIdForProfile maps hand demos", () => {
  assert.equal(mediaTitleIdForProfile(ROOT, "star-blazer"), "starblazer");
  assert.equal(mediaTitleIdForProfile(ROOT, "galaxian-demo"), "galaxian");
});

test("resolveProfileSelection demos token", () => {
  const ids = resolveProfileSelection(ROOT, "demos");
  for (const id of DEMO_PROFILE_IDS) {
    if (existsSync(join(ROOT, "config/device/profiles", id, "system.json"))) {
      assert.ok(ids.includes(id), id);
    }
  }
});
