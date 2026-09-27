// ===========================================
// release-context-audit.mjs — CHECK / FIX
// ===========================================
//
// CHECK does not modify files.
// FIX normalizes release-info context tags (idempotent).
// Distinguishes translatable natural-language vs machine/identity fields.

import { existsSync, readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import {
  PrepareReleaseContext,
  RELEASE_INFO_CONTEXT_TAG,
  StripReleaseContext,
} from "./release-context.mjs";
import { LoadCentralReleases, LoadDeveloperReleaseFiles } from "./release-merge.mjs";
import { ReleasesPaths } from "./paths.mjs";

/** Object keys whose string values are user-visible / translatable. */
export const TRANSLATABLE_INFO_KEYS = new Set([
  "title",
  "description",
  "status",
  "note",
  "notes",
  "label",
  "text",
  "summary",
  "detail",
  "details",
  "message",
]);

/** Object keys that must never receive release-info context. */
export const MACHINE_INFO_KEYS = new Set([
  "version",
  "date",
  "commit",
  "sha",
  "sha256",
  "hash",
  "url",
  "path",
  "uuid",
  "id",
  "model",
  "value",
  "cps",
  "mhz",
  "author",
  "main",
  "minor",
  "revision",
  "beta",
  "category",
]);

const RELEASE_CONTEXT_RE = /<context\s*=\s*["']release info["']\s*\/>/gi;
const ANY_CONTEXT_RE = /<context\s*=\s*["'][^"']*["']\s*\/>/gi;

/**
 * @typedef {object} ContextIssue
 * @property {"missing"|"duplicate"|"malformed"|"machine_tagged"} kind
 * @property {string} path
 * @property {string} [text]
 */

/**
 * @param {string} _text
 * @returns {number}
 */
export function CountReleaseInfoContextTags(_text) {
  return (String(_text ?? "").match(RELEASE_CONTEXT_RE) || []).length;
}

/**
 * @param {string} _text
 * @returns {boolean}
 */
export function HasMalformedReleaseContext(_text) {
  const s = String(_text ?? "");
  // Equivalent but non-canonical forms (wrong spacing / quotes) after strip still leave residue? —
  // detect multiple contexts or context not at end.
  const count = CountReleaseInfoContextTags(s);
  if (count > 1) return true;
  if (count === 1 && !s.trimEnd().endsWith(RELEASE_INFO_CONTEXT_TAG) && !/<context="release info"\/>\s*$/.test(s)) {
    return true;
  }
  return false;
}

/**
 * Walk a release `info` value and collect issues (CHECK) or rewrite (FIX).
 * @param {unknown} _info
 * @param {string} _path
 * @param {{ fix?: boolean }} [_opts]
 * @returns {{ issues: ContextIssue[], value: unknown, changed: boolean }}
 */
export function AuditInfoValue(_info, _path, _opts = {}) {
  const issues = [];
  let changed = false;

  if (typeof _info === "string") {
    const r = _AuditTranslatableString(_info, _path, _opts.fix === true);
    return { issues: r.issues, value: r.value, changed: r.changed };
  }

  if (Array.isArray(_info)) {
    const out = [];
    for (let i = 0; i < _info.length; i++) {
      const child = AuditInfoValue(_info[i], `${_path}[${i}]`, _opts);
      issues.push(...child.issues);
      out.push(child.value);
      if (child.changed) changed = true;
    }
    return { issues, value: out, changed };
  }

  if (_info && typeof _info === "object") {
    /** @type {Record<string, unknown>} */
    const out = {};
    for (const [key, val] of Object.entries(_info)) {
      const childPath = `${_path}.${key}`;
      if (typeof val === "string") {
        if (MACHINE_INFO_KEYS.has(key)) {
          const tags = CountReleaseInfoContextTags(val);
          if (tags > 0) {
            issues.push({
              kind: "machine_tagged",
              path: childPath,
              text: StripReleaseContext(val),
            });
            if (_opts.fix) {
              out[key] = StripReleaseContext(val);
              changed = true;
              continue;
            }
          }
          out[key] = val;
          continue;
        }
        if (TRANSLATABLE_INFO_KEYS.has(key) || !MACHINE_INFO_KEYS.has(key)) {
          // Unknown keys that look like prose: treat as translatable if they contain spaces/letters
          const treatAsText =
            TRANSLATABLE_INFO_KEYS.has(key) || _LooksLikeProse(val);
          if (treatAsText) {
            const r = _AuditTranslatableString(val, childPath, _opts.fix === true);
            issues.push(...r.issues);
            out[key] = r.value;
            if (r.changed) changed = true;
            continue;
          }
        }
        out[key] = val;
      } else {
        const child = AuditInfoValue(val, childPath, _opts);
        issues.push(...child.issues);
        out[key] = child.value;
        if (child.changed) changed = true;
      }
    }
    return { issues, value: out, changed };
  }

  return { issues, value: _info, changed: false };
}

/**
 * CHECK all central + contributor release sources.
 * @param {object} _options
 * @param {string} _options.root
 * @returns {{ ok: boolean, issues: ContextIssue[], scanned: number }}
 */
export function CheckReleaseContexts(_options) {
  const paths = ReleasesPaths(_options.root);
  const issues = [];
  let scanned = 0;

  const central = LoadCentralReleases(paths.releasesPath);
  for (let ri = 0; ri < central.releases.length; ri++) {
    const release = central.releases[ri];
    const r = AuditInfoValue(release.info, `RELEASES.json.releases[${ri}].info`, {
      fix: false,
    });
    issues.push(...r.issues);
    scanned += _CountLeaves(release.info);
  }

  for (const file of LoadDeveloperReleaseFiles(paths.developerDir)) {
    for (let ci = 0; ci < file.contributions.length; ci++) {
      const contribution = file.contributions[ci];
      // Contributor files may omit context (tooling owns tags) — CHECK only
      // flags duplicates/malformed/machine tags, not missing tags in contributors.
      const r = AuditInfoValue(
        contribution.info,
        `${file.path}.contributions[${ci}].info`,
        { fix: false }
      );
      for (const issue of r.issues) {
        if (issue.kind === "missing") continue; // allowed in contributor sources
        issues.push(issue);
      }
      scanned += _CountLeaves(contribution.info);
    }
  }

  return { ok: issues.length === 0, issues, scanned };
}

/**
 * FIX context on RELEASES.json only (canonical). Contributor files stay plain en-US.
 * @param {object} _options
 * @param {string} _options.root
 * @param {boolean} [_options.write=true]
 * @returns {{ changed: boolean, issuesFixed: number, releases: object }}
 */
export function FixReleaseContexts(_options) {
  const paths = ReleasesPaths(_options.root);
  const central = LoadCentralReleases(paths.releasesPath);
  let issuesFixed = 0;
  let changed = false;

  const releases = central.releases.map((release, ri) => {
    const before = JSON.stringify(release.info);
    const audit = AuditInfoValue(release.info, `releases[${ri}].info`, { fix: true });
    issuesFixed += audit.issues.length;
    if (audit.changed || before !== JSON.stringify(audit.value)) changed = true;
    return { ...release, info: audit.value };
  });

  const doc = { releases };
  if (_options.write !== false) {
    writeFileSync(paths.releasesPath, `${JSON.stringify(doc, null, "\t")}\n`, "utf8");
  }
  return { changed, issuesFixed, releases: doc };
}

/**
 * @param {string} _text
 * @param {string} _path
 * @param {boolean} _fix
 */
function _AuditTranslatableString(_text, _path, _fix) {
  const issues = [];
  const count = CountReleaseInfoContextTags(_text);
  let value = _text;
  let changed = false;

  if (count === 0) {
    issues.push({ kind: "missing", path: _path, text: _text });
    if (_fix) {
      value = PrepareReleaseContext(_text);
      changed = true;
    }
  } else if (count > 1 || HasMalformedReleaseContext(_text)) {
    issues.push({
      kind: count > 1 ? "duplicate" : "malformed",
      path: _path,
      text: StripReleaseContext(_text),
    });
    if (_fix) {
      value = PrepareReleaseContext(_text);
      changed = true;
    }
  } else {
    const normalized = PrepareReleaseContext(_text);
    if (normalized !== _text) {
      issues.push({ kind: "malformed", path: _path, text: StripReleaseContext(_text) });
      if (_fix) {
        value = normalized;
        changed = true;
      }
    }
  }
  return { issues, value, changed };
}

function _LooksLikeProse(_text) {
  const s = String(_text ?? "");
  if (s.length < 8) return false;
  if (/^[0-9a-f]{7,40}$/i.test(s)) return false;
  if (/^https?:\/\//i.test(s)) return false;
  if (/^[A-Z0-9._-]{1,24}$/.test(s) && !/\s/.test(s)) return false;
  return /[a-zA-Z]{3,}/.test(s) && (/\s/.test(s) || s.length > 20);
}

function _CountLeaves(_info) {
  if (typeof _info === "string") return 1;
  if (Array.isArray(_info)) return _info.reduce((n, v) => n + _CountLeaves(v), 0);
  if (_info && typeof _info === "object") {
    return Object.values(_info).reduce((n, v) => n + _CountLeaves(v), 0);
  }
  return 0;
}

/**
 * @param {string} _root
 * @returns {string[]}
 */
export function ListReleaseSourceBodies(_root) {
  const paths = ReleasesPaths(_root);
  const bodies = [];
  const central = LoadCentralReleases(paths.releasesPath);
  for (const release of central.releases) {
    _CollectBodies(release.info, bodies);
  }
  return [...new Set(bodies)];
}

function _CollectBodies(_info, _out) {
  if (typeof _info === "string") {
    _out.push(StripReleaseContext(_info));
    return;
  }
  if (Array.isArray(_info)) {
    for (const v of _info) _CollectBodies(v, _out);
    return;
  }
  if (_info && typeof _info === "object") {
    for (const [key, val] of Object.entries(_info)) {
      if (MACHINE_INFO_KEYS.has(key)) continue;
      if (typeof val === "string" && (TRANSLATABLE_INFO_KEYS.has(key) || _LooksLikeProse(val))) {
        _out.push(StripReleaseContext(val));
      } else {
        _CollectBodies(val, _out);
      }
    }
  }
}

// silence unused import in some bundlers
void existsSync;
void join;
void readFileSync;
void ANY_CONTEXT_RE;
