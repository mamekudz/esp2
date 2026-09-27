#pragma once

#include <cstdint>

namespace esp_bracket {

/**
 * Landscape presentation math (panel-independent).
 *
 * Apple II 280×192 → CW 90° → 192×280 → nearest-neighbor scale to outW×outH.
 * Pure functions — no display I/O, no Apple II soft-switches.
 */
struct LandscapePresent {
    static constexpr int kSrcW = 280;
    static constexpr int kSrcH = 192;

    /**
     * Map destination index d ∈ [0, dstN) → source index ∈ [0, srcN).
     *
     * Plain `(d * srcN) / dstN` starves the last source index on non-integer
     * upscales (e.g. 192→280: first gets 2 taps, last only 1). After CW 90°,
     * that last index is Apple II scanline 0 (SCORE), which then appears as a
     * single-pixel-thick edge and looks clipped.
     *
     * Symmetric endpoint mapping keeps both edges equally thick and monotonic.
     */
    static constexpr int nnMapPreserveEdges(int d, int srcN, int dstN) {
        if (srcN <= 1 || dstN <= 1) {
            return 0;
        }
        if (d + d < dstN) {
            return (d * srcN) / dstN;
        }
        return srcN - 1 - ((dstN - 1 - d) * srcN) / dstN;
    }

    /** Legacy floor map (starves last source) — kept for regression contrast. */
    static constexpr int nnMapFloor(int d, int srcN, int dstN) {
        if (dstN <= 0) {
            return 0;
        }
        return (d * srcN) / dstN;
    }

    /**
     * Inverse CW 90° sample: rotated (rx,ry) with rotW=srcH, rotH=srcW
     * → source (sx,sy).
     */
    static constexpr void rotCwInv(int rx, int ry, int *sx, int *sy) {
        *sx = ry;
        *sy = kSrcH - 1 - rx;
    }

    /** Destination (dx,dy) in outW×outH → source (sx,sy) in 280×192. */
    static constexpr void mapDestToSrc(int dx, int dy, int outW, int outH, int *sx, int *sy) {
        const int rotW = kSrcH; // 192
        const int rotH = kSrcW; // 280
        const int rx = nnMapPreserveEdges(dx, rotW, outW);
        const int ry = nnMapPreserveEdges(dy, rotH, outH);
        rotCwInv(rx, ry, sx, sy);
    }

    static constexpr void mapDestToSrcFloor(int dx, int dy, int outW, int outH, int *sx, int *sy) {
        const int rotW = kSrcH;
        const int rotH = kSrcW;
        const int rx = nnMapFloor(dx, rotW, outW);
        const int ry = nnMapFloor(dy, rotH, outH);
        rotCwInv(rx, ry, sx, sy);
    }

    /** Geometry: maximize out size on panelW×panelH, aspect of 192×280 preserved. */
    static constexpr void computeOutSize(int panelW, int panelH, int *outW, int *outH, int *ox,
                                         int *oy) {
        const int rotW = kSrcH;
        const int rotH = kSrcW;
        int w = panelW;
        int h = (rotH * panelW) / rotW;
        if (h > panelH) {
            h = panelH;
            w = (rotW * panelH) / rotH;
        }
        *outW = w;
        *outH = h;
        *ox = (panelW - w) / 2;
        *oy = (panelH - h) / 2;
    }

    /** Nearest-neighbor CW90 + scale into dst (outW * outH RGB565). */
    static void transformRgb565(const uint16_t *src280x192, uint16_t *dst, int outW, int outH) {
        for (int dy = 0; dy < outH; ++dy) {
            uint16_t *row = dst + dy * outW;
            for (int dx = 0; dx < outW; ++dx) {
                int sx = 0, sy = 0;
                mapDestToSrc(dx, dy, outW, outH, &sx, &sy);
                row[dx] = src280x192[sx + sy * kSrcW];
            }
        }
    }
};

} // namespace esp_bracket
