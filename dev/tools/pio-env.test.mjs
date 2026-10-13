/**
 * PlatformIO env resolution for ESP][ µGulp firmware tasks.
 */
import test from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { ResolvePioEnv, PIO_ENV_DEFAULT, PIO_ENVS, UploadArgs } from "./pio.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "../..");

test("production default PlatformIO env is apple2_text", () => {
  const prev = process.env.ESP2_PIO_ENV;
  try {
    delete process.env.ESP2_PIO_ENV;
    assert.equal(PIO_ENV_DEFAULT, "apple2_text");
    assert.equal(ResolvePioEnv(), "apple2_text");
  } finally {
    if (prev === undefined) delete process.env.ESP2_PIO_ENV;
    else process.env.ESP2_PIO_ENV = prev;
  }
});

test("explicit apple2_text / bringup resolve correctly", () => {
  const prev = process.env.ESP2_PIO_ENV;
  try {
    delete process.env.ESP2_PIO_ENV;
    assert.equal(ResolvePioEnv("apple2_text"), "apple2_text");
    assert.equal(ResolvePioEnv("bringup"), "bringup");
    process.env.ESP2_PIO_ENV = "bringup";
    assert.equal(ResolvePioEnv(), "bringup");
    assert.equal(ResolvePioEnv("apple2_text"), "apple2_text");
  } finally {
    if (prev === undefined) delete process.env.ESP2_PIO_ENV;
    else process.env.ESP2_PIO_ENV = prev;
  }
});

test("invalid environment throws clear error (no silent fallback)", () => {
  const prev = process.env.ESP2_PIO_ENV;
  try {
    delete process.env.ESP2_PIO_ENV;
    assert.throws(() => ResolvePioEnv("esp2_no_such_env_zz"), /Unknown PlatformIO env/);
    process.env.ESP2_PIO_ENV = "not_a_real_env";
    assert.throws(() => ResolvePioEnv(), /Unknown ESP2_PIO_ENV/);
  } finally {
    if (prev === undefined) delete process.env.ESP2_PIO_ENV;
    else process.env.ESP2_PIO_ENV = prev;
  }
});

test("UploadArgs uses the same resolved env as explicit build", () => {
  const args = UploadArgs("COM5", "apple2_text");
  assert.deepEqual(args.slice(0, 4), ["run", "-e", "apple2_text", "-t"]);
  assert.ok(args.includes("upload"));
  assert.throws(() => UploadArgs("COM5", "bad_env"), /Unknown PlatformIO env/);
});

test("platformio.ini default_envs is apple2_text; bringup still declared", () => {
  const ini = readFileSync(join(ROOT, "platformio.ini"), "utf8");
  assert.match(ini, /default_envs\s*=\s*apple2_text/);
  for (const env of PIO_ENVS) {
    assert.match(ini, new RegExp(`\\[env:${env}\\]`));
  }
});
