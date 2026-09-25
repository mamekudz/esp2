import assert from "node:assert/strict";
import {
  createBitBuffer,
  fillPattern,
  countClasses,
  monoLuma,
} from "../lib/artifact_color.mjs";

export function testArtifactColor() {
  const alt = createBitBuffer();
  fillPattern(alt, "alt1010");
  const c0 = countClasses(alt, false);
  assert.ok(c0.purple > 0, "alt1010 should produce purple pairs (phase even)");
  assert.equal(c0.green, 0);

  const alt2 = createBitBuffer();
  fillPattern(alt2, "alt0101");
  const c1 = countClasses(alt2, false);
  assert.ok(c1.green > 0, "alt0101 should produce green pairs");

  const white = createBitBuffer();
  fillPattern(white, "white_run");
  const cw = countClasses(white, false);
  assert.ok(cw.white > 1000);

  const mono = monoLuma(alt);
  assert.equal(mono[0], 255);
  assert.equal(mono[1], 0);
  // Ensure mono came from bits, not a composite RGB pipeline (by construction).
  assert.equal(mono.length, alt.length);
}
