#!/usr/bin/env node
/**
 * Host OS volume helpers for ESP][ USB MSC sessions.
 *
 * Platform-specific: Windows / macOS (Linux best-effort).
 * Never identify a card by capacity alone.
 */
import { existsSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { platform } from "node:os";

export function hostPlatform() {
  return platform(); // win32 | darwin | linux | …
}

/**
 * @typedef {{
 *   root: string,
 *   letter?: string,
 *   filesystem?: string,
 *   label?: string,
 *   capacityBytes?: number,
 *   freeBytes?: number,
 *   driveType?: string,
 * }} HostVolume
 */

/**
 * @returns {HostVolume[]}
 */
export function detectMountedVolumes() {
  const p = hostPlatform();
  if (p === "win32") return detectWindowsVolumes();
  if (p === "darwin") return detectDarwinVolumes();
  return detectLinuxVolumes();
}

/**
 * @param {string} root
 */
export function openFileManager(root) {
  const abs = String(root || "").trim();
  if (!abs) throw new Error("openFileManager: empty path");
  const p = hostPlatform();
  if (p === "win32") {
    const r = spawnSync("explorer.exe", [abs], { windowsHide: true });
    return { ok: r.error == null, platform: p };
  }
  if (p === "darwin") {
    const r = spawnSync("open", [abs], { encoding: "utf8" });
    return { ok: r.status === 0, platform: p };
  }
  const r = spawnSync("xdg-open", [abs], { encoding: "utf8" });
  return { ok: r.status === 0, platform: p };
}

/**
 * Best-effort safe eject. Returns instructions when automation is unreliable.
 * @param {string} root
 */
export function requestSafeEject(root) {
  const abs = String(root || "").trim();
  const p = hostPlatform();
  if (p === "win32") {
    // Shell.Application Eject is unreliable for USB MSC; prefer explicit user eject.
    return {
      ok: false,
      automated: false,
      platform: p,
      instruction:
        "Eject the ESP][ SD volume in Windows Explorer (Safely Remove / Eject), then continue.",
      root: abs,
    };
  }
  if (p === "darwin") {
    const r = spawnSync("diskutil", ["eject", abs], { encoding: "utf8" });
    if (r.status === 0) {
      return { ok: true, automated: true, platform: p, root: abs };
    }
    return {
      ok: false,
      automated: false,
      platform: p,
      instruction: "Eject the ESP][ SD in Finder, then continue.",
      detail: String(r.stderr || r.stdout || "").trim(),
      root: abs,
    };
  }
  return {
    ok: false,
    automated: false,
    platform: p,
    instruction: "Unmount/eject the ESP][ SD volume, then continue.",
    root: abs,
  };
}

function detectWindowsVolumes() {
  /** @type {HostVolume[]} */
  const out = [];
  const ps = `
Get-Volume | Where-Object { $_.DriveLetter } | ForEach-Object {
  $letter = [string]$_.DriveLetter
  $free = $null; $used = $null
  $d = Get-PSDrive -Name $letter -ErrorAction SilentlyContinue
  if ($d) { $free = [int64]$d.Free; $used = [int64]$d.Used }
  $cap = if ($_.Size) { [int64]$_.Size } elseif ($free -ne $null -and $used -ne $null) { $free + $used } else { $null }
  $type = try { [string]$_.DriveType } catch { "" }
  Write-Output ("letter=$letter|fs=$($_.FileSystem)|label=$($_.FileSystemLabel)|size=$cap|free=$free|type=$type")
}
`;
  const r = spawnSync(
    "powershell.exe",
    ["-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", ps],
    { encoding: "utf8", windowsHide: true },
  );
  for (const line of String(r.stdout || "").split(/\r?\n/)) {
    const t = line.trim();
    if (!t.startsWith("letter=")) continue;
    const parts = Object.fromEntries(
      t.split("|").map((p) => {
        const i = p.indexOf("=");
        return i >= 0 ? [p.slice(0, i), p.slice(i + 1)] : [p, ""];
      }),
    );
    const letter = String(parts.letter || "").trim();
    if (!letter) continue;
    out.push({
      root: `${letter.toUpperCase()}:\\`,
      letter: letter.toUpperCase(),
      filesystem: parts.fs || undefined,
      label: parts.label || undefined,
      capacityBytes: parts.size ? Number(parts.size) : undefined,
      freeBytes: parts.free ? Number(parts.free) : undefined,
      driveType: parts.type || undefined,
    });
  }
  return out;
}

function detectDarwinVolumes() {
  /** @type {HostVolume[]} */
  const out = [];
  const base = "/Volumes";
  if (!existsSync(base)) return out;
  const r = spawnSync("ls", ["-1", base], { encoding: "utf8" });
  for (const name of String(r.stdout || "").split(/\r?\n/)) {
    const n = name.trim();
    if (!n || n === "Macintosh HD") continue;
    const root = `${base}/${n}`;
    if (!existsSync(root)) continue;
    out.push({ root, label: n });
  }
  return out;
}

function detectLinuxVolumes() {
  /** @type {HostVolume[]} */
  const out = [];
  for (const base of ["/media", "/run/media"]) {
    if (!existsSync(base)) continue;
    const r = spawnSync("find", [base, "-mindepth", "2", "-maxdepth", "2", "-type", "d"], {
      encoding: "utf8",
    });
    for (const line of String(r.stdout || "").split(/\r?\n/)) {
      const root = line.trim();
      if (root) out.push({ root });
    }
  }
  return out;
}
