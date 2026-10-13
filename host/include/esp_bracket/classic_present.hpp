#pragma once

#include <cstdint>

namespace esp_bracket {

/**
 * Classic (upright) present: Apple II 280×192 → nearest-neighbor scale to panel fit.
 * No rotation. Used when the panel is narrower/shorter than 280×192 letterbox (e.g. 240×320).
 */
struct ClassicPresent {
  static constexpr int kSrcW = 280;
  static constexpr int kSrcH = 192;

  static constexpr int nnMapPreserveEdges(int d, int srcN, int dstN) {
    if (srcN <= 1 || dstN <= 1) {
      return 0;
    }
    if (d <= 0) {
      return 0;
    }
    if (d >= dstN - 1) {
      return srcN - 1;
    }
    return (d * (srcN - 1) + (dstN - 1) / 2) / (dstN - 1);
  }

  /** Maximize fractional size preserving aspect; center on panel. */
  static void computeOutSize(int panelW, int panelH, int *outW, int *outH, int *ox, int *oy) {
    // Prefer full width when panel is shorter than 280 and taller than 192 after scale.
    int w = panelW;
    int h = (kSrcH * panelW) / kSrcW;
    if (h > panelH) {
      h = panelH;
      w = (kSrcW * panelH) / kSrcH;
    }
    if (w < 1) {
      w = 1;
    }
    if (h < 1) {
      h = 1;
    }
    // Even RGB565 widths avoid SPI word-alignment color fringing on ILI9341-class panels.
    if ((w & 1) && w > 1) {
      --w;
    }
    if ((h & 1) && h > 1) {
      --h;
    }
    *outW = w;
    *outH = h;
    *ox = (panelW - w) / 2;
    *oy = (panelH - h) / 2;
  }

  /**
   * Largest integer scale of 280×192 that fits (1×, 2×, …). Sharp pixels; letterbox
   * bars on unused edges. On 400×300 that is 1× (280×192) — 2× needs 560×384.
   */
  static void computeOutSizeInteger(int panelW, int panelH, int *outW, int *outH, int *ox,
                                    int *oy) {
    int s = 1;
    while (kSrcW * (s + 1) <= panelW && kSrcH * (s + 1) <= panelH) {
      ++s;
    }
    *outW = kSrcW * s;
    *outH = kSrcH * s;
    *ox = (panelW - *outW) / 2;
    *oy = (panelH - *outH) / 2;
  }

  static void scaleRgb565(const uint16_t *src280x192, uint16_t *dst, int outW, int outH) {
    for (int y = 0; y < outH; ++y) {
      const int sy = nnMapPreserveEdges(y, kSrcH, outH);
      const uint16_t *srcRow = src280x192 + sy * kSrcW;
      uint16_t *dstRow = dst + y * outW;
      for (int x = 0; x < outW; ++x) {
        dstRow[x] = srcRow[nnMapPreserveEdges(x, kSrcW, outW)];
      }
    }
  }

  /** Integer scale (each src pixel → s×s block). outW/outH must be kSrcW*s / kSrcH*s. */
  static void scaleRgb565Integer(const uint16_t *src280x192, uint16_t *dst, int scale) {
    if (scale < 1) {
      return;
    }
    const int outW = kSrcW * scale;
    for (int sy = 0; sy < kSrcH; ++sy) {
      const uint16_t *srcRow = src280x192 + sy * kSrcW;
      for (int dy = 0; dy < scale; ++dy) {
        uint16_t *dstRow = dst + (sy * scale + dy) * outW;
        if (scale == 1) {
          for (int x = 0; x < kSrcW; ++x) {
            dstRow[x] = srcRow[x];
          }
        } else {
          for (int sx = 0; sx < kSrcW; ++sx) {
            const uint16_t c = srcRow[sx];
            uint16_t *p = dstRow + sx * scale;
            for (int dx = 0; dx < scale; ++dx) {
              p[dx] = c;
            }
          }
        }
      }
    }
  }
};

} // namespace esp_bracket
