/**
 * Build a minimal legal synthetic apple2js-style JSON disk (35×16 zero sectors)
 * for converter tests — not Apple IP.
 */
import { writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const zeroSector = Buffer.alloc(256, 0).toString("base64");
const tracks = [];
for (let t = 0; t < 35; t++) {
  const sectors = [];
  for (let s = 0; s < 16; s++) sectors.push(zeroSector);
  tracks.push(sectors);
}

const disk = {
  name: "Synthetic Zero DSK",
  category: "Test",
  type: "dsk",
  encoding: "base64",
  data: tracks,
};

const out = join(
  dirname(fileURLToPath(import.meta.url)),
  "synthetic-zero.dsk.json",
);
writeFileSync(out, JSON.stringify(disk) + "\n");
console.log("wrote", out);
