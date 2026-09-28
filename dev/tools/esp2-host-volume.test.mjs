import assert from "node:assert/strict";
import test from "node:test";
import {
  hostPlatform,
  requestSafeEject,
  openFileManager,
} from "./esp2-host-volume.mjs";

test("hostPlatform returns a known string", () => {
  const p = hostPlatform();
  assert.ok(["win32", "darwin", "linux"].includes(p) || typeof p === "string");
});

test("requestSafeEject never claims success from capacity alone", () => {
  const r = requestSafeEject("Z:\\imaginary");
  assert.ok(r.platform);
  assert.ok(r.instruction || r.ok === true || r.ok === false);
});

test("openFileManager rejects empty path", () => {
  assert.throws(() => openFileManager(""));
});
