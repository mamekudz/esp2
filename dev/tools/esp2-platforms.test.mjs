/**
 * Multi-target platform IDs + Project Control helpers.
 */
import test from "node:test";
import assert from "node:assert/strict";
import { createRequire } from "node:module";
import { readFileSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import {
  PLATFORM_DEFAULT,
  PLATFORM_DIYMORE,
  PLATFORM_RLCD42,
  PLATFORM_WAVESHARE,
  PLATFORM_IDS,
  buildFilterPlatformOptions,
  buildPlatformProjectControl,
  defaultPioEnvForPlatform,
  pioEnvsForPlatform,
  resolveActivePlatform,
  visibleOnDiymore,
  visibleOnRlcd42,
  visibleOnWaveshare,
} from "./esp2-platforms.mjs";
import { ResolvePioEnv } from "./pio.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "../..");
const require = createRequire(import.meta.url);

test("platform IDs are stable and default is Waveshare AMOLED", () => {
  assert.equal(PLATFORM_WAVESHARE, "waveshare-amoled-s3");
  assert.equal(PLATFORM_DIYMORE, "diymore-es3c28p");
  assert.equal(PLATFORM_RLCD42, "waveshare-rlcd42");
  assert.equal(PLATFORM_DEFAULT, PLATFORM_WAVESHARE);
  assert.deepEqual([...PLATFORM_IDS], [PLATFORM_WAVESHARE, PLATFORM_DIYMORE, PLATFORM_RLCD42]);
});

test("Project Control declaration is radio id=platform with short labels", () => {
  const ctrl = buildPlatformProjectControl();
  assert.equal(ctrl.id, "platform");
  assert.equal(ctrl.type, "radio");
  assert.equal(ctrl.label, "Target");
  assert.equal(ctrl.default, PLATFORM_WAVESHARE);
  assert.equal(ctrl.options.length, 3);
  assert.equal(ctrl.options[0].value, PLATFORM_WAVESHARE);
  assert.equal(ctrl.options[0].label, "Waveshare AMOLED");
  assert.equal(ctrl.options[1].value, PLATFORM_DIYMORE);
  assert.equal(ctrl.options[1].label, 'diymore 2.8"');
  assert.match(ctrl.options[1].tooltip, /AM03-FBBA0125-AAA/);
  assert.equal(ctrl.options[2].value, PLATFORM_RLCD42);
  assert.equal(ctrl.options[2].label, "RLCD 4.2");
});

test("microgulp.config.json mirrors platform Project Control", () => {
  const cfg = JSON.parse(
    readFileSync(join(ROOT, "microgulp.config.json"), "utf8"),
  );
  const platform = (cfg.project?.controls || []).find((c) => c.id === "platform");
  assert.ok(platform);
  assert.equal(platform.type, "radio");
  assert.equal(platform.default, PLATFORM_WAVESHARE);
  assert.deepEqual(
    platform.options.map((o) => o.value),
    [PLATFORM_WAVESHARE, PLATFORM_DIYMORE, PLATFORM_RLCD42],
  );
});

test("resolveActivePlatform uses MICROGULP_CONTROLS then ESP2_PLATFORM then default", () => {
  const prevControls = process.env.MICROGULP_CONTROLS;
  const prevPlatform = process.env.ESP2_PLATFORM;
  try {
    delete process.env.MICROGULP_CONTROLS;
    delete process.env.ESP2_PLATFORM;
    assert.equal(resolveActivePlatform(), PLATFORM_WAVESHARE);

    process.env.ESP2_PLATFORM = PLATFORM_DIYMORE;
    assert.equal(resolveActivePlatform(), PLATFORM_DIYMORE);

    process.env.MICROGULP_CONTROLS = JSON.stringify({
      platform: PLATFORM_WAVESHARE,
    });
    assert.equal(resolveActivePlatform(), PLATFORM_WAVESHARE);

    process.env.MICROGULP_CONTROLS = JSON.stringify({
      platform: PLATFORM_DIYMORE,
    });
    assert.equal(resolveActivePlatform(), PLATFORM_DIYMORE);

    process.env.MICROGULP_CONTROLS = JSON.stringify({ platform: "nope" });
    delete process.env.ESP2_PLATFORM;
    assert.equal(resolveActivePlatform(), PLATFORM_WAVESHARE);
  } finally {
    if (prevControls === undefined) delete process.env.MICROGULP_CONTROLS;
    else process.env.MICROGULP_CONTROLS = prevControls;
    if (prevPlatform === undefined) delete process.env.ESP2_PLATFORM;
    else process.env.ESP2_PLATFORM = prevPlatform;
  }
});

test("pio envs are scoped per platform", () => {
  const prev = process.env.ESP2_PIO_ENV;
  try {
    delete process.env.ESP2_PIO_ENV;
    assert.deepEqual([...pioEnvsForPlatform(PLATFORM_WAVESHARE)], [
      "apple2_text",
      "bringup",
      "core_smoke",
    ]);
    assert.equal(defaultPioEnvForPlatform(PLATFORM_WAVESHARE), "apple2_text");
    assert.deepEqual([...pioEnvsForPlatform(PLATFORM_DIYMORE)], [
      "diymore_bringup",
      "diymore_display_probe",
      "diymore_apple2",
    ]);
    assert.equal(defaultPioEnvForPlatform(PLATFORM_DIYMORE), "diymore_apple2");
    assert.deepEqual([...pioEnvsForPlatform(PLATFORM_RLCD42)], [
      "rlcd42_bringup",
      "rlcd42_display_probe",
      "rlcd42_apple2",
    ]);
    assert.equal(defaultPioEnvForPlatform(PLATFORM_RLCD42), "rlcd42_bringup");

    assert.equal(
      ResolvePioEnv(undefined, {
        allowedEnvs: pioEnvsForPlatform(PLATFORM_WAVESHARE),
        defaultEnv: defaultPioEnvForPlatform(PLATFORM_WAVESHARE),
      }),
      "apple2_text",
    );
    assert.equal(
      ResolvePioEnv(undefined, {
        allowedEnvs: pioEnvsForPlatform(PLATFORM_DIYMORE),
        defaultEnv: defaultPioEnvForPlatform(PLATFORM_DIYMORE),
      }),
      "diymore_apple2",
    );
    assert.throws(
      () =>
        ResolvePioEnv("apple2_text", {
          allowedEnvs: pioEnvsForPlatform(PLATFORM_DIYMORE),
          defaultEnv: defaultPioEnvForPlatform(PLATFORM_DIYMORE),
        }),
      /Unknown PlatformIO env/,
    );
  } finally {
    if (prev === undefined) delete process.env.ESP2_PIO_ENV;
    else process.env.ESP2_PIO_ENV = prev;
  }
});

test("µVisible helpers flip with controls.platform", () => {
  assert.equal(
    visibleOnWaveshare({ controls: { platform: PLATFORM_WAVESHARE } }),
    true,
  );
  assert.equal(
    visibleOnWaveshare({ controls: { platform: PLATFORM_DIYMORE } }),
    false,
  );
  assert.equal(
    visibleOnDiymore({ controls: { platform: PLATFORM_DIYMORE } }),
    true,
  );
  assert.equal(
    visibleOnDiymore({ controls: { platform: PLATFORM_WAVESHARE } }),
    false,
  );
  assert.equal(
    visibleOnRlcd42({ controls: { platform: PLATFORM_RLCD42 } }),
    true,
  );
  assert.equal(
    visibleOnRlcd42({ controls: { platform: PLATFORM_WAVESHARE } }),
    false,
  );
});

test("gulp-mu-build-filter platform keeps only matching board tags", () => {
  const µBuildFilter = require("gulp-mu-build-filter");
  const waveshareH = readFileSync(
    join(ROOT, "include/boards/waveshare_amoled_s3.h"),
    "utf8",
  );
  const diymoreH = readFileSync(
    join(ROOT, "include/boards/diymore_es3c28p.h"),
    "utf8",
  );

  const ws = µBuildFilter({
    text: waveshareH + "\n" + diymoreH,
    ...buildFilterPlatformOptions(PLATFORM_WAVESHARE),
  });
  assert.match(ws, /ESP2_PLATFORM_TAG_WAVESHARE/);
  assert.doesNotMatch(ws, /ESP2_PLATFORM_TAG_DIYMORE/);

  const dm = µBuildFilter({
    text: waveshareH + "\n" + diymoreH,
    ...buildFilterPlatformOptions(PLATFORM_DIYMORE),
  });
  assert.match(dm, /ESP2_PLATFORM_TAG_DIYMORE/);
  assert.doesNotMatch(dm, /ESP2_PLATFORM_TAG_WAVESHARE/);
});

test("platformio.ini declares diymore_bringup with measured 16MB flash", () => {
  const ini = readFileSync(join(ROOT, "platformio.ini"), "utf8");
  assert.match(ini, /\[env:diymore_bringup\]/);
  assert.match(ini, /default_envs\s*=\s*apple2_text/);
  const diymoreBlock = ini.split("[env:diymore_bringup]")[1].split("[env:")[0];
  assert.match(diymoreBlock, /BOARD_HAS_PSRAM/);
  assert.match(diymoreBlock, /ESP2_PLATFORM_DIYMORE_ES3C28P/);
  assert.match(diymoreBlock, /board_upload\.flash_size\s*=\s*16MB/);
});

test("platformio.ini declares rlcd42_bringup with 16MB flash", () => {
  const ini = readFileSync(join(ROOT, "platformio.ini"), "utf8");
  assert.match(ini, /\[env:rlcd42_bringup\]/);
  const block = ini.split("[env:rlcd42_bringup]")[1].split("[env:")[0];
  assert.match(block, /BOARD_HAS_PSRAM/);
  assert.match(block, /ESP2_PLATFORM_WAVESHARE_RLCD42/);
  assert.match(block, /board_upload\.flash_size\s*=\s*16MB/);
});

test(".microgulp/ is gitignored (covers project-controls.json persistence)", () => {
  const gi = readFileSync(join(ROOT, ".gitignore"), "utf8");
  assert.match(gi, /^\.microgulp\/$/m);
});
