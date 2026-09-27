/**
 * Relative dial/encoder → absolute Apple II paddle (0..255).
 * Shared by Windows input bridge providers (Logitech Dial, etc.).
 */

export const PADDLE_MIN = 0;
export const PADDLE_MAX = 255;
export const PADDLE_CENTER = 128;

/** Default: one detent ≈ 2 paddle units (configurable). */
export const DEFAULT_DIAL_SENSITIVITY = 2;

export class VirtualPaddleAccumulator {
  /**
   * @param {{ initial?: number, sensitivity?: number, min?: number, max?: number }} [opts]
   */
  constructor(opts = {}) {
    this.min = opts.min ?? PADDLE_MIN;
    this.max = opts.max ?? PADDLE_MAX;
    this.sensitivity = opts.sensitivity ?? DEFAULT_DIAL_SENSITIVITY;
    this.value = clamp(
      opts.initial ?? PADDLE_CENTER,
      this.min,
      this.max,
    );
  }

  /** @param {number} delta relative ticks (negative = left / decrease) */
  applyDelta(delta) {
    const d = Number(delta);
    if (!Number.isFinite(d) || d === 0) {
      return this.value;
    }
    this.value = clamp(
      this.value + d * this.sensitivity,
      this.min,
      this.max,
    );
    return this.value;
  }

  /** @returns {number} integer paddle 0..255 */
  paddle() {
    return Math.round(this.value);
  }
}

function clamp(v, lo, hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}
