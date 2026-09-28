import assert from "node:assert/strict";
import test from "node:test";
import {
  sanitizeEsp2Path,
  MAGIC,
  DEFAULT_CHUNK,
  parseMediaUploadProgressLine,
} from "./esp2-upload.mjs";
import { parsePioUploadProgress } from "./pio.mjs";

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

test("parseMediaUploadProgressLine", () => {
  assert.equal(
    parseMediaUploadProgressLine("PROGRESS offset=128 total=1024 pct=12.5 seq=4"),
    0.125,
  );
  assert.equal(parseMediaUploadProgressLine("PROGRESS offset=1024 total=1024 pct=100.0"), 1);
  assert.equal(parseMediaUploadProgressLine("#ACK DATA seq=1"), null);
});

test("parsePioUploadProgress", () => {
  assert.equal(parsePioUploadProgress("Writing at 0x00010000... (42 %)"), 0.42);
  assert.ok(
    Math.abs(
      parsePioUploadProgress(
        "Writing at 0x00010000 [==============================] 100.0% 16384/16384 bytes...",
      ) - 1,
    ) < 0.001,
  );
  assert.equal(parsePioUploadProgress("Hash of data verified."), null);
});
