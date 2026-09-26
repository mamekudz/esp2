/**
 * µGulp task-catalog regression tests for ESP][.
 *
 * Guards against:
 * - loss of canonical first-party tasks (see docs/tooling/gulp-task-manifest.json)
 * - unexpected first-party duplicates
 * - missing / collapsed required groups
 * - missing en-US / de-DE metadata
 * - UTF-8 mojibake of µ metadata
 * - readiness emphasis binding
 */
import test from "node:test";
import assert from "node:assert/strict";
import { existsSync, readFileSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "../..");
const GULPFILE = join(ROOT, "gulpfile.mjs");
const MANIFEST = join(ROOT, "docs", "tooling", "gulp-task-manifest.json");
const I18X_DE = join(ROOT, "i18x", "gulp", "de-DE.json");
const I18X_EN = join(ROOT, "i18x", "gulp", "en-US.json");
const UTF8_FILES = [
  GULPFILE,
  join(ROOT, "dev", "tools", "build-i18x-gulp.mjs"),
  join(ROOT, "dev", "tools", "pio.mjs"),
  I18X_EN,
  I18X_DE,
];

const MU = Buffer.from([0xc2, 0xb5]);
const DOUBLE_MU = Buffer.from([0xc3, 0x82, 0xc2, 0xb5]);
const MOJIBAKE_MARKERS = ["Ã", "Âµ", "ÃÂµ"];

/**
 * @returns {{ groups: string[], tasks: { id: string, group: string, i18x?: boolean }[] }}
 */
export function loadCanonicalManifest() {
  assert.ok(existsSync(MANIFEST), "canonical manifest required");
  return JSON.parse(readFileSync(MANIFEST, "utf8"));
}

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
  return import(pathToFileURL(GULPFILE).href + `?t=${Date.now()}`);
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

function stripContext(phrase) {
  return String(phrase || "").replace(/<context="[^"]+"\/>$/, "");
}

test("canonical manifest exists and lists unique task IDs", () => {
  const manifest = loadCanonicalManifest();
  assert.ok(Array.isArray(manifest.tasks) && manifest.tasks.length > 0);
  assert.ok(Array.isArray(manifest.groups) && manifest.groups.length > 0);
  const ids = manifest.tasks.map((t) => t.id);
  assert.equal(new Set(ids).size, ids.length, "duplicate IDs in manifest");
});

test("no canonical first-party task is missing from gulp registry", async () => {
  const manifest = loadCanonicalManifest();
  const mod = await loadGulpTasks();
  const tasks = collectTaggedTasks(mod);
  const missing = manifest.tasks
    .map((t) => t.id)
    .filter((id) => !tasks.has(id));
  assert.deepEqual(
    missing,
    [],
    `missing canonical tasks: ${missing.join(", ")} (have ${tasks.size})`,
  );
});

test("no unexpected first-party duplicate displayNames", async () => {
  const mod = await loadGulpTasks();
  const seen = new Map();
  const dups = [];
  for (const value of Object.values(mod)) {
    if (typeof value !== "function" || !value.displayName) continue;
    const id = value.displayName;
    if (seen.has(id) && seen.get(id) !== value) dups.push(id);
    else seen.set(id, value);
  }
  assert.deepEqual(dups, [], `duplicate tagged tasks: ${dups.join(", ")}`);
});

test("required µGroups remain present", async () => {
  const manifest = loadCanonicalManifest();
  const mod = await loadGulpTasks();
  const groups = mod["\u00b5Groups"];
  assert.ok(groups && groups.groups, "µGroups export required");
  const keys = Object.keys(groups.groups).map(stripContext);
  const missing = manifest.groups.filter((g) => !keys.includes(g));
  assert.deepEqual(missing, [], `missing groups: ${missing.join(", ")}`);
  // Git / Backup / Firmware must start open (UX recovery)
  assert.equal(groups.groups['Firmware<context="µGroup"/>'], "open");
  assert.equal(groups.groups['Git<context="µGroup"/>'], "open");
  assert.equal(groups.groups['Backup<context="µGroup"/>'], "open");
});

test("canonical tasks expose µDisplayName / µGroup matching manifest group", async () => {
  const manifest = loadCanonicalManifest();
  const mod = await loadGulpTasks();
  const tasks = collectTaggedTasks(mod);
  const bad = [];
  for (const entry of manifest.tasks) {
    const fn = tasks.get(entry.id);
    assert.ok(fn, `missing task ${entry.id}`);
    const dn = fn["\u00b5DisplayName"];
    const group = fn["\u00b5Group"];
    if (!dn || typeof dn !== "string") bad.push(`${entry.id}: missing µDisplayName`);
    else if (dn === entry.id) bad.push(`${entry.id}: µDisplayName equals technical id`);
    else if (MOJIBAKE_MARKERS.some((m) => dn.includes(m)))
      bad.push(`${entry.id}: mojibake in µDisplayName`);
    if (!group || typeof group !== "string") bad.push(`${entry.id}: missing µGroup`);
    else if (stripContext(group) !== entry.group)
      bad.push(
        `${entry.id}: group want ${entry.group} got ${stripContext(group)}`,
      );
  }
  assert.deepEqual(bad, [], bad.join("\n"));
});

test("en-US and de-DE cover canonical display names + groups", async () => {
  assert.ok(existsSync(I18X_EN), "i18x/gulp/en-US.json required");
  assert.ok(existsSync(I18X_DE), "i18x/gulp/de-DE.json required");
  const en = JSON.parse(readFileSync(I18X_EN, "utf8"));
  const de = JSON.parse(readFileSync(I18X_DE, "utf8"));
  const manifest = loadCanonicalManifest();
  const mod = await loadGulpTasks();
  const tasks = collectTaggedTasks(mod);
  const missingEn = [];
  const missingDe = [];
  for (const entry of manifest.tasks) {
    const fn = tasks.get(entry.id);
    const dn = fn["\u00b5DisplayName"];
    const group = fn["\u00b5Group"];
    for (const phrase of [dn, group]) {
      if (!(phrase in en)) missingEn.push(`${entry.id}: ${phrase}`);
      if (!(phrase in de)) missingDe.push(`${entry.id}: ${phrase}`);
      else {
        assert.notEqual(de[phrase], entry.id);
        assert.ok(String(de[phrase]).length > 0);
        for (const m of MOJIBAKE_MARKERS) {
          assert.equal(
            String(de[phrase]).includes(m) && m !== "Ã",
            false,
            `de-DE mojibake for ${phrase}`,
          );
        }
      }
    }
  }
  assert.deepEqual(missingEn, [], `missing en-US:\n${missingEn.join("\n")}`);
  assert.deepEqual(missingDe, [], `missing de-DE:\n${missingDe.join("\n")}`);
});

test("UTF-8 integrity: real µ, no double-encoding in metadata sources", () => {
  for (const file of UTF8_FILES) {
    assert.ok(existsSync(file), file);
    const raw = readFileSync(file);
    const text = raw.toString("utf8");
    if (file.endsWith("gulpfile.mjs") || file.endsWith("build-i18x-gulp.mjs")) {
      assert.ok(raw.includes(MU), `${file}: real UTF-8 µ required`);
    }
    assert.equal(
      raw.includes(DOUBLE_MU),
      false,
      `${file}: double-encoded µ must not appear`,
    );
    assert.equal(text.includes("ÃÂµ"), false, `${file}: ÃÂµ forbidden`);
    assert.equal(text.includes("Âµ"), false, `${file}: Âµ forbidden`);
  }
  const gulp = readFileSync(GULPFILE, "utf8");
  assert.match(gulp, /export const µI18xContext/);
  assert.match(gulp, /export const µGroups/);
});

test("registry merge yields UNION (no silent replace of old set)", () => {
  const manifest = loadCanonicalManifest();
  const half = Math.floor(manifest.tasks.length / 2);
  const leftIds = manifest.tasks.slice(0, half).map((t) => t.id);
  const rightIds = manifest.tasks.slice(half).map((t) => t.id);
  const oldSet = new Map(leftIds.map((id) => [id, { gulpName: id, set: "old" }]));
  const newSet = new Map(rightIds.map((id) => [id, { gulpName: id, set: "new" }]));
  const merged = mergeTaskRegistry(oldSet, newSet);
  assert.equal(merged.size, leftIds.length + rightIds.length);
  assert.throws(
    () =>
      mergeTaskRegistry(
        new Map([["build", { gulpName: "build" }]]),
        new Map([["build", { gulpName: "build-replaced" }]]),
      ),
    /replace forbidden/,
  );
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
});

test("52357f4 incomplete baseline is smaller than canonical catalog", () => {
  const manifest = loadCanonicalManifest();
  // Documented incomplete baseline count (see gulp-task-history.md)
  assert.ok(manifest.tasks.length > 28);
  assert.ok(manifest.tasks.length >= 36);
});
