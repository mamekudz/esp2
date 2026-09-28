import assert from "node:assert/strict";
import test from "node:test";
import {
  axisToPaddle,
  mapWindowsKeyToApple7,
  applyHostBridgeToArgs,
  selectGamepad,
  buildGamepadSelectOptions,
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

test("selectGamepad auto / none / index", () => {
  const pads = [
    { index: 0, name: "XInput#0", id: "XInput#0", connected: true },
    { index: 1, name: "XInput#1", id: "XInput#1", connected: false },
    { index: 2, name: "PadTwo", id: "PadTwo", connected: true },
  ];
  assert.equal(selectGamepad(pads, { gamepad: null }), null);
  assert.equal(selectGamepad(pads, { gamepad: "none" }), null);
  assert.equal(selectGamepad(pads, { gamepad: "auto" })?.index, 0);
  assert.equal(selectGamepad(pads, { gamepad: 2 })?.name, "PadTwo");
  assert.equal(selectGamepad(pads, { gamepadId: "padtwo" })?.index, 2);
});

test("applyHostBridgeToArgs fills gaps; CLI wins", () => {
  const base = {
    gamepad: null,
    gamepadId: "",
    pdl0: "auto",
    pdl1: "auto",
    pb0: "auto",
    dial: false,
    keyboard: true,
    deadzone: DEFAULT_DEADZONE,
  };
  const filled = applyHostBridgeToArgs(
    {
      gamepad: "auto",
      gamepadId: "",
      pdl0: "gamepadX",
      pdl1: "none",
      pb0: "gamepadA",
      dial: false,
      keyboard: true,
      deadzone: 0.1,
    },
    { ...base },
  );
  assert.equal(filled.gamepad, "auto");
  assert.equal(filled.pdl0, "gamepadX");
  assert.equal(filled.pdl1, "none");
  assert.equal(filled.deadzone, 0.1);

  const cliWins = applyHostBridgeToArgs(
    { gamepad: "auto", pdl0: "gamepadX", pdl1: "none", pb0: "gamepadA" },
    { ...base, gamepad: 1, pdl0: "dial" },
  );
  assert.equal(cliWins.gamepad, 1);
  assert.equal(cliWins.pdl0, "dial");
});

test("buildGamepadSelectOptions lists press + connected labels", () => {
  const pads = [
    {
      index: 0,
      name: "8BitDo SN30",
      id: "8BitDo SN30",
      label: "#0 · 8BitDo SN30",
      connected: true,
    },
    {
      index: 1,
      name: "XInput#1",
      id: "XInput#1",
      label: "#1 · asleep/offline",
      connected: false,
    },
  ];
  pads.pnpNames = ["8BitDo SN30", "Other Pad DInput"];
  const opts = buildGamepadSelectOptions(pads, { includePress: true });
  assert.ok(opts.some((o) => o.value === "press"));
  assert.equal(opts.find((o) => o.value === "0")?.label, "#0 · 8BitDo SN30");
  assert.ok(opts.some((o) => o.value === "1"), "offline slots remain selectable");
  assert.ok(opts.some((o) => String(o.label).includes("not XInput")));
});
