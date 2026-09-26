/**
 * µGulp task-catalog regression tests for ESP][.
 *
 * Guards against:
 * - loss of pre-Apple-II-media tasks (catalog completeness)
 * - silent registry replacement (union)
 * - missing en-US / de-DE metadata for first-party tasks
 * - readiness emphasis binding
 */
import test from "node:test";
import assert from "node:assert/strict";
import { existsSync, readFileSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "../..");
const GULPFILE = join(ROOT, "gulpfile.mjs");
const I18X_DE = join(ROOT, "i18x", "gulp", "de-DE.json");
const I18X_EN = join(ROOT, "i18x", "gulp", "en-US.json");

/**
 * Known-good first-party task IDs from the last pre-regression commit
 * (parent of b886473 / 52357f4 gulpfile inventory).
 */
export const BASELINE_TASK_IDS = Object.freeze([
  "build",
  "flash",
  "upload",
  "size",
  "clean",
  "rebuild",
  "devices",
  "monitor",
  "docs",
  "docs:en-US",
  "docs:de-DE",
  "backup:git",
  "backup",
  "backup:all",
  "apple2js:sync",
  "apple2js:catalog",
  "apple2js:audit",
  "media:identify",
  "media:import",
  "media:inspect",
  "sd:prepare",
  "apple2:rom-identify",
  "apple2:rom-test",
  "apple2:host",
  "apple2:disk-test",
  "apple2:compat",
  "device:upload",
  "device:usb-storage",
]);

/** Apple II local-media tasks introduced with b886473 (must remain). */
export const APPLE2_MEDIA_TASK_IDS = Object.freeze([
  "apple2:rom:sync",
  "apple2:media:sync",
  "apple2:media:prepare",
  "apple2:media:status",
  "apple2:media:list",
  "apple2:media:audit",
  "apple2:media:clean",
  "apple2:device:sync",
]);

/**
 * Merge two task-id → meta maps. Must never silently drop the left set.
 * @param {Map<string, object>} left
 * @param {Map<string, object>} right
 * @returns {Map<string, object>}
 */
export function mergeTaskRegistry(left, right) {
  const out = new Map(left);
  for (const [id, meta] of right) {
    if (out.has(id)) {
      // Same id may be re-registered only with identical gulpName identity.
      const prev = out.get(id);
      if (prev && prev !== meta && prev.gulpName !== meta.gulpName) {
        throw new Error(`task registry replace forbidden for id=${id}`);
      }
    }
    out.set(id, meta);
  }
  return out;
}

function loadGulpTasks() {
  return import(pathToFileURL(GULPFILE).href);
}

function collectTaggedTasks(mod) {
  /** @type {Map<string, Function>} */
  const map = new Map();
  for (const value of Object.values(mod)) {
    if (typeof value === "function" && value.displayName) {
      map.set(value.displayName, value);
    }
  }
  return map;
}

test("baseline µGulp tasks remain present (catalog completeness)", async () => {
  const mod = await loadGulpTasks();
  const tasks = collectTaggedTasks(mod);
  const ids = [...tasks.keys()];
  const missing = BASELINE_TASK_IDS.filter((id) => !tasks.has(id));
  assert.deepEqual(
    missing,
    [],
    `missing baseline tasks: ${missing.join(", ")} (have ${ids.length})`,
  );
});

test("new Apple II media tasks remain present", async () => {
  const mod = await loadGulpTasks();
  const tasks = collectTaggedTasks(mod);
  const missing = APPLE2_MEDIA_TASK_IDS.filter((id) => !tasks.has(id));
  assert.deepEqual(missing, [], `missing Apple II tasks: ${missing.join(", ")}`);
});

test("registry merge yields UNION (no silent replace of old set)", () => {
  const oldSet = new Map(
    BASELINE_TASK_IDS.map((id) => [id, { gulpName: id, set: "old" }]),
  );
  const newSet = new Map(
    APPLE2_MEDIA_TASK_IDS.map((id) => [id, { gulpName: id, set: "new" }]),
  );
  const merged = mergeTaskRegistry(oldSet, newSet);
  assert.equal(merged.size, BASELINE_TASK_IDS.length + APPLE2_MEDIA_TASK_IDS.length);
  for (const id of BASELINE_TASK_IDS) assert.equal(merged.get(id).set, "old");
  for (const id of APPLE2_MEDIA_TASK_IDS) assert.equal(merged.get(id).set, "new");

  assert.throws(
    () =>
      mergeTaskRegistry(
        new Map([["build", { gulpName: "build" }]]),
        new Map([["build", { gulpName: "build-replaced" }]]),
      ),
    /replace forbidden/,
  );
});

test("first-party tasks expose µDisplayName and µGroup (not raw-only)", async () => {
  const mod = await loadGulpTasks();
  const tasks = collectTaggedTasks(mod);
  const firstParty = [...BASELINE_TASK_IDS, ...APPLE2_MEDIA_TASK_IDS];
  const bad = [];
  for (const id of firstParty) {
    const fn = tasks.get(id);
    assert.ok(fn, `missing task ${id}`);
    const dn = fn["\u00b5DisplayName"];
    const group = fn["\u00b5Group"];
    if (!dn || typeof dn !== "string") bad.push(`${id}: missing µDisplayName`);
    else if (dn === id) bad.push(`${id}: µDisplayName equals technical id`);
    if (!group || typeof group !== "string") bad.push(`${id}: missing µGroup`);
  }
  assert.deepEqual(bad, [], bad.join("\n"));
});

test("en-US and de-DE gulp dictionaries cover first-party display names", async () => {
  assert.ok(existsSync(I18X_EN), "i18x/gulp/en-US.json required");
  assert.ok(existsSync(I18X_DE), "i18x/gulp/de-DE.json required");
  const en = JSON.parse(readFileSync(I18X_EN, "utf8"));
  const de = JSON.parse(readFileSync(I18X_DE, "utf8"));
  const mod = await loadGulpTasks();
  const tasks = collectTaggedTasks(mod);
  const firstParty = [...BASELINE_TASK_IDS, ...APPLE2_MEDIA_TASK_IDS];
  const missingEn = [];
  const missingDe = [];
  for (const id of firstParty) {
    const fn = tasks.get(id);
    const dn = fn["\u00b5DisplayName"];
    const group = fn["\u00b5Group"];
    for (const phrase of [dn, group]) {
      if (!(phrase in en)) missingEn.push(`${id}: ${phrase}`);
      if (!(phrase in de)) missingDe.push(`${id}: ${phrase}`);
      else {
        // de-DE must not simply echo the technical id
        assert.notEqual(de[phrase], id);
        assert.ok(String(de[phrase]).length > 0);
      }
    }
  }
  assert.deepEqual(missingEn, [], `missing en-US:\n${missingEn.join("\n")}`);
  assert.deepEqual(missingDe, [], `missing de-DE:\n${missingDe.join("\n")}`);
});

test("Apple II readiness emphasis binds µAttention on guide tasks", async () => {
  const { computeApple2MediaReadiness } = await import(
    "./apple2-local/mugulp-readiness.mjs"
  );
  const mod = await loadGulpTasks();
  const tasks = collectTaggedTasks(mod);
  for (const id of [
    "apple2:rom:sync",
    "apple2:media:sync",
    "apple2:media:prepare",
    "apple2:compat",
    "apple2:device:sync",
  ]) {
    const fn = tasks.get(id);
    assert.equal(typeof fn["\u00b5Attention"], "function", `${id} µAttention`);
  }
  const r = computeApple2MediaReadiness({
    title: "galaxian",
    manifest: { roms: {}, titles: {} },
  });
  assert.equal(r.step, "rom_sync");
  assert.equal(r.emphasizeTaskId, "apple2:rom:sync");

  const r2 = computeApple2MediaReadiness({
    title: "galaxian",
    manifest: {
      roms: {
        appleiigo: { id: "appleiigo", path: GULPFILE }, // exists as path sentinel
      },
      titles: {},
    },
  });
  // freeRomReady true because path exists → next is media_sync
  assert.equal(r2.freeRomReady, true);
  assert.equal(r2.emphasizeTaskId, "apple2:media:sync");
});

test("gulpfile encoding uses real U+00B5 for µ metadata keys", () => {
  const raw = readFileSync(GULPFILE);
  assert.ok(raw.includes(Buffer.from([0xc2, 0xb5])), "real UTF-8 µ required");
  assert.equal(
    raw.includes(Buffer.from([0xc3, 0x82, 0xc2, 0xb5])),
    false,
    "double-encoded µ must not appear",
  );
  const text = raw.toString("utf8");
  assert.match(text, /export const µI18xContext/);
  assert.match(text, /export const µGroups/);
});
