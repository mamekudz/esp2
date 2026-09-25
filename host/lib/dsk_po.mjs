/** DOS 3.3-ish DSK/PO geometry helpers (host). */

export const TRACKS = 35;
export const SECTORS = 16;
export const SECTOR_SIZE = 256;
export const IMAGE_SIZE = TRACKS * SECTORS * SECTOR_SIZE; // 143360

export function createBlankImage(fill = 0) {
  const buf = new Uint8Array(IMAGE_SIZE);
  buf.fill(fill & 0xff);
  return buf;
}

export function sectorOffset(track, sector) {
  if (track < 0 || track >= TRACKS) throw new Error("track OOB");
  if (sector < 0 || sector >= SECTORS) throw new Error("sector OOB");
  return (track * SECTORS + sector) * SECTOR_SIZE;
}

export function readSector(image, track, sector) {
  if (image.length < IMAGE_SIZE) throw new Error("truncated image");
  const off = sectorOffset(track, sector);
  return image.subarray(off, off + SECTOR_SIZE);
}

export function writeSector(image, track, sector, data256) {
  if (image.length < IMAGE_SIZE) throw new Error("truncated image");
  if (data256.length !== SECTOR_SIZE) throw new Error("bad sector size");
  const off = sectorOffset(track, sector);
  image.set(data256, off);
}

export function validateImageSize(n) {
  return n === IMAGE_SIZE;
}
