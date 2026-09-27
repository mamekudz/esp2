// ===========================================
// readme-compose.mjs — locale .src.md → bilingual Git README
// ESP][ (esp2) — µGulp / microCSS-style compose
// ===========================================
//
// Sources (edit these):
//   dev/docs/readme/en-US.src.md
//   dev/docs/readme/de-DE.src.md
//
// Generated:
//   README.md                 ← English then German (one page, #deutsch)
//   dev/docs/readme/en-US.md  ← filtered en-US baseline
//   dev/docs/readme/de-DE.md  ← filtered de-DE baseline
//
// Channel markers (outside fenced code), same idea as microGulp:
//   unmarked text       → published
//   <!-- git … -->      → Git README only
//   <!-- website … -->  → skipped for Git README
//   <!-- note … -->     → maintainer only (never published)

import { existsSync, mkdirSync, readFileSync, unlinkSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const PROJECT_ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const README_DIR = join(PROJECT_ROOT, "dev", "docs", "readme");

/** @deprecated use README_SOURCES — kept for older callers */
export const README_SOURCE_RELATIVE = "dev/docs/readme/en-US.src.md";

export const README_SOURCES = Object.freeze({
  "en-US": {
    sourceRel: "dev/docs/readme/en-US.src.md",
    baselineRel: "dev/docs/readme/en-US.md",
  },
  "de-DE": {
    sourceRel: "dev/docs/readme/de-DE.src.md",
    baselineRel: "dev/docs/readme/de-DE.md",
  },
});

/** Single public GitHub README (bilingual, English first). */
export const README_OUTPUT_RELATIVE = "README.md";

/** Legacy separate German README — removed by compose when present. */
export const README_LEGACY_DE_RELATIVE = "README.de-DE.md";

export const MICROGULP_READY_ASSET = "docs/assets/microgulp-ready.png";

/**
 * Strip HTML channel comment blocks outside fenced code.
 * @param {string} _text
 * @param {"git" | "website"} _channel
 */
export function FilterChannels(_text, _channel = "git") {
  const lines = String(_text).replace(/\r\n/g, "\n").split("\n");
  const out = [];
  let inFence = false;
  let skipDepth = 0;
  let skipKind = null;

  for (const line of lines) {
    const fence = line.trimStart().startsWith("```");
    if (fence) inFence = !inFence;

    if (!inFence) {
      const open = line.match(/^\s*<!--\s*(note|git|website)\b/i);
      if (open && !line.includes("-->")) {
        const kind = open[1].toLowerCase();
        skipKind = kind;
        skipDepth = 1;
        continue;
      }
      if (open && line.includes("-->")) {
        const kind = open[1].toLowerCase();
        if (kind === "note") continue;
        if (kind === "website" && _channel === "git") continue;
        if (kind === "git" && _channel === "website") continue;
        const inner = line.replace(
          /^\s*<!--\s*(note|git|website)\b[^>]*-->\s*/i,
          "",
        );
        if (inner) out.push(inner);
        continue;
      }
      if (skipDepth > 0) {
        if (/^\s*<!--/.test(line)) skipDepth += 1;
        if (/-->/.test(line)) {
          skipDepth -= 1;
          if (skipDepth <= 0) {
            const drop =
              skipKind === "note" ||
              (skipKind === "website" && _channel === "git") ||
              (skipKind === "git" && _channel === "website");
            skipDepth = 0;
            skipKind = null;
            if (drop) continue;
          }
        } else if (
          skipKind === "note" ||
          (skipKind === "website" && _channel === "git") ||
          (skipKind === "git" && _channel === "website")
        ) {
          continue;
        }
        if (skipDepth > 0) {
          if (
            skipKind === "note" ||
            (skipKind === "website" && _channel === "git") ||
            (skipKind === "git" && _channel === "website")
          ) {
            continue;
          }
        }
      }
    }

    out.push(line);
  }

  let body = out.join("\n").replace(/[ \t]+$/gm, "");
  body = body.replace(/\n{3,}/g, "\n\n").replace(/^\n+/, "").replace(/\n*$/, "\n");
  return body;
}

/**
 * @param {string} path
 * @param {string} next
 * @returns {boolean} whether file changed
 */
function writeIfChanged(path, next) {
  const prev = existsSync(path) ? readFileSync(path, "utf8") : null;
  if (prev === next) {
    return false;
  }
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, next, "utf8");
  return true;
}

/**
 * Filter one locale source and write its baseline under dev/docs/readme/.
 * Does not write the public README.md (use ComposeReadme for that).
 * @param {{ root?: string, locale?: "en-US" | "de-DE" }} [_opts]
 */
export function ComposeReadmeLocale(_opts = {}) {
  const root = _opts.root ?? PROJECT_ROOT;
  const locale = _opts.locale ?? "en-US";
  const cfg = README_SOURCES[locale];
  if (!cfg) {
    throw new Error(`Unknown README locale: ${locale}`);
  }

  const sourcePath = join(root, cfg.sourceRel);
  const baselinePath = join(root, cfg.baselineRel);

  if (!existsSync(sourcePath)) {
    throw new Error(`README source missing: ${sourcePath}`);
  }

  const raw = readFileSync(sourcePath, "utf8");
  const composed = FilterChannels(raw, "git");
  const changed = writeIfChanged(baselinePath, composed);

  return {
    locale,
    source: sourcePath,
    baseline: baselinePath,
    output: baselinePath,
    bytes: Buffer.byteLength(composed, "utf8"),
    changed,
    body: composed,
  };
}

/**
 * Build bilingual README.md (English first, then German) — microCSS style.
 * Also refreshes locale baselines. Removes legacy README.de-DE.md when present.
 * @param {{ root?: string }} [_opts]
 */
export function ComposeReadme(_opts = {}) {
  const root = _opts.root ?? PROJECT_ROOT;
  const results = [];
  let changed = false;
  let bytes = 0;
  /** @type {Record<string, string>} */
  const bodies = {};

  for (const locale of Object.keys(README_SOURCES)) {
    const r = ComposeReadmeLocale({ root, locale });
    results.push(r);
    bodies[locale] = r.body;
    if (r.changed) changed = true;
    bytes += r.bytes;
  }

  const en = String(bodies["en-US"] ?? "").replace(/\n*$/, "\n");
  const de = String(bodies["de-DE"] ?? "").replace(/^\n+/, "").replace(/\n*$/, "\n");
  const bilingual = `${en}\n---\n\n## Deutsch\n\n${de}`;
  const outPath = join(root, README_OUTPUT_RELATIVE);
  if (writeIfChanged(outPath, bilingual)) changed = true;
  bytes += Buffer.byteLength(bilingual, "utf8");

  const legacyDe = join(root, README_LEGACY_DE_RELATIVE);
  if (existsSync(legacyDe)) {
    unlinkSync(legacyDe);
    changed = true;
  }

  return {
    source: join(root, README_SOURCES["en-US"].sourceRel),
    output: outPath,
    bytes,
    changed,
    results,
  };
}

export {
  README_DIR,
  PROJECT_ROOT,
  SOURCE as _DEPRECATED_SOURCE,
};

const SOURCE = join(README_DIR, "de-DE.src.md");
const BASELINE = join(README_DIR, "de-DE.md");
const PUBLIC_README = join(PROJECT_ROOT, "README.md");

export { SOURCE, BASELINE, PUBLIC_README };
