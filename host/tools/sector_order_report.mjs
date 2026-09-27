/**
 * HOST forensic helper: print DOS 3.3 / Disk II sector-order metadata.
 *
 * Usage:
 *   node host/tools/sector_order_report.mjs
 *   node host/tools/sector_order_report.mjs path/to/image.dsk
 *   node host/tools/sector_order_report.mjs path/to/image.po --po
 *
 * Prints mapping tables and optional per-sector SHA-256 prefixes.
 * Never dumps copyrighted sector payloads.
 */
import { createHash } from "node:crypto";
import { readFileSync } from "node:fs";
import { resolve } from "node:path";

/** index = physical / Address Field ID; value = DOS logical = DSK/DO file slot */
const DO = Object.freeze([
  0x0, 0x7, 0xe, 0x6, 0xd, 0x5, 0xc, 0x4, 0xb, 0x3, 0xa, 0x2, 0x9, 0x1, 0x8, 0xf,
]);

/** index = DOS logical; value = physical / Address Field ID (= apple2js _DO) */
const _DO = Object.freeze([
  0x0, 0xd, 0xb, 0x9, 0x7, 0x5, 0x3, 0x1, 0xe, 0xc, 0xa, 0x8, 0x6, 0x4, 0x2, 0xf,
]);

/** index = PO file slot; value = DOS logical (16-sector floppy) */
const PO_TO_DOS = Object.freeze([
  0x0, 0x8, 0x1, 0x9, 0x2, 0xa, 0x3, 0xb, 0x4, 0xc, 0x5, 0xd, 0x6, 0xe, 0x7, 0xf,
]);

const kImageBytes = 143360;
const kTrackBytes = 4096;

function sha16(buf) {
  return createHash("sha256").update(buf).digest("hex").slice(0, 16);
}

function printCanonicalTable() {
  console.log("PhysicalSlot | AddressID | DOSLogical | DSKSlot | POFileSlot→DOS");
  console.log("-------------|-----------|------------|---------|---------------");
  for (let phys = 0; phys < 16; phys++) {
    const dos = DO[phys];
    const poSlot = PO_TO_DOS.indexOf(dos);
    console.log(
      `${String(phys).padStart(12)} | ${String(phys).padStart(9)} | ${String(dos).padStart(10)} | ${String(dos).padStart(7)} | ${String(poSlot).padStart(5)} → ${dos}`,
    );
  }
  console.log("\n_DO[logical] → AddressID (boot0 / software skew request table):");
  console.log(
    "  " +
      [..._DO]
        .map((v, i) => `L${i.toString(16).toUpperCase()}→$${v.toString(16).toUpperCase().padStart(2, "0")}`)
        .join(" "),
  );
  console.log("\nDefinitions:");
  console.log("  PhysicalSlot / AddressID — rotational sector number in the Address Field (0..15)");
  console.log("  DOSLogical / DSKSlot     — DOS 3.3 logical sector = .dsk/.do file slot in track");
  console.log("  POFileSlot               — .po file slot in track (maps via PO_TO_DOS)");
}

function reportImage(path, poOrder) {
  const raw = readFileSync(path);
  if (raw.length !== kImageBytes) {
    console.error(`Expected ${kImageBytes} bytes, got ${raw.length}`);
    process.exit(1);
  }
  console.log(`\nImage: ${path}`);
  console.log(`  size=${raw.length} sha256=${createHash("sha256").update(raw).digest("hex")}`);
  console.log(`  order=${poOrder ? "PO" : "DSK/DO"}`);
  console.log("  Track0 sector hashes (file slot order):");
  for (let slot = 0; slot < 16; slot++) {
    const off = slot * 256;
    const sec = raw.subarray(off, off + 256);
    const dos = poOrder ? PO_TO_DOS[slot] : slot;
    const addr = _DO[dos];
    console.log(
      `    fileSlot=${String(slot).padStart(2)} dosLogical=${String(dos).padStart(2)} ` +
        `AddressID=$${addr.toString(16).toUpperCase().padStart(2, "0")} sha16=${sha16(sec)}`,
    );
  }
  console.log("  Per-track sha16:");
  for (let t = 0; t < 35; t++) {
    const tr = raw.subarray(t * kTrackBytes, (t + 1) * kTrackBytes);
    console.log(`    T${String(t).padStart(2, "0")} ${sha16(tr)}`);
  }
}

const args = process.argv.slice(2);
const po = args.includes("--po");
const files = args.filter((a) => a !== "--po");

printCanonicalTable();
for (const f of files) {
  reportImage(resolve(f), po);
}
