/**
 * Pack SingleStepTests/65x02 JSON vectors into a compact binary for the HOST
 * SST runner. Output stays under gitignored local/apple2/forensics/sst/.
 *
 * Usage:
 *   node host/tools/sst_pack.mjs local/apple2/forensics/sst/9c.json
 *   node host/tools/sst_pack.mjs --all
 *
 * Binary "SSTB" layout (little-endian):
 *   magic[4]="SSTB"
 *   opcode_u8, pad[3]
 *   count_u32
 *   repeat count:
 *     init: pc_u16, s_u8, a_u8, x_u8, y_u8, p_u8
 *     final: pc_u16, s_u8, a_u8, x_u8, y_u8, p_u8
 *     n_init_u16, (addr_u16, val_u8)*n_init
 *     n_final_u16, (addr_u16, val_u8)*n_final
 */
import { readFileSync, writeFileSync, existsSync } from "node:fs";
import { dirname, join, basename } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "../..");
const sstDir = join(root, "local/apple2/forensics/sst");

function u16(n) {
  const b = Buffer.alloc(2);
  b.writeUInt16LE(n & 0xffff, 0);
  return b;
}
function u32(n) {
  const b = Buffer.alloc(4);
  b.writeUInt32LE(n >>> 0, 0);
  return b;
}
function u8(n) {
  return Buffer.from([n & 0xff]);
}

function packOne(jsonPath) {
  const text = readFileSync(jsonPath, "utf8");
  const data = JSON.parse(text);
  if (!Array.isArray(data) || data.length === 0) {
    throw new Error(`not an SST array: ${jsonPath}`);
  }
  const opByte = data[0].initial.ram.find((p) => p[0] === data[0].initial.pc)?.[1];
  if (opByte === undefined) {
    throw new Error(`opcode byte missing at PC in ${jsonPath}`);
  }

  const chunks = [];
  chunks.push(Buffer.from("SSTB"));
  chunks.push(u8(opByte));
  chunks.push(Buffer.alloc(3));
  chunks.push(u32(data.length));

  for (const t of data) {
    const ini = t.initial;
    const fin = t.final;
    chunks.push(u16(ini.pc));
    chunks.push(u8(ini.s), u8(ini.a), u8(ini.x), u8(ini.y), u8(ini.p));
    chunks.push(u16(fin.pc));
    chunks.push(u8(fin.s), u8(fin.a), u8(fin.x), u8(fin.y), u8(fin.p));
    const ir = ini.ram || [];
    const fr = fin.ram || [];
    chunks.push(u16(ir.length));
    for (const [addr, val] of ir) {
      chunks.push(u16(addr), u8(val));
    }
    chunks.push(u16(fr.length));
    for (const [addr, val] of fr) {
      chunks.push(u16(addr), u8(val));
    }
  }

  const out = Buffer.concat(chunks);
  const outPath = jsonPath.replace(/\.json$/i, ".sstb");
  writeFileSync(outPath, out);
  console.log(
    `packed ${basename(jsonPath)} -> ${basename(outPath)} vectors=${data.length} opcode=$${opByte
      .toString(16)
      .padStart(2, "0")} bytes=${out.length}`,
  );
  return outPath;
}

const args = process.argv.slice(2);
if (args.length === 0 || args[0] === "--all") {
  for (const op of ["9c", "9e", "9f", "cb"]) {
    const p = join(sstDir, `${op}.json`);
    if (!existsSync(p)) {
      console.error(`missing ${p}`);
      process.exit(1);
    }
    packOne(p);
  }
} else {
  for (const a of args) {
    const p = a.startsWith("/") || /^[A-Za-z]:/.test(a) ? a : join(root, a);
    packOne(p);
  }
}
