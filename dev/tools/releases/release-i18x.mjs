// ===========================================
// release-i18x.mjs — extract / completeness (no auto AI translate)
// ===========================================

import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ListReleaseSourceBodies } from "./release-context-audit.mjs";

/**
 * @param {string} _root
 */
export function ReleaseI18xPaths(_root) {
  const dir = join(_root, "i18x", "gulp", "releases");
  return {
    dir,
    enUS: join(dir, "en-US.json"),
    deDE: join(dir, "de-DE.json"),
  };
}

/**
 * Extract en-US identity map from RELEASES.json. Does NOT call AI translation.
 * Preserves existing de-DE translations; reports missing keys.
 *
 * @param {object} _options
 * @param {string} _options.root
 * @param {boolean} [_options.write=true]
 * @returns {{ bodies: number, enUS: Record<string,string>, deDE: Record<string,string>, missingDe: string[], staleDe: string[], wrote: boolean }}
 */
export function UpdateReleaseI18xSources(_options) {
  const paths = ReleaseI18xPaths(_options.root);
  mkdirSync(paths.dir, { recursive: true });

  const bodies = ListReleaseSourceBodies(_options.root).filter(Boolean);
  /** @type {Record<string, string>} */
  const enUS = {};
  for (const body of bodies) {
    enUS[body] = body;
  }

  /** @type {Record<string, string>} */
  let deDE = {};
  if (existsSync(paths.deDE)) {
    try {
      deDE = JSON.parse(readFileSync(paths.deDE, "utf8")) || {};
    } catch {
      deDE = {};
    }
  }

  const missingDe = bodies.filter((b) => !deDE[b] || !String(deDE[b]).trim());
  const staleDe = Object.keys(deDE).filter((k) => !bodies.includes(k));

  // Preserve valid de-DE entries; drop stale only from report (do not delete unless write cleans)
  /** @type {Record<string, string>} */
  const deKept = {};
  for (const body of bodies) {
    if (deDE[body]) deKept[body] = deDE[body];
  }

  let wrote = false;
  if (_options.write !== false) {
    writeFileSync(paths.enUS, `${JSON.stringify(enUS, null, "\t")}\n`, "utf8");
    writeFileSync(paths.deDE, `${JSON.stringify(deKept, null, "\t")}\n`, "utf8");
    wrote = true;
  }

  return {
    bodies: bodies.length,
    enUS,
    deDE: deKept,
    missingDe,
    staleDe,
    wrote,
  };
}

/**
 * @param {string} _root
 * @returns {{ complete: boolean, missingDe: string[], bodyCount: number }}
 */
export function CheckReleaseI18xCompleteness(_root) {
  const result = UpdateReleaseI18xSources({ root: _root, write: false });
  return {
    complete: result.missingDe.length === 0,
    missingDe: result.missingDe,
    bodyCount: result.bodies,
  };
}
