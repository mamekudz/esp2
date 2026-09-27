import assert from "node:assert/strict";
import test from "node:test";
import {
  axisToPaddle,
  mapWindowsKeyToApple7,
  DEFAULT_DEADZONE,
} from "./esp2-input-bridge.mjs";

test("mapWindowsKeyToApple7 letters and return", () => {
  assert.equal(mapWindowsKeyToApple7("a"), 0x41);
  assert.equal(mapWindowsKeyToApple7("A"), 0x41);
  assert.equal(mapWindowsKeyToApple7("1"), 0x31);
  assert.equal(mapWindowsKeyToApple7("return"), 0x0d);
  assert.equal(mapWindowsKeyToApple7("escape"), 0x1b);
  assert.equal(mapWindowsKeyToApple7("space"), 0x20);
  assert.equal(mapWindowsKeyToApple7("a", { ctrl: true }), 0x01);
});

test("axisToPaddle center deadzone", () => {
  assert.equal(axisToPaddle(0, DEFAULT_DEADZONE), 128);
  assert.equal(axisToPaddle(0.05, DEFAULT_DEADZONE), 128);
  assert.ok(axisToPaddle(1, DEFAULT_DEADZONE) >= 250);
  assert.ok(axisToPaddle(-1, DEFAULT_DEADZONE) <= 5);
});
