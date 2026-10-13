/**
 * Host unit tests: SD ownership map + flash guard.
 */
import test from "node:test";
import assert from "node:assert/strict";
import {
  SD_OWNER,
  mapFirmwareOwnerState,
  flashAllowedForOwner,
  flashBlockedMessage,
  parseSdOwnerAck,
  assertFlashSdSafe,
  isValidOwnerTransition,
} from "./esp2-sd-flash-guard.mjs";

test("mapFirmwareOwnerState covers firmware enum", () => {
  assert.equal(mapFirmwareOwnerState("ESP2_OWNS_SD"), SD_OWNER.DEVICE);
  assert.equal(mapFirmwareOwnerState("USB_OWNS_SD"), SD_OWNER.HOST);
  assert.equal(mapFirmwareOwnerState("TRANSITION_TO_USB"), SD_OWNER.TO_HOST);
  assert.equal(mapFirmwareOwnerState("TRANSITION_TO_ESP2"), SD_OWNER.TO_DEVICE);
  assert.equal(mapFirmwareOwnerState("ERROR"), SD_OWNER.ERROR);
  assert.equal(mapFirmwareOwnerState("nope"), SD_OWNER.UNKNOWN);
});

test("flash allowed only for DEVICE/UNKNOWN", () => {
  assert.equal(flashAllowedForOwner(SD_OWNER.DEVICE), true);
  assert.equal(flashAllowedForOwner(SD_OWNER.UNKNOWN), true);
  assert.equal(flashAllowedForOwner(SD_OWNER.HOST), false);
  assert.equal(flashAllowedForOwner(SD_OWNER.TO_HOST), false);
});

test("parseSdOwnerAck", () => {
  const p = parseSdOwnerAck("#ACK SDOWNER state=USB_OWNS_SD msc=1");
  assert.equal(p.owner, SD_OWNER.HOST);
  assert.equal(p.msc, true);
  assert.equal(parseSdOwnerAck("noise"), null);
});

test("assertFlashSdSafe aborts when HOST", () => {
  assert.throws(
    () =>
      assertFlashSdSafe("/proj", "COM9", {
        query: () => ({ owner: SD_OWNER.HOST }),
      }),
    /mounted on the computer/i,
  );
});

test("assertFlashSdSafe can recover via tryReturn", () => {
  let n = 0;
  const r = assertFlashSdSafe("/proj", "COM9", {
    query: () => {
      n += 1;
      return { owner: n === 1 ? SD_OWNER.HOST : SD_OWNER.DEVICE };
    },
    tryReturn: () => true,
  });
  assert.equal(r.ok, true);
  assert.equal(r.owner, SD_OWNER.DEVICE);
});

test("ownership transitions", () => {
  assert.equal(isValidOwnerTransition(SD_OWNER.DEVICE, SD_OWNER.TO_HOST), true);
  assert.equal(isValidOwnerTransition(SD_OWNER.TO_HOST, SD_OWNER.HOST), true);
  assert.equal(isValidOwnerTransition(SD_OWNER.HOST, SD_OWNER.TO_DEVICE), true);
  assert.equal(isValidOwnerTransition(SD_OWNER.TO_DEVICE, SD_OWNER.DEVICE), true);
  assert.equal(isValidOwnerTransition(SD_OWNER.DEVICE, SD_OWNER.HOST), false);
  assert.equal(isValidOwnerTransition(SD_OWNER.HOST, SD_OWNER.DEVICE), false);
});

test("flashBlockedMessage is user-facing", () => {
  assert.match(flashBlockedMessage(SD_OWNER.HOST), /Unmount/i);
  assert.doesNotMatch(flashBlockedMessage(SD_OWNER.HOST), /USB_OWNS_SD/);
});
