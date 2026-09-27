// ===========================================
// project-version.mjs — µGulp V<version/> label
// ===========================================

import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { FormatVersionString } from "./releases/release-version.mjs";

/**
 * Newest RELEASES.json version, else package.json version, else "0.0.0".
 * @param {string} _root
 * @returns {string}
 */
export function GetProjectVersionLabel(_root) {
  const releasesPath = join(_root, "RELEASES.json");
  if (existsSync(releasesPath)) {
    try {
      const raw = JSON.parse(readFileSync(releasesPath, "utf8"));
      const first = Array.isArray(raw?.releases) ? raw.releases[0] : null;
      if (first) return FormatVersionString(first);
    } catch {
      /* fall through */
    }
  }
  const pkgPath = join(_root, "package.json");
  if (existsSync(pkgPath)) {
    try {
      const pkg = JSON.parse(readFileSync(pkgPath, "utf8"));
      const v = String(pkg.version ?? "").trim();
      if (/^\d+\.\d+\.\d+/.test(v)) return v.match(/^\d+\.\d+\.\d+/)[0];
    } catch {
      /* fall through */
    }
  }
  return "0.0.0";
}
