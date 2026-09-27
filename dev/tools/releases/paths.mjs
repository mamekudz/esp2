// ===========================================
// paths.mjs — BlueShift RELEASES locations
// ===========================================

import { join } from "node:path";

/**
 * @param {string} _root
 */
export function ReleasesPaths(_root) {
  return {
    releasesPath: join(_root, "RELEASES.json"),
    developerDir: join(_root, "dev", "releases"),
  };
}
