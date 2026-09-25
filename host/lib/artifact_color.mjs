/**
 * Host-side Apple II HGR artifact helpers (educational model).
 * Not a full NTSC simulator — deterministic patterns for unit tests.
 *
 * Odd/even column pairs in a group of 7+phase produce classic artifact hues
 * in many documented Apple II explanations. We encode groups only.
 */

export const HGR_W = 280;
export const HGR_H = 192;

/** Create empty bit buffer: 1 bit per Apple pixel, row-major. */
export function createBitBuffer() {
  return new Uint8Array(HGR_W * HGR_H);
}

export function setPixel(buf, x, y, on) {
  if (x < 0 || y < 0 || x >= HGR_W || y >= HGR_H) return;
  buf[y * HGR_W + x] = on ? 1 : 0;
}

export function fillPattern(buf, kind) {
  buf.fill(0);
  for (let y = 0; y < HGR_H; y++) {
    for (let x = 0; x < HGR_W; x++) {
      let on = 0;
      switch (kind) {
        case "alt1010":
          on = (x & 1) === 0 ? 1 : 0;
          break;
        case "alt0101":
          on = (x & 1) === 1 ? 1 : 0;
          break;
        case "isolated":
          on = x % 7 === 0 && y % 7 === 0 ? 1 : 0;
          break;
        case "adjacent_pair":
          on = x % 4 === 0 || x % 4 === 1 ? 1 : 0;
          break;
        case "white_run":
          on = x >= 40 && x < 240 ? 1 : 0;
          break;
        case "black_run":
          on = 0;
          break;
        default:
          on = 0;
      }
      setPixel(buf, x, y, on);
    }
  }
}

/**
 * Very coarse phase group: bits [x,x+1] with page phase bit.
 * Returns 'black'|'white'|'green'|'purple'|'orange'|'blue'|'other'
 */
export function classifyPair(b0, b1, phaseOdd) {
  if (!b0 && !b1) return "black";
  if (b0 && b1) return "white";
  if (b0 && !b1) return phaseOdd ? "blue" : "purple";
  if (!b0 && b1) return phaseOdd ? "orange" : "green";
  return "other";
}

export function countClasses(buf, phaseOdd = false) {
  const counts = {
    black: 0,
    white: 0,
    green: 0,
    purple: 0,
    orange: 0,
    blue: 0,
    other: 0,
  };
  for (let y = 0; y < HGR_H; y++) {
    for (let x = 0; x + 1 < HGR_W; x += 2) {
      const c = classifyPair(
        buf[y * HGR_W + x],
        buf[y * HGR_W + x + 1],
        phaseOdd,
      );
      counts[c]++;
    }
  }
  return counts;
}

/** Monochrome luminance from bits (NOT desaturated composite). */
export function monoLuma(buf) {
  const out = new Uint8Array(buf.length);
  for (let i = 0; i < buf.length; i++) out[i] = buf[i] ? 255 : 0;
  return out;
}
