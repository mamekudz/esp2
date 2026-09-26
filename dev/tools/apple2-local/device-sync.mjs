/**
 * apple2:device:sync — upload selected runtime ROM + disk via serial ESPU.
 * Never uploads the complete catalog cache.
 */

import { spawnSync } from "node:child_process";
import { existsSync } from "node:fs";
import { join } from "node:path";
import {
  ESP2_ROOT,
  loadLocalLibraryManifest,
  saveLocalLibraryManifest,
} from "./paths.mjs";
import { resolveLocalRom } from "./rom-sync.mjs";

/**
 * @param {{ title: string, port: string, romId?: string, log?: Function }} opts
 */
export function deviceSyncTitle(opts) {
  const log = opts.log ?? console.log;
  const title = String(opts.title || "")
    .trim()
    .toLowerCase();
  const port = opts.port;
  if (!port) {
    throw new Error("apple2:device:sync requires --port COMx");
  }
  if (!title) {
    throw new Error("apple2:device:sync requires --title <id>");
  }

  const manifest = loadLocalLibraryManifest();
  const rec =
    manifest.titles[title] ||
    Object.values(manifest.titles || {}).find(
      (t) =>
        t.id === title || String(t.title || "").toLowerCase() === title,
    );
  if (!rec) {
    throw new Error(
      `[DEVICE] unknown title=${title}. Sync+prepare first.`,
    );
  }
  if (!rec.runtimePath || !existsSync(rec.runtimePath)) {
    throw new Error(
      `[DEVICE] runtime not prepared for ${rec.id}. Run: gulp apple2:media:prepare --title ${rec.id}`,
    );
  }

  const romId = opts.romId || rec.defaultRomId || "appleiigo";
  const rom = resolveLocalRom(romId);
  if (!rom?.path || !existsSync(rom.path)) {
    throw new Error(
      `[DEVICE] ROM id=${romId} missing. Run: gulp apple2:rom:sync`,
    );
  }

  const uploads = [
    {
      file: rom.path,
      target: rom.devicePath || "/esp2/roms/appleiigo.rom",
      sha256: rom.sha256,
      size: rom.size,
      kind: "rom",
    },
    {
      file: rec.runtimePath,
      target: rec.devicePath || `/esp2/disks/${rec.deviceDiskName}`,
      sha256: rec.runtimeSha256,
      size: rec.runtimeSize,
      kind: "disk",
    },
  ];

  const results = [];
  for (const u of uploads) {
    log(`[DEVICE] upload ${u.kind} ${u.file} -> ${u.target} port=${port}`);
    const r = spawnSync(
      process.execPath,
      [
        join(ESP2_ROOT, "dev/tools/esp2-upload.mjs"),
        "--port",
        port,
        "--listen-ms",
        "20000",
        "--chunk",
        "256",
        "--file",
        u.file,
        "--target",
        u.target,
      ],
      { cwd: ESP2_ROOT, encoding: "utf8", windowsHide: true },
    );
    if (r.status !== 0) {
      throw new Error(
        `[DEVICE][FAIL] upload ${u.target}: ${r.stderr || r.stdout || r.status}`,
      );
    }
    log(`[DEVICE] verify ${u.target}`);
    const v = spawnSync(
      process.execPath,
      [
        join(ESP2_ROOT, "dev/tools/esp2-upload.mjs"),
        "--port",
        port,
        "--listen-ms",
        "20000",
        "--verify",
        u.target,
      ],
      { cwd: ESP2_ROOT, encoding: "utf8", windowsHide: true },
    );
    const out = `${v.stdout || ""}\n${v.stderr || ""}`;
    if (v.status !== 0) {
      throw new Error(`[DEVICE][FAIL] verify ${u.target}: ${out}`);
    }
    // Best-effort parse of size/sha from uploader output
    const shaMatch = out.match(/sha256[=:\s]+([0-9a-f]{64})/i);
    const sizeMatch = out.match(/size[=:\s]+(\d+)/i);
    const deviceSha = shaMatch ? shaMatch[1].toLowerCase() : null;
    const deviceSize = sizeMatch ? Number(sizeMatch[1]) : null;
    if (deviceSha && u.sha256 && deviceSha !== u.sha256) {
      throw new Error(
        `[DEVICE][FAIL] sha256 mismatch ${u.target} device=${deviceSha} local=${u.sha256}`,
      );
    }
    if (deviceSize != null && u.size != null && deviceSize !== u.size) {
      throw new Error(
        `[DEVICE][FAIL] size mismatch ${u.target} device=${deviceSize} local=${u.size}`,
      );
    }
    results.push({
      ...u,
      deviceSha256: deviceSha || u.sha256,
      deviceSize: deviceSize ?? u.size,
      verifyOk: true,
    });
    log(
      `[DEVICE] OK ${u.target} size=${deviceSize ?? u.size} sha256=${(deviceSha || u.sha256 || "").slice(0, 12)}…`,
    );
  }

  rec.deviceSyncedAt = new Date().toISOString();
  manifest.titles[rec.id] = rec;
  if (manifest.roms[romId]) {
    manifest.roms[romId].deviceSyncedAt = rec.deviceSyncedAt;
  }
  saveLocalLibraryManifest(manifest);

  return { ok: true, title: rec.id, uploads: results };
}
