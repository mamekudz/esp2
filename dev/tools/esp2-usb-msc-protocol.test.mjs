import assert from "node:assert/strict";
import test from "node:test";
import {
  parseUsbMscLine,
  usbMscHostMessage,
  mscEnterAllowedForCard,
  USB_MSC_ERROR,
} from "./esp2-usb-msc-protocol.mjs";

test("parseUsbMscLine READY / LEFT / ERROR", () => {
  assert.deepEqual(parseUsbMscLine("#ESP2USBMSC READY"), {
    ok: true,
    state: "READY",
  });
  assert.deepEqual(parseUsbMscLine("#ESP2USBMSC LEFT"), {
    ok: true,
    state: "LEFT",
  });
  assert.deepEqual(parseUsbMscLine("#ESP2USBMSC ERROR UNSUPPORTED"), {
    ok: false,
    code: "UNSUPPORTED",
  });
  assert.equal(parseUsbMscLine("[USB] MSC unsupported"), null);
});

test("usbMscHostMessage UNSUPPORTED is immediate and not a timeout", () => {
  const msg = usbMscHostMessage(USB_MSC_ERROR.UNSUPPORTED);
  assert.match(msg, /hardware CDC/i);
  assert.doesNotMatch(msg, /timeout/i);
});

test("empty card with block device may enter MSC; missing card may not", () => {
  assert.equal(mscEnterAllowedForCard({ cardPresent: true, fatMounted: false, hasEsp2Tree: false }), true);
  assert.equal(mscEnterAllowedForCard({ cardPresent: false, fatMounted: false }), false);
});
