// ===========================================
// release-version.mjs — comparable version keys
// ===========================================

/**
 * @param {object|string|null|undefined} _value
 * @returns {{ main: number, minor: number, revision: number }}
 */
export function NormalizeVersionParts(_value) {
  if (_value && typeof _value === "object") {
    return {
      main: _ToNonNegInt(_value.main),
      minor: _ToNonNegInt(_value.minor),
      revision: _ToNonNegInt(_value.revision),
    };
  }
  const text = String(_value ?? "").trim();
  const match = text.match(/^(\d+)\.(\d+)\.(\d+)/);
  if (!match) {
    throw new Error(`Invalid version: ${text || "(empty)"}`);
  }
  return {
    main: Number(match[1]),
    minor: Number(match[2]),
    revision: Number(match[3]),
  };
}

/**
 * @param {object|string} _value
 * @returns {string}
 */
export function FormatVersionString(_value) {
  const parts = NormalizeVersionParts(_value);
  return `${parts.main}.${parts.minor}.${parts.revision}`;
}

/**
 * Integer sort key — safe across practical release ranges.
 * @param {object|string} _value
 * @returns {number}
 */
export function VersionToSortKey(_value) {
  const parts = NormalizeVersionParts(_value);
  return parts.main * 1_000_000 + parts.minor * 1_000 + parts.revision;
}

/**
 * @param {object|string} _left
 * @param {object|string} _right
 * @returns {number}
 */
export function CompareVersions(_left, _right) {
  return VersionToSortKey(_left) - VersionToSortKey(_right);
}

/**
 * @param {number|string|null|undefined} _value
 * @returns {number}
 */
function _ToNonNegInt(_value) {
  const number = Number(_value);
  if (!Number.isFinite(number) || number < 0) return 0;
  return Math.floor(number);
}
