import assert from "node:assert/strict";

/** Minimal virtual keyboard layout model (data only). */
export const kAppleLayoutPortrait = {
  id: "apple2-touch-v0",
  orientation: "portrait",
  cols: 10,
  rows: 5,
  keys: [
    { id: "key.esc", row: 0, col: 0, appleKeycode: 0x1b },
    { id: "key.1", row: 0, col: 1, appleKeycode: 0x31 },
    { id: "key.a", row: 2, col: 1, appleKeycode: 0x41 },
    { id: "key.return", row: 3, col: 8, colSpan: 2, appleKeycode: 0x0d },
    { id: "key.space", row: 4, col: 2, colSpan: 6, appleKeycode: 0x20 },
    { id: "mod.ctrl", row: 3, col: 0, appleKeycode: 0 },
    { id: "mod.shift", row: 4, col: 0, appleKeycode: 0 },
  ],
};

export function testVirtualKeyboard() {
  assert.equal(kAppleLayoutPortrait.keys.length >= 5, true);
  const space = kAppleLayoutPortrait.keys.find((k) => k.id === "key.space");
  assert.ok(space.colSpan >= 2);
  // Stable ids — not localized glyphs — for i18x presentation layer.
  for (const k of kAppleLayoutPortrait.keys) {
    assert.ok(k.id.startsWith("key.") || k.id.startsWith("mod."));
  }
}

export function testGamepadStateShape() {
  const g = {
    connected: false,
    leftX: 0,
    leftY: 0,
    rightX: 0,
    rightY: 0,
    dpadUp: false,
    dpadDown: false,
    dpadLeft: false,
    dpadRight: false,
    btnA: false,
    btnB: false,
    btnX: false,
    btnY: false,
    btnL1: false,
    btnR1: false,
    btnL2: false,
    btnR2: false,
    btnStart: false,
    btnSelect: false,
    btnHome: false,
    batteryPercent: 255,
  };
  assert.equal(typeof g.leftX, "number");
  assert.equal(g.batteryPercent, 255);
}
