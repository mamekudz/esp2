/**
 * Little Brick Out is NOT on the Apple ][js website catalog.
 * Source: Cowgod Apple II Cassette Software (DSK.gz) — LOCAL_TEST_ONLY.
 *
 * Cowgod HELLO boots Color Demos leftover; we apply a small HELLO patch so
 * boot runs LITTLE BRICK OUT (matches local forensic runtime image).
 */
import { createHash } from "node:crypto";
import { createGunzip } from "node:zlib";
import { pipeline } from "node:stream/promises";
import { createWriteStream, existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { Readable } from "node:stream";
import {
  ESP2_ROOT,
  LOCAL_CACHE_DIR,
  LOCAL_DISKS_DIR,
  ensureLocalApple2Layout,
} from "./paths.mjs";
import { assertDestinationIgnored } from "./git-safety.mjs";

export const LITTLE_BRICK_OUT_URL =
  "https://cowgod.org/apple2/tapes/little-brick-out.dsk.gz";

/** Unpatched Cowgod DSK after gunzip. */
export const LITTLE_BRICK_OUT_ORIGINAL_SHA256 =
  "2f9b78ee27f5d0167a2a7ead2193607a0ded6ef1e90406b30726483c62307f2f";

/** Runtime image after HELLO → RUN LITTLE BRICK OUT. */
export const LITTLE_BRICK_OUT_RUNTIME_SHA256 =
  "50502098248f122d02fd5ab9779b8e94be1f040803fde2913f9f31f718eea505";

const HELLO_PATCH_OFFSET = 69120;
/** Contiguous HELLO rewrite (135 bytes) — verified against forensic runtime. */
const HELLO_PATCH_B64 =
  "IwAhCAoAuucoNCk7IlJVTiBMSVRUTEUgQlJJQ0sgT1VUIgAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";

const DEVICE_PATH = "/esp2/disks/LittleBrickOut.dsk";

function sha256(buf) {
  return createHash("sha256").update(buf).digest("hex");
}

async function fetchBinary(url, { timeoutMs = 90000 } = {}) {
  const ctrl = new AbortController();
  const t = setTimeout(() => ctrl.abort(), timeoutMs);
  try {
    const res = await fetch(url, { signal: ctrl.signal });
    if (!res.ok) {
      throw new Error(`[LBO] HTTP ${res.status} for ${url}`);
    }
    return Buffer.from(await res.arrayBuffer());
  } finally {
    clearTimeout(t);
  }
}

/**
 * Download Cowgod Little Brick Out, gunzip, apply HELLO patch.
 * @param {{ log?: Function, force?: boolean }} [opts]
 */
export async function syncLittleBrickOut(opts = {}) {
  const log = opts.log ?? console.log;
  ensureLocalApple2Layout();
  assertDestinationIgnored(LOCAL_DISKS_DIR);

  const cowgodCache = join(LOCAL_CACHE_DIR, "cowgod-tapes");
  mkdirSync(cowgodCache, { recursive: true });
  const gzPath = join(cowgodCache, "little-brick-out.dsk.gz");
  const originalPath = join(LOCAL_DISKS_DIR, "LittleBrickOut.cowgod-original.dsk");
  const runtimePath = join(LOCAL_DISKS_DIR, "LittleBrickOut.dsk");

  if (
    !opts.force &&
    existsSync(runtimePath) &&
    sha256(readFileSync(runtimePath)) === LITTLE_BRICK_OUT_RUNTIME_SHA256
  ) {
    log(
      `[LBO] OK already present sha256=${LITTLE_BRICK_OUT_RUNTIME_SHA256.slice(0, 12)}… provenance=USER_SUPPLIED_ONLY`,
    );
    return {
      ok: true,
      skipped: true,
      runtimePath,
      devicePath: DEVICE_PATH,
      runtimeSha256: LITTLE_BRICK_OUT_RUNTIME_SHA256,
    };
  }

  let gzBuf;
  if (!opts.force && existsSync(gzPath)) {
    gzBuf = readFileSync(gzPath);
    log(`[LBO] using cached ${gzPath}`);
  } else {
    log(`[LBO] fetching url=${LITTLE_BRICK_OUT_URL}`);
    gzBuf = await fetchBinary(LITTLE_BRICK_OUT_URL);
    writeFileSync(gzPath, gzBuf);
  }

  const tmpRaw = join(cowgodCache, "little-brick-out.raw.tmp");
  await pipeline(Readable.from(gzBuf), createGunzip(), createWriteStream(tmpRaw));
  const raw = readFileSync(tmpRaw);
  const rawHash = sha256(raw);
  if (rawHash !== LITTLE_BRICK_OUT_ORIGINAL_SHA256) {
    throw new Error(
      `[LBO] unexpected Cowgod sha256 got=${rawHash} expected=${LITTLE_BRICK_OUT_ORIGINAL_SHA256}`,
    );
  }
  writeFileSync(originalPath, raw);

  const patched = Buffer.from(raw);
  const patch = Buffer.from(HELLO_PATCH_B64, "base64");
  if (HELLO_PATCH_OFFSET + patch.length > patched.length) {
    throw new Error("[LBO] HELLO patch out of range");
  }
  patched.set(patch, HELLO_PATCH_OFFSET);
  const runtimeHash = sha256(patched);
  if (runtimeHash !== LITTLE_BRICK_OUT_RUNTIME_SHA256) {
    throw new Error(
      `[LBO] HELLO patch sha mismatch got=${runtimeHash} expected=${LITTLE_BRICK_OUT_RUNTIME_SHA256}`,
    );
  }
  writeFileSync(runtimePath, patched);

  log(
    `[LBO] OK runtime=${runtimePath} size=${patched.length} sha256=${runtimeHash.slice(0, 12)}… provenance=USER_SUPPLIED_ONLY source=cowgod`,
  );

  return {
    ok: true,
    skipped: false,
    runtimePath,
    originalPath,
    devicePath: DEVICE_PATH,
    runtimeSha256: runtimeHash,
    root: ESP2_ROOT,
  };
}
