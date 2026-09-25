import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { createBlankImage, IMAGE_SIZE } from "../lib/dsk_po.mjs";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../..");
const out = path.join(
  root,
  "fixtures/synthetic/library/DemoCart/disk1.dsk.bin",
);
fs.mkdirSync(path.dirname(out), { recursive: true });
const img = createBlankImage(0);
// Mark track 0 sector 0 with a tiny signature for tests (not Apple DOS).
img[0] = 0x45; // 'E'
img[1] = 0x53; // 'S'
img[2] = 0x50; // 'P'
img[3] = 0x5d; // ']'
img[4] = 0x5b; // '['
fs.writeFileSync(out, img);
console.log(`wrote ${out} (${IMAGE_SIZE} bytes)`);
