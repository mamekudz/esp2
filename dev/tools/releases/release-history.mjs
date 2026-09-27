// ===========================================
// release-history.mjs — localized history view
// ===========================================

import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { StripReleaseContext } from "./release-context.mjs";
import { LoadCentralReleases } from "./release-merge.mjs";
import { FormatVersionString } from "./release-version.mjs";

/**
 * Minimal en-US → de-DE dictionary for release-info lines.
 * Keys are stripped en-US bodies (no context tag).
 * @param {string} _root
 * @returns {Record<string, Record<string, string>>}
 */
export function LoadReleaseDictionaries(_root) {
  /** @type {Record<string, Record<string, string>>} */
  const out = { "en-US": {}, "de-DE": {} };
  for (const lid of ["en-US", "de-DE"]) {
    const path = join(_root, "i18x", "gulp", "releases", `${lid}.json`);
    if (!existsSync(path)) continue;
    const raw = JSON.parse(readFileSync(path, "utf8"));
    if (raw && typeof raw === "object") out[lid] = raw;
  }
  return out;
}

/**
 * @param {string} _text
 * @param {string} _lid
 * @param {Record<string, Record<string, string>>} _dicts
 * @returns {string}
 */
export function TranslateReleaseInfoLine(_text, _lid, _dicts) {
  const body = StripReleaseContext(_text);
  if (_lid === "en-US") return body;
  const map = _dicts[_lid] ?? {};
  return map[body] ?? body;
}

/**
 * Accordion payload for LogAccordion / CLI fallback.
 *
 * @param {object} _options
 * @param {string} _options.releasesPath
 * @param {string} [_options.root] project root for dictionaries
 * @param {string} [_options.lid='en-US']
 * @param {number} [_options.maxReleases]
 * @param {boolean} [_options.includeBeta=true]
 * @returns {{ title: string, items: Array<{ summary: string, open: boolean, lines: string[] }>, versionLabel: string, entryCount: number }}
 */
export function BuildReleaseHistoryAccordion(_options) {
  const lid = _options.lid || "en-US";
  const dicts = LoadReleaseDictionaries(_options.root ?? "");
  let releases = LoadCentralReleases(_options.releasesPath).releases;
  if (_options.includeBeta === false) {
    releases = releases.filter((_r) => !_r.beta);
  }
  if (_options.maxReleases > 0) {
    releases = releases.slice(0, _options.maxReleases);
  }

  const betaLabel =
    lid === "de-DE" ? "Beta" : TranslateReleaseInfoLine("beta", lid, dicts) || "beta";
  const title =
    lid === "de-DE" ? "ESP][ Versionshistorie" : "ESP][ release history";

  const items = releases.map((_release, _index) => {
    const version = FormatVersionString(_release);
    const betaSuffix = _release.beta ? ` (${betaLabel})` : "";
    return {
      summary: `V${version}${betaSuffix} — ${_release.date}`,
      open: _index === 0,
      lines: (_release.info ?? []).map((_line) =>
        TranslateReleaseInfoLine(_line, lid, dicts)
      ),
    };
  });

  const newest = releases[0];
  const versionLabel = newest ? FormatVersionString(newest) : "none";
  const entryCount = releases.reduce(
    (_n, _r) => _n + (_r.info?.length ?? 0),
    0
  );

  return { title, items, versionLabel, entryCount };
}

/**
 * Plain multi-line history for CLI when accordion is unavailable.
 * @param {ReturnType<typeof BuildReleaseHistoryAccordion>} _accordion
 * @returns {string}
 */
export function FormatReleaseHistoryText(_accordion) {
  const lines = [_accordion.title, "=".repeat(_accordion.title.length), ""];
  for (const item of _accordion.items) {
    lines.push(item.summary);
    for (const line of item.lines) {
      lines.push(`  • ${line}`);
    }
    lines.push("");
  }
  return lines.join("\n").trimEnd() + "\n";
}
