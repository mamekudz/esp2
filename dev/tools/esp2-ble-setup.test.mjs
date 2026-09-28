/**
 * Host tests: BLE setup model + local hotkey/chord helpers (no firmware).
 */
import assert from "node:assert/strict";
import {
  buildBleSetupForm,
  defaultControlBindings,
  displayNameIsTransparent,
  formatGamepadChord,
  formatKeyboardBinding,
  parseBleDeviceLine,
  parseGamepadChordText,
  parseKeyboardBindingText,
} from "./esp2-ble-setup.mjs";

function testParseDeviceLines() {
  const d = parseBleDeviceLine(
    '#BLE DEV index=0 name="8BitDo SN30 Pro (BlueShift)" type=gamepad addr=AA:BB:CC:DD:EE:FF addrType=1 rssi=-55 known=0 connected=0',
  );
  assert.ok(d);
  assert.equal(d.name, "8BitDo SN30 Pro (BlueShift)");
  assert.equal(d.type, "gamepad");
  assert.equal(d.address, "AA:BB:CC:DD:EE:FF");
  assert.equal(displayNameIsTransparent(d.name), true);

  const k = parseBleDeviceLine(
    '#BLE KNOWN index=1 name="Bluetooth Keyboard" type=keyboard addr=11:22:33:44:55:66 addrType=0 auto=1 bonded=1',
  );
  assert.ok(k);
  assert.equal(k.known, true);
  assert.equal(k.type, "keyboard");
  assert.equal(k.bonded, true);
}

function testBindingsRoundtrip() {
  const c = defaultControlBindings();
  assert.equal(formatKeyboardBinding(c.resetAppleIi.keyboard), "Ctrl+Alt+R");
  assert.match(formatGamepadChord(c.openSettings.gamepad), /start\+select/i);

  const kb = parseKeyboardBindingText("Ctrl+Alt+R");
  assert.deepEqual(kb, {
    ctrl: true,
    alt: true,
    shift: false,
    meta: false,
    key: "R",
  });
  const gp = parseGamepadChordText("start+select (hold 1.5 s)");
  assert.deepEqual(gp.buttons, ["start", "select"]);
  assert.equal(gp.holdMs, 1500);

  // Malformed → null (fallback safety at higher layer)
  assert.equal(parseKeyboardBindingText(""), null);
  assert.equal(parseGamepadChordText("(none)"), null);
}

function testFormShape() {
  const form = buildBleSetupForm({
    port: "COM5",
    portOptions: [
      { value: "COM5", label: "COM5 — ESP32-S3" },
      { value: "COM7", label: "COM7 — USB Serial" },
    ],
    discovered: [
      {
        address: "AA:BB:CC:DD:EE:FF",
        name: "Terios T3",
        type: "gamepad",
        rssi: -60,
        isKnown: false,
      },
    ],
    known: [
      {
        address: "11:22:33:44:55:66",
        name: "Bluetooth Keyboard",
        type: "keyboard",
      },
    ],
  });
  const portField = form.fields.find((f) => f.id === "port");
  assert.equal(portField.type, "select");
  assert.equal(portField.default, "COM5");
  assert.ok(portField.options.some((o) => o.value === "COM5"));
  assert.ok(form.fields.some((f) => f.id === "action"));
  assert.ok(form.fields.some((f) => f.id === "discoveredAddress"));
  assert.ok(form.fields.some((f) => f.id === "knownAddress"));
  assert.ok(form.fields.some((f) => f.id === "resetKeyboard"));
  // No separate tasks — one form.
  const actions = form.fields.find((f) => f.id === "action").options.map((o) => o.value);
  assert.ok(actions.includes("scan"));
  assert.ok(actions.includes("pair"));
  assert.ok(actions.includes("forget"));
  assert.ok(!actions.includes("ble:scan"));
}

testParseDeviceLines();
testBindingsRoundtrip();
testFormShape();
console.log("esp2-ble-setup.test.mjs: PASS");
