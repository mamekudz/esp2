import assert from "node:assert/strict";
import test from "node:test";
import {
  VirtualPaddleAccumulator,
  PADDLE_CENTER,
  DEFAULT_DIAL_SENSITIVITY,
} from "./esp2-paddle-accumulator.mjs";

test("starts at center", () => {
  const a = new VirtualPaddleAccumulator();
  assert.equal(a.paddle(), PADDLE_CENTER);
});

test("rotate left decreases PDL0", () => {
  const a = new VirtualPaddleAccumulator({ sensitivity: 2 });
  a.applyDelta(-3);
  assert.equal(a.paddle(), PADDLE_CENTER - 6);
});

test("rotate right increases PDL0", () => {
  const a = new VirtualPaddleAccumulator({ sensitivity: 2 });
  a.applyDelta(4);
  assert.equal(a.paddle(), PADDLE_CENTER + 8);
});

test("stop leaves value stable", () => {
  const a = new VirtualPaddleAccumulator();
  a.applyDelta(2);
  const v = a.paddle();
  a.applyDelta(0);
  assert.equal(a.paddle(), v);
});

test("clamps min/max no wraparound", () => {
  const a = new VirtualPaddleAccumulator({ sensitivity: 10 });
  a.applyDelta(-1000);
  assert.equal(a.paddle(), 0);
  a.applyDelta(1000);
  assert.equal(a.paddle(), 255);
});

test("default sensitivity documented", () => {
  assert.equal(DEFAULT_DIAL_SENSITIVITY, 2);
});
