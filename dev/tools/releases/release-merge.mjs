// ===========================================
// release-merge.mjs — contributor notes → RELEASES.json
// ===========================================
//
// Permanent rules:
//   - Contributor files under dev/releases/*.json (en-US authoring)
//   - Merge only entries newer than 30 days that are not already present
//   - Duplicate identity = version + fingerprint(stripped info text)
//   - Tooling owns <context="release info"/> normalization

import { existsSync, mkdirSync, readdirSync, readFileSync, writeFileSync } from "node:fs";
import { basename, dirname, join } from "node:path";
import {
  FingerprintReleaseText,
  PrepareReleaseContext,
  StripReleaseContext,
} from "./release-context.mjs";
import {
  CompareVersions,
  FormatVersionString,
  NormalizeVersionParts,
} from "./release-version.mjs";

export const DEFAULT_MAX_AGE_DAYS = 30;

/**
 * @typedef {object} ReleaseEntry
 * @property {number} main
 * @property {number} minor
 * @property {number} revision
 * @property {string} date
 * @property {boolean} [beta]
 * @property {string[]} info
 * @property {string} [author]
 */

/**
 * @typedef {object} MergeSummary
 * @property {number} scanned
 * @property {number} merged
 * @property {number} duplicates
 * @property {number} expired
 * @property {number} invalid
 * @property {string[]} authors
 * @property {ReleaseEntry[]} releases
 * @property {object[]} accepted
 * @property {object[]} skipped
 */

/**
 * @param {string} _dateText
 * @returns {Date|null}
 */
export function ParseReleaseDate(_dateText) {
  const text = String(_dateText ?? "").trim();
  const match = text.match(
    /^(\d{4})-(\d{2})-(\d{2})(?:[ T](\d{2}):(\d{2})(?::(\d{2}))?)?/
  );
  if (!match) return null;
  const iso = `${match[1]}-${match[2]}-${match[3]}T${match[4] ?? "00"}:${match[5] ?? "00"}:${match[6] ?? "00"}`;
  const date = new Date(iso);
  return Number.isNaN(date.getTime()) ? null : date;
}

/**
 * @param {Date} _now
 * @param {Date} _date
 * @param {number} [_maxAgeDays]
 * @returns {boolean}
 */
export function IsWithinMaxAge(_now, _date, _maxAgeDays = DEFAULT_MAX_AGE_DAYS) {
  const maxMs = Math.max(0, Number(_maxAgeDays) || 0) * 86_400_000;
  return _now.getTime() - _date.getTime() <= maxMs;
}

/**
 * @param {string} _dir
 * @returns {{ author: string, path: string, contributions: ReleaseEntry[] }[]}
 */
export function LoadDeveloperReleaseFiles(_dir) {
  if (!existsSync(_dir)) return [];
  const files = readdirSync(_dir)
    .filter((_name) => _name.toLowerCase().endsWith(".json"))
    .sort((_a, _b) => _a.localeCompare(_b));
  const out = [];
  for (const name of files) {
    const path = join(_dir, name);
    const raw = JSON.parse(readFileSync(path, "utf8"));
    const contributions = _NormalizeContributions(raw);
    out.push({
      author: String(raw.author || basename(name, ".json")),
      path,
      contributions,
    });
  }
  return out;
}

/**
 * @param {string} _releasesPath
 * @returns {{ releases: ReleaseEntry[] }}
 */
export function LoadCentralReleases(_releasesPath) {
  if (!existsSync(_releasesPath)) return { releases: [] };
  const raw = JSON.parse(readFileSync(_releasesPath, "utf8"));
  const releases = Array.isArray(raw.releases)
    ? raw.releases.map(_NormalizeReleaseEntry).filter(Boolean)
    : [];
  return { releases };
}

/**
 * @param {ReleaseEntry[]} _releases
 * @returns {Set<string>}
 */
export function CollectExistingInfoFingerprints(_releases) {
  const set = new Set();
  for (const release of _releases ?? []) {
    const version = FormatVersionString(release);
    for (const line of release.info ?? []) {
      set.add(_InfoKey(version, line));
    }
  }
  return set;
}

/**
 * Merge developer contributions into central RELEASES.json.
 * Idempotent: re-running with the same inputs does not duplicate info lines.
 *
 * @param {object} _options
 * @param {string} _options.developerDir
 * @param {string} _options.releasesPath
 * @param {Date|string|number} [_options.now]
 * @param {number} [_options.maxAgeDays]
 * @param {boolean} [_options.write=true]
 * @returns {MergeSummary}
 */
export function MergeDeveloperReleases(_options) {
  const now = _ResolveNow(_options.now);
  const maxAgeDays = _options.maxAgeDays ?? DEFAULT_MAX_AGE_DAYS;
  const central = LoadCentralReleases(_options.releasesPath);
  const releases = central.releases.map((_entry) => ({
    ..._entry,
    info: [...(_entry.info ?? [])],
  }));
  const fingerprints = CollectExistingInfoFingerprints(releases);
  const byVersion = new Map();
  for (const release of releases) {
    byVersion.set(FormatVersionString(release), release);
  }

  let scanned = 0;
  let merged = 0;
  let duplicates = 0;
  let expired = 0;
  let invalid = 0;
  const authors = new Set();
  const accepted = [];
  const skipped = [];

  for (const file of LoadDeveloperReleaseFiles(_options.developerDir)) {
    authors.add(file.author);
    for (const contribution of file.contributions) {
      scanned += 1;
      const date = ParseReleaseDate(contribution.date);
      if (!date) {
        invalid += 1;
        skipped.push({ reason: "invalid-date", author: file.author, contribution });
        continue;
      }
      if (!IsWithinMaxAge(now, date, maxAgeDays)) {
        expired += 1;
        skipped.push({
          reason: "expired",
          author: file.author,
          contribution,
          ageDays: _AgeDays(now, date),
        });
        continue;
      }

      const version = FormatVersionString(contribution);
      const preparedInfo = (contribution.info ?? []).map((_line) =>
        PrepareReleaseContext(_line)
      );
      const newLines = [];
      for (const line of preparedInfo) {
        const key = _InfoKey(version, line);
        if (fingerprints.has(key)) {
          duplicates += 1;
          skipped.push({
            reason: "duplicate",
            author: file.author,
            version,
            text: StripReleaseContext(line),
          });
          continue;
        }
        fingerprints.add(key);
        newLines.push(line);
      }
      if (!newLines.length) continue;

      let target = byVersion.get(version);
      if (!target) {
        target = {
          main: contribution.main,
          minor: contribution.minor,
          revision: contribution.revision,
          date: contribution.date,
          beta: contribution.beta === true,
          info: [],
        };
        releases.push(target);
        byVersion.set(version, target);
      } else if (
        ParseReleaseDate(contribution.date) &&
        (!ParseReleaseDate(target.date) ||
          ParseReleaseDate(contribution.date) > ParseReleaseDate(target.date))
      ) {
        target.date = contribution.date;
      }
      if (contribution.beta === true) target.beta = true;
      target.info.push(...newLines);
      merged += newLines.length;
      accepted.push({
        author: file.author,
        version,
        date: contribution.date,
        lines: newLines.map(StripReleaseContext),
      });
    }
  }

  releases.sort((_a, _b) => {
    const byVersionCmp = CompareVersions(_b, _a);
    if (byVersionCmp !== 0) return byVersionCmp;
    const dateA = ParseReleaseDate(_a.date)?.getTime() ?? 0;
    const dateB = ParseReleaseDate(_b.date)?.getTime() ?? 0;
    return dateB - dateA;
  });

  if (_options.write !== false) {
    mkdirSync(dirname(_options.releasesPath), { recursive: true });
    writeFileSync(
      _options.releasesPath,
      `${JSON.stringify({ releases }, null, "\t")}\n`,
      "utf8"
    );
  }

  return {
    scanned,
    merged,
    duplicates,
    expired,
    invalid,
    authors: [...authors].sort(),
    releases,
    accepted,
    skipped,
  };
}

/**
 * Dry-run count of contributor info lines that would merge now.
 * Used for µGulp dynamic display names / µAttention (ACTION_AVAILABLE).
 *
 * @param {object} _options same as MergeDeveloperReleases
 * @returns {number}
 */
export function CountPendingContributorMerges(_options) {
  const summary = MergeDeveloperReleases({
    ..._options,
    write: false,
  });
  return summary.merged;
}

/**
 * @param {unknown} _raw
 * @returns {ReleaseEntry[]}
 */
function _NormalizeContributions(_raw) {
  const list = Array.isArray(_raw?.contributions)
    ? _raw.contributions
    : Array.isArray(_raw?.releases)
      ? _raw.releases
      : Array.isArray(_raw)
        ? _raw
        : [];
  return list.map(_NormalizeReleaseEntry).filter(Boolean);
}

/**
 * @param {unknown} _raw
 * @returns {ReleaseEntry|null}
 */
function _NormalizeReleaseEntry(_raw) {
  if (!_raw || typeof _raw !== "object") return null;
  const parts = NormalizeVersionParts(_raw);
  const info = Array.isArray(_raw.info)
    ? _raw.info.map((_line) => String(_line ?? "")).filter(Boolean)
    : typeof _raw.text === "string"
      ? [String(_raw.text)]
      : [];
  if (!info.length) return null;
  return {
    main: parts.main,
    minor: parts.minor,
    revision: parts.revision,
    date: String(_raw.date ?? ""),
    beta: _raw.beta === true,
    info,
    author: _raw.author != null ? String(_raw.author) : undefined,
  };
}

/**
 * @param {string} _version
 * @param {string} _line
 */
function _InfoKey(_version, _line) {
  return `${_version}|${FingerprintReleaseText(_line)}`;
}

/**
 * @param {Date|string|number|undefined} _now
 * @returns {Date}
 */
function _ResolveNow(_now) {
  if (_now instanceof Date) return _now;
  if (typeof _now === "number" || typeof _now === "string") {
    const date = new Date(_now);
    if (!Number.isNaN(date.getTime())) return date;
  }
  return new Date();
}

/**
 * @param {Date} _now
 * @param {Date} _date
 */
function _AgeDays(_now, _date) {
  return Math.floor((_now.getTime() - _date.getTime()) / 86_400_000);
}
