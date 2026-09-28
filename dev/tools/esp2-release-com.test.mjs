import assert from "node:assert/strict";
import test from "node:test";
import { listSerialHolderCandidates } from "./esp2-release-com.mjs";

test("listSerialHolderCandidates returns an array", () => {
  const rows = listSerialHolderCandidates();
  assert.ok(Array.isArray(rows));
  for (const r of rows) {
    assert.ok(Number.isFinite(r.pid));
    assert.ok(typeof r.cmd === "string");
  }
});
