// ===========================================
// release-context.mjs — release-info context tags
// ===========================================
//
// Family convention: info lines end with <context="release info"/>.
// Contributors author plain en-US; tooling owns tag normalization.

export const RELEASE_INFO_CONTEXT = "release info";
export const RELEASE_INFO_CONTEXT_TAG = `<context="${RELEASE_INFO_CONTEXT}"/>`;

const RELEASE_CONTEXT_RE = /<context\s*=\s*["']release info["']\s*\/>/gi;

/**
 * Removes every release-info context tag and collapses whitespace.
 * @param {string} _text
 * @returns {string}
 */
export function StripReleaseContext(_text) {
  return String(_text ?? "")
    .replace(RELEASE_CONTEXT_RE, "")
    .replace(/\s+/g, " ")
    .trim();
}

/**
 * Ensures exactly one trailing release-info context tag.
 * Safe to call repeatedly — never stacks tags.
 * @param {string} _text
 * @returns {string}
 */
export function PrepareReleaseContext(_text) {
  const body = StripReleaseContext(_text);
  if (!body) return RELEASE_INFO_CONTEXT_TAG;
  return `${body}${RELEASE_INFO_CONTEXT_TAG}`;
}

/**
 * Stable fingerprint for duplicate detection (case-insensitive, no context).
 * @param {string} _text
 * @returns {string}
 */
export function FingerprintReleaseText(_text) {
  return StripReleaseContext(_text).toLowerCase();
}
