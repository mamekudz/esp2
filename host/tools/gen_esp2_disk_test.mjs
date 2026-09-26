/**
 * Write fixtures/synthetic/diskii/Esp2DiskTest.dsk (project-owned, no Apple DOS).
 */
import { writeFileSync, mkdirSync } from 'node:fs';
import { join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import { spawnSync } from 'node:child_process';

const root = join(dirname(fileURLToPath(import.meta.url)), '../..');
const outDir = join(root, 'fixtures/synthetic/diskii');
mkdirSync(outDir, { recursive: true });

// Prefer generating from the C++ generator via a small helper compile is heavy;
// embed the same layout: 143360 zeroed + boot sector matching generateEsp2DiskTestImage.
const SIZE = 35 * 16 * 256;
const buf = Buffer.alloc(SIZE, 0);
let j = 0;
const b = (v) => { buf[j++] = v; };
b(0x8d); b(0x51); b(0xc0);
b(0x8d); b(0x54); b(0xc0);
const msg = 'ESP][ DISK BOOT OK';
for (let col = 0; col < msg.length; col++) {
  const ch = 0x80 | msg.charCodeAt(col);
  const addr = 0x0400 + col;
  b(0xa9); b(ch);
  b(0x8d); b(addr & 0xff); b((addr >> 8) & 0xff);
}
b(0xa9); b(0xa5);
b(0x8d); b(0xff); b(0x03);
const loop = 0x0800 + j;
b(0x4c); b(loop & 0xff); b((loop >> 8) & 0xff);
for (let i = 0; i < 256; i++) {
  buf[16 * 256 + i] = i;
  buf[16 * 256 + 256 + i] = 0x00;
  buf[16 * 256 + 512 + i] = 0xff;
}
const out = join(outDir, 'Esp2DiskTest.dsk');
writeFileSync(out, buf);
console.log('Wrote', out, buf.length);
