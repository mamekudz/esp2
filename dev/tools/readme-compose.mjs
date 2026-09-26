// ===========================================
// readme-compose.mjs — locale .src.md → public READMEs
// ESP][ (esp2) — µGulp-style compose
// ===========================================
//
// Sources (edit these):
//   dev/docs/readme/en-US.src.md  →  README.md          (GitHub default)
//   dev/docs/readme/de-DE.src.md  →  README.de-DE.md
// Optional filtered baselines:
//   dev/docs/readme/en-US.md
//   dev/docs/readme/de-DE.md
//
// Channel markers (outside fenced code), same idea as microGulp:
//   unmarked text       → published
//   <!-- git … -->      → Git README only
//   <!-- website … -->  → skipped for Git README
//   <!-- note … -->     → maintainer only (never published)

import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
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
    outputRel: "README.md",
  },
  "de-DE": {
    sourceRel: "dev/docs/readme/de-DE.src.md",
    baselineRel: "dev/docs/readme/de-DE.md",
    outputRel: "README.de-DE.md",
  },
});

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
 * Compose one locale README from its .src.md. Deterministic.
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
  const outPath = join(root, cfg.outputRel);

  if (!existsSync(sourcePath)) {
    throw new Error(`README source missing: ${sourcePath}`);
  }

  const raw = readFileSync(sourcePath, "utf8");
  const composed = FilterChannels(raw, "git");

  let changed = false;
  if (writeIfChanged(baselinePath, composed)) changed = true;
  if (writeIfChanged(outPath, composed)) changed = true;

  return {
    locale,
    source: sourcePath,
    baseline: baselinePath,
    output: outPath,
    bytes: Buffer.byteLength(composed, "utf8"),
    changed,
  };
}

/**
 * Compose all public README locales. Deterministic.
 * @param {{ root?: string }} [_opts]
 */
export function ComposeReadme(_opts = {}) {
  const root = _opts.root ?? PROJECT_ROOT;
  const results = [];
  let changed = false;
  let bytes = 0;
  for (const locale of Object.keys(README_SOURCES)) {
    const r = ComposeReadmeLocale({ root, locale });
    results.push(r);
    if (r.changed) changed = true;
    bytes += r.bytes;
  }
  return {
    source: join(root, README_SOURCES["en-US"].sourceRel),
    output: join(root, "README.md"),
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
