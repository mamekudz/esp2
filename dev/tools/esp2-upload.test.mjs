import assert from "node:assert/strict";
import test from "node:test";
import { sanitizeEsp2Path, MAGIC, DEFAULT_CHUNK } from "./esp2-upload.mjs";

test("sanitizeEsp2Path accepts rom target", () => {
  assert.equal(sanitizeEsp2Path("/esp2/roms/system.rom"), "/esp2/roms/system.rom");
});

test("sanitizeEsp2Path collapses slashes", () => {
  assert.equal(sanitizeEsp2Path("/esp2//roms///a.dsk"), "/esp2/roms/a.dsk");
});

test("sanitizeEsp2Path rejects traversal", () => {
  assert.throws(() => sanitizeEsp2Path("/esp2/../etc/passwd"));
  assert.throws(() => sanitizeEsp2Path("esp2/roms/x"));
  assert.throws(() => sanitizeEsp2Path("/tmp/x"));
  assert.throws(() => sanitizeEsp2Path("/esp2/"));
});

test("protocol constants", () => {
  assert.equal(MAGIC, 0x55505345);
  assert.equal(DEFAULT_CHUNK, 256);
});
