/**
 * apple2:media:prepare — convert cached apple2js JSON to ESP][ runtime image.
 * Offline: uses already-downloaded local source only.
 */

import { existsSync, writeFileSync, readFileSync } from "node:fs";
import { join } from "node:path";
import {
  jsonDiskToFlatImage,
  parseJsonDiskMeta,
  sha256Hex,
  isStandardDos33Size,
} from "../apple2js/json_disk.mjs";
import {
  LOCAL_DISKS_DIR,
  ensureLocalApple2Layout,
  loadLocalLibraryManifest,
  saveLocalLibraryManifest,
  retainProvenance,
} from "./paths.mjs";
import { assertDestinationIgnored } from "./git-safety.mjs";

/**
 * Prepare runtime disk for a title from local cache.
 * @param {string} titleId
 * @param {{ log?: Function }} [opts]
 */
export function prepareTitleMedia(titleId, opts = {}) {
  const log = opts.log ?? console.log;
  ensureLocalApple2Layout();
  assertDestinationIgnored(LOCAL_DISKS_DIR);

  const q = String(titleId || "")
    .trim()
    .toLowerCase();
  const manifest = loadLocalLibraryManifest();
  const rec =
    manifest.titles[q] ||
    Object.values(manifest.titles || {}).find(
      (t) =>
        t.id === q ||
        String(t.title || "").toLowerCase() === q ||
        (t.sourceRelative || "").includes(q),
    );

  if (!rec) {
    throw new Error(
      `[PREPARE] no local cache for title=${titleId}. Run: gulp apple2:media:sync --title ${titleId}`,
    );
  }
  if (!rec.sourcePath || !existsSync(rec.sourcePath)) {
    throw new Error(
      `[PREPARE] missing source file for ${rec.id}: ${rec.sourcePath || "(none)"}`,
    );
  }

  const json = JSON.parse(readFileSync(rec.sourcePath, "utf8"));
  const meta = parseJsonDiskMeta(json);
  const type = meta.type;

  let runtimeBytes = null;
  let runtimeFormat = null;
  let runtimeExt = null;

  if (type === "dsk" || type === "po" || type === "d13") {
    runtimeBytes = jsonDiskToFlatImage(json);
    if (!isStandardDos33Size(runtimeBytes.length) && type !== "d13") {
      throw new Error(
        `[PREPARE] unexpected flat size ${runtimeBytes.length} for type=${type}`,
      );
    }
    runtimeFormat = type === "po" ? "po" : type === "d13" ? "d13" : "dsk";
    runtimeExt = runtimeFormat === "po" ? ".po" : ".dsk";
  } else if (type === "nib") {
    // Preserve nibble representation as concatenated track blobs is lossy for
    // our Dos33 path — keep original JSON alongside a .nib only if we can
    // flatten tracks. Prefer not forcing DSK.
    throw new Error(
      `[PREPARE] type=nib requires NIB runtime support; refusing forced DSK conversion for ${rec.id}`,
    );
  } else {
    throw new Error(
      `[PREPARE] unsupported apple2js type=${type} for ${rec.id} (will not force DSK)`,
    );
  }

  const outName =
    rec.deviceDiskName ||
    `${rec.title || rec.id}${runtimeExt}`.replace(/[^\w.\-]+/g, "_");
  const outPath = join(LOCAL_DISKS_DIR, outName);
  writeFileSync(outPath, runtimeBytes);

  const hash = sha256Hex(runtimeBytes);
  const provenance = retainProvenance(rec.provenance);

  rec.runtimePath = outPath;
  rec.runtimeRelative = `local/apple2/disks/${outName}`;
  rec.runtimeFormat = runtimeFormat;
  rec.runtimeSha256 = hash;
  rec.runtimeSize = runtimeBytes.length;
  rec.preparedAt = new Date().toISOString();
  rec.provenance = provenance;
  rec.kind = "generated_runtime";
  if (!rec.devicePath) {
    rec.devicePath = `/esp2/disks/${outName}`;
  }
  if (!rec.deviceDiskName) {
    rec.deviceDiskName = outName;
  }

  manifest.titles[rec.id] = rec;
  saveLocalLibraryManifest(manifest);

  log(
    `[PREPARE] OK title=${rec.id} format=${runtimeFormat} size=${runtimeBytes.length} sha256=${hash.slice(0, 12)}… provenance=${provenance}`,
  );

  return {
    ok: true,
    id: rec.id,
    runtimePath: outPath,
    runtimeFormat,
    runtimeSha256: hash,
    runtimeSize: runtimeBytes.length,
    provenance,
    sourceType: type,
    record: rec,
  };
}
