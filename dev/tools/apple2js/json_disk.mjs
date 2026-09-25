/**
 * apple2js JSON disk representation (host-side).
 *
 * Format (from bin/dsk2json):
 * {
 *   name, category, type: "dsk"|"po"|"nib"|...,
 *   encoding: "base64",
 *   readOnly?, disk?, "2e"?,
 *   data: for dsk/po: track[35][sector 13|16] base64 of 256 bytes
 *         for nib: track[] base64 of 0x1a00 nibbles
 * }
 *
 * Conversion capability ≠ redistribution permission.
 */

import { createHash } from "node:crypto";

const DOS33_BYTES = 35 * 16 * 256; // 143360

/**
 * @param {unknown} json
 */
export function parseJsonDiskMeta(json) {
  if (json == null || typeof json !== "object") {
    throw new Error("json disk: expected object");
  }
  const o = /** @type {Record<string, unknown>} */ (json);
  if (typeof o.name !== "string" || !o.name) {
    throw new Error("json disk: missing name");
  }
  if (typeof o.type !== "string" || !o.type) {
    throw new Error("json disk: missing type");
  }
  if (o.encoding !== "base64") {
    throw new Error(`json disk: unsupported encoding ${o.encoding}`);
  }
  if (!Array.isArray(o.data)) {
    throw new Error("json disk: missing data[]");
  }
  return {
    name: o.name,
    category: typeof o.category === "string" ? o.category : null,
    type: o.type.toLowerCase(),
    encoding: "base64",
    readOnly: Boolean(o.readOnly),
    disk: typeof o.disk === "string" ? o.disk : null,
    apple2e: Boolean(o["2e"]),
    private: Boolean(o.private),
    trackCount: o.data.length,
  };
}

/**
 * Convert dsk/po JSON disk (35×16×256) back to a Uint8Array image.
 * @param {object} json
 * @returns {Uint8Array}
 */
export function jsonDiskToFlatImage(json) {
  const meta = parseJsonDiskMeta(json);
  if (meta.type !== "dsk" && meta.type !== "po" && meta.type !== "d13") {
    throw new Error(
      `jsonDiskToFlatImage: type ${meta.type} not supported (host prototype supports dsk/po/d13)`,
    );
  }
  const sectorsPerTrack = meta.type === "d13" ? 13 : 16;
  const expected = 35 * sectorsPerTrack * 256;
  const out = new Uint8Array(expected);
  const data = json.data;
  if (!Array.isArray(data) || data.length !== 35) {
    throw new Error(`expected 35 tracks, got ${data?.length}`);
  }
  let offset = 0;
  for (let t = 0; t < 35; t++) {
    const track = data[t];
    if (!Array.isArray(track) || track.length !== sectorsPerTrack) {
      throw new Error(
        `track ${t}: expected ${sectorsPerTrack} sectors, got ${track?.length}`,
      );
    }
    for (let s = 0; s < sectorsPerTrack; s++) {
      const b64 = track[s];
      if (typeof b64 !== "string") {
        throw new Error(`track ${t} sector ${s}: expected base64 string`);
      }
      const buf = Buffer.from(b64, "base64");
      if (buf.length !== 256) {
        throw new Error(
          `track ${t} sector ${s}: expected 256 bytes, got ${buf.length}`,
        );
      }
      out.set(buf, offset);
      offset += 256;
    }
  }
  if (offset !== expected) {
    throw new Error(`internal size mismatch ${offset} vs ${expected}`);
  }
  return out;
}

export function sha256Hex(bytes) {
  return createHash("sha256").update(bytes).digest("hex");
}

export function isStandardDos33Size(byteLength) {
  return byteLength === DOS33_BYTES;
}

export { DOS33_BYTES };
