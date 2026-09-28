/**
 * µGulp task-catalog regression tests for ESP][.
 *
 * Protects capabilities, canonical visible IDs, metadata, UTF-8.
 * Intentional UX cleanup (hiding aliases/internal tasks) is allowed via
 * docs/tooling/gulp-task-manifest.json version ≥ 2.
 */
import test from "node:test";
import assert from "node:assert/strict";
import { existsSync, readFileSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { spawnSync } from "node:child_process";

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
 * @returns {{
 *   groups: string[],
 *   tasks: { id: string, group: string, i18x?: boolean, visible?: boolean }[],
 *   cliAliases?: { id: string, aliasOf: string }[],
 *   internalCli?: { id: string }[],
 *   layout?: { collapsedDefault?: boolean }
 * }}
 */
export function loadCanonicalManifest() {
  assert.ok(existsSync(MANIFEST), "canonical manifest required");
  return JSON.parse(readFileSync(MANIFEST, "utf8"));
}

export function visibleTasks(manifest) {
  return (manifest.tasks || []).filter((t) => t.visible !== false);
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

function resolveMetaPhrase(value) {
  let v = value;
  if (typeof v === "function") {
    try {
      v = v();
    } catch {
      return null;
    }
  }
  return typeof v === "string" && v.length > 0 ? v : null;
}

test("canonical manifest exists and lists unique visible task IDs", () => {
  const manifest = loadCanonicalManifest();
  assert.ok(Array.isArray(manifest.tasks) && manifest.tasks.length > 0);
  assert.ok(Array.isArray(manifest.groups) && manifest.groups.length > 0);
  const visible = visibleTasks(manifest);
  const ids = visible.map((t) => t.id);
  assert.equal(new Set(ids).size, ids.length, "duplicate IDs in visible tasks");
  assert.ok(manifest.version >= 2, "manifest v2+ for UX cleanup");
});

test("no visible first-party task is missing from gulp registry", async () => {
  const manifest = loadCanonicalManifest();
  const mod = await loadGulpTasks();
  const tasks = collectTaggedTasks(mod);
  const missing = visibleTasks(manifest)
    .map((t) => t.id)
    .filter((id) => !tasks.has(id));
  assert.deepEqual(
    missing,
    [],
    `missing visible tasks: ${missing.join(", ")} (have ${tasks.size})`,
  );
});

test("internal/alias tasks are not exported as dashboard entries", async () => {
  const mod = await loadGulpTasks();
  const tasks = collectTaggedTasks(mod);
  for (const id of [
    "backup:nas",
    "backup:git",
    "releases:context-check",
    "releases:context-fix",
    "releases:i18x-update",
    "device:usb-storage",
  ]) {
    assert.equal(tasks.has(id), false, `${id} must not be a tagged dashboard task`);
  }
  assert.equal(typeof mod.BACKUP_GIT, "undefined");
  assert.equal(typeof mod.backupNas, "undefined");
  assert.equal(typeof mod.RELEASES_CONTEXT_CHECK, "undefined");
  assert.equal(typeof mod.RELEASES_CONTEXT_FIX, "undefined");
  assert.equal(typeof mod.RELEASES_I18X_UPDATE, "undefined");
  assert.equal(typeof mod.deviceUsbStorage, "undefined");
  const gulpSrc = readFileSync(GULPFILE, "utf8");
  assert.equal(gulpSrc.includes("(alias)"), false);
  assert.equal(gulpSrc.includes("Release context CHECK"), false);
  assert.equal(gulpSrc.includes("Release context FIX"), false);
});

test("CLI aliases remain registered with gulp", () => {
  const r = spawnSync(
    process.execPath,
    [
      "--input-type=module",
      "-e",
      `import gulp from 'gulp';
       await import('./gulpfile.mjs');
       console.log(JSON.stringify(Object.keys(gulp.registry().tasks())));`,
    ],
    { cwd: ROOT, encoding: "utf8" },
  );
  assert.equal(r.status, 0, r.stderr || r.stdout);
  const ids = JSON.parse(r.stdout.trim().split("\n").pop());
  for (const need of [
    "backup:nas",
    "backup:git",
    "releases:context-check",
    "releases:context-fix",
    "releases:i18x-update",
    "device:usb-storage",
  ]) {
    assert.ok(ids.includes(need), `gulp task missing: ${need}`);
  }
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

test("required µGroups remain present and start collapsed", async () => {
  const manifest = loadCanonicalManifest();
  const mod = await loadGulpTasks();
  const groups = mod["\u00b5Groups"];
  assert.ok(groups && groups.groups, "µGroups export required");
  assert.equal(groups.collapsed, true, "default collapsed");
  const keys = Object.keys(groups.groups).map(stripContext);
  const missing = manifest.groups.filter((g) => !keys.includes(g));
  assert.deepEqual(missing, [], `missing groups: ${missing.join(", ")}`);
  for (const [key, state] of Object.entries(groups.groups)) {
    assert.equal(
      state,
      "collapsed",
      `${key} must start collapsed (got ${state})`,
    );
  }
});

test("visible tasks expose µDisplayName / µGroup matching manifest", async () => {
  const manifest = loadCanonicalManifest();
  const mod = await loadGulpTasks();
  const tasks = collectTaggedTasks(mod);
  const bad = [];
  for (const entry of visibleTasks(manifest)) {
    const fn = tasks.get(entry.id);
    assert.ok(fn, `missing task ${entry.id}`);
    const rawDn = fn["\u00b5DisplayName"];
    const dn = resolveMetaPhrase(rawDn);
    const group = fn["\u00b5Group"];
    if (rawDn == null) bad.push(`${entry.id}: missing µDisplayName`);
    else if (typeof rawDn !== "string" && typeof rawDn !== "function") {
      bad.push(`${entry.id}: µDisplayName must be string or function`);
    } else if (!dn) bad.push(`${entry.id}: µDisplayName did not resolve to string`);
    else if (dn === entry.id) bad.push(`${entry.id}: µDisplayName equals technical id`);
    else if (MOJIBAKE_MARKERS.some((m) => dn.includes(m)))
      bad.push(`${entry.id}: mojibake in µDisplayName`);
    else if (!dn.includes("V<version/>") && !String(rawDn).includes("V<version/>")) {
      // dynamic functions return registered phrases that include V<version/>
      if (!dn.includes("V<version/>")) {
        bad.push(`${entry.id}: µDisplayName should include V<version/>`);
      }
    }
    if (!group || typeof group !== "string") bad.push(`${entry.id}: missing µGroup`);
    else if (stripContext(group) !== entry.group)
      bad.push(
        `${entry.id}: group want ${entry.group} got ${stripContext(group)}`,
      );
  }
  assert.deepEqual(bad, [], bad.join("\n"));
});

test("en-US and de-DE cover visible display names + groups", async () => {
  assert.ok(existsSync(I18X_EN), "i18x/gulp/en-US.json required");
  assert.ok(existsSync(I18X_DE), "i18x/gulp/de-DE.json required");
  const en = JSON.parse(readFileSync(I18X_EN, "utf8"));
  const de = JSON.parse(readFileSync(I18X_DE, "utf8"));
  const manifest = loadCanonicalManifest();
  const mod = await loadGulpTasks();
  const tasks = collectTaggedTasks(mod);
  const missingEn = [];
  const missingDe = [];
  for (const entry of visibleTasks(manifest)) {
    const fn = tasks.get(entry.id);
    const dn = resolveMetaPhrase(fn["\u00b5DisplayName"]);
    const group = fn["\u00b5Group"];
    for (const phrase of [dn, group]) {
      if (!phrase) {
        missingEn.push(`${entry.id}: (unresolved µDisplayName)`);
        continue;
      }
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
  const visible = visibleTasks(manifest);
  const half = Math.floor(visible.length / 2);
  const leftIds = visible.slice(0, half).map((t) => t.id);
  const rightIds = visible.slice(half).map((t) => t.id);
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

test("visible catalog is leaner than pre-cleanup 55-task clutter", () => {
  const manifest = loadCanonicalManifest();
  const visible = visibleTasks(manifest);
  assert.ok(visible.length >= 40, "still keeps core capabilities");
  assert.ok(visible.length < 55, "intentional UX reduction below 55");
  assert.ok(!(manifest.tasks || []).some((t) => t.id === "backup:nas" && t.visible));
  assert.ok(!(manifest.tasks || []).some((t) => t.id === "releases:context-check"));
});
