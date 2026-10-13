import assert from "node:assert/strict";
import test from "node:test";
import {
  parseIdentAck,
  parseSdInfoAck,
  classifyUsbPort,
  selectEsp2Candidates,
  sameComPort,
  formatDeviceLabel,
  hintPortFromEnvAndForm,
  resolveEsp2Device,
  ESPRESSIF_VID,
  PRODUCT_ESP2,
} from "./esp2-device-resolver.mjs";
import { PLATFORM_WAVESHARE } from "./esp2-platforms.mjs";

test("parseIdentAck requires ESP][ product", () => {
  const id = parseIdentAck(
    "#ACK IDENT product=ESP][ platform=waveshare-amoled-s3 build=apple2_standalone_v1 msc=0 usb=HW_CDC",
  );
  assert.equal(id.product, PRODUCT_ESP2);
  assert.equal(id.platform, PLATFORM_WAVESHARE);
  assert.equal(id.msc, false);
  assert.equal(parseIdentAck("hello"), null);
});

test("parseSdInfoAck", () => {
  const i = parseSdInfoAck(
    "#ACK SDINFO present=unknown fs=none size_mb=0 esp2=0 owner=ESP2_OWNS_SD msc=0",
  );
  assert.equal(i.fs, "none");
  assert.equal(i.esp2, "0");
});

test("classifyUsbPort skips Bluetooth; keeps Espressif 303A", () => {
  const bt = classifyUsbPort({
    path: "COM10",
    pnpId: "BTHENUM\\{...}",
    vendorId: "0000",
  });
  assert.equal(bt.skip, true);
  const ws = classifyUsbPort({
    path: "COM5",
    vendorId: "303A",
    productId: "1001",
    serialNumber: "AABB",
    manufacturer: "Espressif",
  });
  assert.equal(ws.skip, false);
  assert.equal(ws.espressif, true);
  assert.equal(ws.vid, ESPRESSIF_VID);
});

test("sameComPort ignores case", () => {
  assert.equal(sameComPort("COM5", "com5"), true);
});

test("hintPortFromEnvAndForm prefers form over env (still only a hint)", () => {
  assert.equal(
    hintPortFromEnvAndForm({ formPort: "COM7", envPort: "COM5" }),
    "COM7",
  );
  assert.equal(hintPortFromEnvAndForm({ envPort: "COM5" }), "COM5");
});

function cand(port, platform, extra = {}) {
  return {
    port,
    ident: { product: PRODUCT_ESP2, platform, build: "t" },
    occupied: false,
    usb: { vid: ESPRESSIF_VID, pid: "1001", path: port },
    ...extra,
  };
}

test("matrix: only Waveshare (ESP][ IDENT) → auto", () => {
  const r = selectEsp2Candidates([cand("COM5", PLATFORM_WAVESHARE)], {
    expectedPlatform: PLATFORM_WAVESHARE,
    hintPort: "COM99",
  });
  assert.equal(r.kind, "auto");
  assert.equal(r.devices[0].port, "COM5");
});

test("matrix: only BlueShift (no IDENT) → none", () => {
  const r = selectEsp2Candidates(
    [
      {
        port: "COM5",
        ident: null,
        occupied: false,
        usb: { vid: ESPRESSIF_VID, pid: "1001" },
      },
    ],
    { expectedPlatform: PLATFORM_WAVESHARE, hintPort: "COM5" },
  );
  assert.equal(r.kind, "none");
});

test("matrix: Waveshare + BlueShift → Waveshare only, ignore stale COM5 hint on BlueShift", () => {
  const r = selectEsp2Candidates(
    [
      {
        port: "COM5",
        ident: null,
        usb: { vid: ESPRESSIF_VID },
        occupied: false,
      },
      cand("COM12", PLATFORM_WAVESHARE),
    ],
    { expectedPlatform: PLATFORM_WAVESHARE, hintPort: "COM5" },
  );
  assert.equal(r.kind, "auto");
  assert.equal(r.devices[0].port, "COM12");
});

test("matrix: two ESP][ → ambiguous, do not guess", () => {
  const r = selectEsp2Candidates(
    [cand("COM5", PLATFORM_WAVESHARE), cand("COM8", PLATFORM_WAVESHARE)],
    { expectedPlatform: PLATFORM_WAVESHARE },
  );
  assert.equal(r.kind, "ambiguous");
});

test("matrix: no device → none", () => {
  const r = selectEsp2Candidates([], { expectedPlatform: PLATFORM_WAVESHARE });
  assert.equal(r.kind, "none");
});

test("changed COM: hint COM5 ignored when device now on COM12", () => {
  const r = selectEsp2Candidates([cand("COM12", PLATFORM_WAVESHARE)], {
    expectedPlatform: PLATFORM_WAVESHARE,
    hintPort: "COM5",
  });
  assert.equal(r.kind, "auto");
  assert.equal(r.devices[0].port, "COM12");
});

test("verified hint still wins among multiple ESP][", () => {
  const r = selectEsp2Candidates(
    [cand("COM5", PLATFORM_WAVESHARE), cand("COM8", PLATFORM_WAVESHARE)],
    { expectedPlatform: PLATFORM_WAVESHARE, hintPort: "COM8" },
  );
  assert.equal(r.kind, "auto");
  assert.equal(r.devices[0].port, "COM8");
  assert.equal(r.reason, "hint_verified");
});

test("formatDeviceLabel does not treat COM as identity", () => {
  const label = formatDeviceLabel({
    port: "COM5",
    ident: { platform: PLATFORM_WAVESHARE, build: "apple2_standalone_v1" },
    usb: { serialNumber: "X1" },
  });
  assert.match(label, /Waveshare/);
  assert.match(label, /COM5/);
});

test("resolveEsp2Device: zero ports fail clearly", async () => {
  await assert.rejects(
    () =>
      resolveEsp2Device({
        expectedPlatform: PLATFORM_WAVESHARE,
        listPorts: async () => [],
        probeIdent: async () => ({ ident: null }),
      }),
    /No compatible ESP]\[ device detected/,
  );
});

test("resolveEsp2Device: BlueShift-only does not auto-select", async () => {
  await assert.rejects(
    () =>
      resolveEsp2Device({
        expectedPlatform: PLATFORM_WAVESHARE,
        hintPort: "COM5",
        listPorts: async () => [
          { path: "COM5", vendorId: "303A", productId: "1001" },
        ],
        probeIdent: async () => ({ ident: null }),
      }),
    /No compatible ESP]\[ device detected/,
  );
});

test("resolveEsp2Device: one ESP][ auto even if ESP2_PORT is wrong", async () => {
  const d = await resolveEsp2Device({
    expectedPlatform: PLATFORM_WAVESHARE,
    hintPort: "COM5",
    listPorts: async () => [
      { path: "COM12", vendorId: "303A", productId: "1001", serialNumber: "S" },
    ],
    probeIdent: async () => ({
      ident: {
        product: PRODUCT_ESP2,
        platform: PLATFORM_WAVESHARE,
        build: "t",
        msc: false,
        usb: "HW_CDC",
      },
    }),
  });
  assert.equal(d.port, "COM12");
  assert.equal(d.source, "single_match");
  assert.equal(d.identity.product, PRODUCT_ESP2);
});

test("resolveEsp2Device: two ESP][ require pick", async () => {
  const d = await resolveEsp2Device({
    expectedPlatform: PLATFORM_WAVESHARE,
    listPorts: async () => [
      { path: "COM5", vendorId: "303A", productId: "1001" },
      { path: "COM8", vendorId: "303A", productId: "1001" },
    ],
    probeIdent: async (path) => ({
      ident: {
        product: PRODUCT_ESP2,
        platform: PLATFORM_WAVESHARE,
        build: "t",
        msc: false,
        usb: "HW_CDC",
      },
    }),
    requestPick: async (cands) => {
      assert.equal(cands.length, 2);
      return "COM8";
    },
  });
  assert.equal(d.port, "COM8");
  assert.equal(d.source, "user_pick");
});
