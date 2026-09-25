/** Viewport mapping (mirrors host/include/esp_bracket/viewport.hpp). */

export function mapAppleToPhysical(cfg, ax, ay) {
  const { visibleEnclosure: v, appleLogical: a } = cfg;
  if (a.w <= 0 || a.h <= 0) return { x: 0, y: 0 };
  const nx = ax - a.x;
  const ny = ay - a.y;
  return {
    x: (v.x + Math.trunc((nx * v.w) / a.w)) | 0,
    y: (v.y + Math.trunc((ny * v.h) / a.h)) | 0,
  };
}

export const kPhysical = { x: 0, y: 0, w: 280, h: 456 };
export const kApple = { x: 0, y: 0, w: 280, h: 192 };
