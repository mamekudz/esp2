import assert from "node:assert/strict";
import {
  mapAppleToPhysical,
  kPhysical,
  kApple,
} from "../lib/viewport.mjs";

export function testViewport() {
  const cfg = {
    physicalDisplay: kPhysical,
    visibleEnclosure: { x: 20, y: 40, w: 240, h: 180 },
    appleLogical: kApple,
    mode: "PortraitApple2Case",
  };
  const tl = mapAppleToPhysical(cfg, 0, 0);
  assert.deepEqual(tl, { x: 20, y: 40 });
  const br = mapAppleToPhysical(cfg, 279, 191);
  assert.ok(br.x >= 20 && br.x < 20 + 240);
  assert.ok(br.y >= 40 && br.y < 40 + 180);

  const mid = mapAppleToPhysical(cfg, 140, 96);
  assert.ok(mid.x > 20 && mid.x < 260);
}
