/**
 * Landscape presentation: CW90° + NN scale edge preservation.
 * Synthetic edge/corner pattern — no Galaxian dependency.
 */
#include <cstdio>
#include <cstring>
#include <vector>

#include "esp_bracket/landscape_present.hpp"

using esp_bracket::LandscapePresent;

static int g_fails = 0;

static void expect(bool cond, const char *msg) {
    if (!cond) {
        std::printf("FAIL  %s\n", msg);
        ++g_fails;
    } else {
        std::printf("PASS  %s\n", msg);
    }
}

enum : uint16_t {
    kBg = 0x0000,
    kTop = 0xF800,    // red — Apple II scanline 0
    kBottom = 0x07E0, // green — scanline 191
    kLeft = 0x001F,   // blue — sx=0
    kRight = 0xFFE0,  // yellow — sx=279
    kTL = 0xF81F,     // magenta corner
    kTR = 0x07FF,     // cyan
    kBL = 0xFC00,     // orange
    kBR = 0xFFFF,     // white
};

static void paintEdges(uint16_t *fb) {
    std::memset(fb, 0, LandscapePresent::kSrcW * LandscapePresent::kSrcH * sizeof(uint16_t));
    for (int x = 0; x < LandscapePresent::kSrcW; ++x) {
        fb[x + 0 * LandscapePresent::kSrcW] = kTop;
        fb[x + 191 * LandscapePresent::kSrcW] = kBottom;
    }
    for (int y = 0; y < LandscapePresent::kSrcH; ++y) {
        fb[0 + y * LandscapePresent::kSrcW] = kLeft;
        fb[279 + y * LandscapePresent::kSrcW] = kRight;
    }
    fb[0 + 0 * LandscapePresent::kSrcW] = kTL;
    fb[279 + 0 * LandscapePresent::kSrcW] = kTR;
    fb[0 + 191 * LandscapePresent::kSrcW] = kBL;
    fb[279 + 191 * LandscapePresent::kSrcW] = kBR;
}

static int countSrcY(const uint16_t *dst, int outW, int outH, int syWant, uint16_t color) {
    int n = 0;
    for (int dy = 0; dy < outH; ++dy) {
        for (int dx = 0; dx < outW; ++dx) {
            int sx = 0, sy = 0;
            LandscapePresent::mapDestToSrc(dx, dy, outW, outH, &sx, &sy);
            if (sy == syWant && dst[dx + dy * outW] == color) {
                ++n;
            }
        }
    }
    return n;
}

static int countMappedColsForSy(int outW, int outH, int syWant, bool useFloor) {
    int cols = 0;
    for (int dx = 0; dx < outW; ++dx) {
        bool hit = false;
        for (int dy = 0; dy < outH && !hit; ++dy) {
            int sx = 0, sy = 0;
            if (useFloor) {
                LandscapePresent::mapDestToSrcFloor(dx, dy, outW, outH, &sx, &sy);
            } else {
                LandscapePresent::mapDestToSrc(dx, dy, outW, outH, &sx, &sy);
            }
            if (sy == syWant) {
                hit = true;
            }
        }
        if (hit) {
            ++cols;
        }
    }
    return cols;
}

int main() {
    int outW = 0, outH = 0, ox = 0, oy = 0;
    LandscapePresent::computeOutSize(280, 456, &outW, &outH, &ox, &oy);
    expect(outW == 280 && outH == 408, "geometry 280x408");
    expect(ox == 0 && oy == 24, "centering ox=0 oy=24");

    // Rotation edge mapping (unit square corners).
    {
        int sx, sy;
        LandscapePresent::rotCwInv(0, 0, &sx, &sy);
        expect(sx == 0 && sy == 191, "CW inv (0,0) -> bottom-left src");
        LandscapePresent::rotCwInv(191, 0, &sx, &sy);
        expect(sx == 0 && sy == 0, "CW inv (191,0) -> top-left src");
        LandscapePresent::rotCwInv(0, 279, &sx, &sy);
        expect(sx == 279 && sy == 191, "CW inv (0,279) -> bottom-right src");
        LandscapePresent::rotCwInv(191, 279, &sx, &sy);
        expect(sx == 279 && sy == 0, "CW inv (191,279) -> top-right src");
    }

    const int floorTopCols = countMappedColsForSy(outW, outH, 0, true);
    const int fixTopCols = countMappedColsForSy(outW, outH, 0, false);
    const int fixBotCols = countMappedColsForSy(outW, outH, 191, false);
    expect(floorTopCols == 1, "legacy floor: Apple II top (sy=0) only 1 dest col");
    expect(fixTopCols >= 2, "edge-preserving: Apple II top >= 2 dest cols");
    expect(fixBotCols >= 2, "edge-preserving: Apple II bottom >= 2 dest cols");
    expect(fixTopCols == fixBotCols, "top/bottom edge thickness matched");

    std::vector<uint16_t> src(LandscapePresent::kSrcW * LandscapePresent::kSrcH);
    std::vector<uint16_t> dst(static_cast<size_t>(outW * outH));
    paintEdges(src.data());
    LandscapePresent::transformRgb565(src.data(), dst.data(), outW, outH);

    // Corners of destination must carry corner marker colors.
    auto at = [&](int dx, int dy) { return dst[dx + dy * outW]; };
    expect(at(0, 0) == kBL || at(0, 0) == kLeft || at(0, 0) == kBottom,
           "dst(0,0) is Apple II bottom/left region");
    expect(at(outW - 1, 0) == kTL || at(outW - 1, 0) == kTop || at(outW - 1, 0) == kLeft,
           "dst(right,0) is Apple II top/left region");
    expect(at(0, outH - 1) == kBR || at(0, outH - 1) == kRight || at(0, outH - 1) == kBottom,
           "dst(0,bottom) is Apple II bottom/right region");
    expect(at(outW - 1, outH - 1) == kTR || at(outW - 1, outH - 1) == kTop ||
               at(outW - 1, outH - 1) == kRight,
           "dst(right,bottom) is Apple II top/right region");

    // All four source edges must appear in the destination.
    bool sawTop = false, sawBot = false, sawLeft = false, sawRight = false;
    bool sawTL = false, sawTR = false, sawBL = false, sawBR = false;
    for (int dy = 0; dy < outH; ++dy) {
        for (int dx = 0; dx < outW; ++dx) {
            const uint16_t c = at(dx, dy);
            if (c == kTop) {
                sawTop = true;
            }
            if (c == kBottom) {
                sawBot = true;
            }
            if (c == kLeft) {
                sawLeft = true;
            }
            if (c == kRight) {
                sawRight = true;
            }
            if (c == kTL) {
                sawTL = true;
            }
            if (c == kTR) {
                sawTR = true;
            }
            if (c == kBL) {
                sawBL = true;
            }
            if (c == kBR) {
                sawBR = true;
            }
        }
    }
    expect(sawTop && sawBot && sawLeft && sawRight, "all four edge colors survive");
    expect(sawTL && sawTR && sawBL && sawBR, "all four corner markers survive");

    const int topPx = countSrcY(dst.data(), outW, outH, 0, kTop) +
                      countSrcY(dst.data(), outW, outH, 0, kTL) +
                      countSrcY(dst.data(), outW, outH, 0, kTR);
    expect(topPx >= outH * 2, "top scanline covers >= 2 full dest columns");

    // Classic path is out of scope here; landscape must not invent OOB samples.
    bool oob = false;
    for (int dy = 0; dy < outH; ++dy) {
        for (int dx = 0; dx < outW; ++dx) {
            int sx = 0, sy = 0;
            LandscapePresent::mapDestToSrc(dx, dy, outW, outH, &sx, &sy);
            if (sx < 0 || sx >= LandscapePresent::kSrcW || sy < 0 ||
                sy >= LandscapePresent::kSrcH) {
                oob = true;
            }
        }
    }
    expect(!oob, "no OOB source coordinates");

    if (g_fails) {
        std::printf("Landscape present tests FAILED (%d)\n", g_fails);
        return 1;
    }
    std::printf("All LandscapePresent host tests passed\n");
    return 0;
}
