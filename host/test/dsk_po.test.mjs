import assert from "node:assert/strict";
import {
  IMAGE_SIZE,
  createBlankImage,
  readSector,
  writeSector,
  validateImageSize,
} from "../lib/dsk_po.mjs";

export function testDskPo() {
  assert.equal(IMAGE_SIZE, 143360);
  assert.equal(validateImageSize(IMAGE_SIZE), true);
  assert.equal(validateImageSize(IMAGE_SIZE - 1), false);

  const img = createBlankImage(0);
  const sector = new Uint8Array(256);
  for (let i = 0; i < 256; i++) sector[i] = i & 0xff;
  writeSector(img, 17, 15, sector);
  const back = readSector(img, 17, 15);
  assert.deepEqual(Array.from(back), Array.from(sector));

  assert.throws(() => readSector(img, 35, 0));
  assert.throws(() => readSector(img, 0, 16));
  assert.throws(() => readSector(new Uint8Array(100), 0, 0));
}
