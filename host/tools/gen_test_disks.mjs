/**
 * Deterministic synthetic Apple II floppy images for host tests.
 * Project-owned; no Apple IP / commercial software.
 */

import { createHash } from "node:crypto";
import { mkdirSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

export const DOS33_BYTES = 143360;

/**
 * @param {"zero"|"sector_id"|"speaker_ramp"|"multidisk_marker"} kind
 * @param {{ diskIndex?: number }} [opts]
 */
export function generateSyntheticDisk(kind = "sector_id", opts = {}) {
  const buf = Buffer.alloc(DOS33_BYTES, 0);
  const diskIndex = opts.diskIndex ?? 1;
  if (kind === "zero") return buf;

  for (let t = 0; t < 35; t++) {
    for (let s = 0; s < 16; s++) {
      const off = (t * 16 + s) * 256;
      if (kind === "sector_id" || kind === "multidisk_marker") {
        buf[off] = t & 0xff;
        buf[off + 1] = s & 0xff;
        buf[off + 2] = 0xe5; // ESP][ marker nibble family
        buf[off + 3] = 0x50; // 'P'
        buf[off + 4] = 0x32; // '2'
        buf[off + 5] = diskIndex & 0xff;
        // Fill rest with a stable pattern
        for (let i = 6; i < 256; i++) {
          buf[off + i] = (t * 17 + s * 3 + i) & 0xff;
        }
      } else if (kind === "speaker_ramp") {
        for (let i = 0; i < 256; i++) {
          buf[off + i] = (t + s + i) & 0xff;
        }
      }
    }
  }
  return buf;
}

export function sha256Hex(buf) {
  return createHash("sha256").update(buf).digest("hex");
}

export function writeSyntheticDisk(path, kind, opts) {
  mkdirSync(dirname(path), { recursive: true });
  const buf = generateSyntheticDisk(kind, opts);
  writeFileSync(path, buf);
  return { path, bytes: buf.length, sha256: sha256Hex(buf), kind };
}

function main() {
  const root = join(dirname(fileURLToPath(import.meta.url)), "../..");
  const outDir = join(root, "fixtures/synthetic/library/Esp2TestSuite");
  const results = [];
  results.push(
    writeSyntheticDisk(join(outDir, "disk1.dsk.bin"), "multidisk_marker", {
      diskIndex: 1,
    }),
  );
  results.push(
    writeSyntheticDisk(join(outDir, "disk2.dsk.bin"), "multidisk_marker", {
      diskIndex: 2,
    }),
  );
  results.push(
    writeSyntheticDisk(
      join(outDir, "speaker_ramp.dsk.bin"),
      "speaker_ramp",
    ),
  );
  writeFileSync(
    join(outDir, "game.json"),
    JSON.stringify(
      {
        schemaVersion: 1,
        id: "esp2-test-suite",
        title: "ESP][ Synthetic Test Suite",
        disks: [
          {
            file: "disk1.dsk.bin",
            label: "disk1",
            writeProtected: true,
            mediaInstalled: true,
          },
          {
            file: "disk2.dsk.bin",
            label: "disk2",
            writeProtected: true,
            mediaInstalled: true,
          },
        ],
        bootDisk: "disk1.dsk.bin",
        defaultDrive1: "disk1.dsk.bin",
        defaultDrive2: "disk2.dsk.bin",
        mediaStatus: "installed",
        redistribution: {
          status: "REDISTRIBUTABLE",
          evidence: "Project-generated synthetic fixture; no third-party media.",
        },
        notes: "Deterministic sector markers for host/firmware tests.",
      },
      null,
      2,
    ) + "\n",
  );
  console.log(JSON.stringify(results, null, 2));
}

const isCli =
  process.argv[1] &&
  (process.argv[1].endsWith("gen_test_disks.mjs") ||
    process.argv[1].endsWith("gen_test_disks.js"));
if (isCli) {
  main();
}
