#include "esp2_display_hal.h"
#include "boards/waveshare_rlcd42.h"
#include "esp_bracket/classic_present.hpp"

#include <Arduino.h>
#include <SPI.h>
#include <U8g2lib.h>
#include <cstring>

#pragma GCC optimize("O3")

namespace {

Arduino_Canvas *g_canvas = nullptr;
U8G2_ST7305_300X400_F_4W_HW_SPI *g_u8g2 = nullptr;
bool g_panelReady = false;
bool g_invert = false;
bool g_hpm = false;
bool g_classicFitFull = false; // default: integer 1× + letterbox

// ST7305 native buffer (U8g2 horizontal_right_lsb): 38 bytes/row × 400 rows.
constexpr int kTileW = 38;
constexpr int kTileH = 50;
constexpr int kNativeW = RLCD42_LCD_NATIVE_W; // 300
constexpr int kNativeH = RLCD42_LCD_NATIVE_H; // 400
constexpr int kUserW = RLCD42_LCD_WIDTH;      // 400 (U8G2_R1)
constexpr int kUserH = RLCD42_LCD_HEIGHT;     // 300
constexpr int kUserRowBytes = (kUserW + 7) / 8; // 50
constexpr size_t kMonoBytes = static_cast<size_t>(kTileW) * static_cast<size_t>(kNativeH);

// ST7305 packs 2×24 1bpp pixels → 6 bytes (U8g2 u8x8_st7305_convert_a0 maps).
// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays)
static const uint8_t kMap1[16] = {0x00, 0x02, 0x08, 0x0A, 0x20, 0x22, 0x28, 0x2A,
                                  0x80, 0x82, 0x88, 0x8A, 0xA0, 0xA2, 0xA8, 0xAA};
// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays)
static const uint8_t kMap2[16] = {0x00, 0x01, 0x04, 0x05, 0x10, 0x11, 0x14, 0x15,
                                  0x40, 0x41, 0x44, 0x45, 0x50, 0x51, 0x54, 0x55};

static inline bool isInk(uint16_t c) {
    const bool dark = ((c >> 5) & 0x3F) < 24;
    return g_invert ? !dark : dark;
}

/** Full 400×300 user framebuffer → native mono (U8G2_R1). */
static void packFullUserFb(uint8_t *buf, const uint16_t *fb) {
    for (int ny = 0; ny < kNativeH; ++ny) {
        const int ux = ny;
        uint8_t *dst = buf + ny * kTileW;
        for (int bx = 0; bx < kTileW; ++bx) {
            uint8_t bits = 0;
            const int nx0 = bx << 3;
            for (int b = 0; b < 8; ++b) {
                const int nx = nx0 + b;
                if (nx >= kNativeW) {
                    break;
                }
                const int uy = (kUserH - 1) - nx;
                if (isInk(fb[uy * kUserW + ux])) {
                    bits |= static_cast<uint8_t>(0x80 >> b);
                }
            }
            dst[bx] = bits;
        }
    }
}

static void packRectUserFb(uint8_t *buf, const uint16_t *src, int srcW, int srcH, int ox,
                           int oy) {
    const int x1 = ox + srcW;
    const int y1 = oy + srcH;
    const bool letterInk = isInk(0);
    for (int ny = 0; ny < kNativeH; ++ny) {
        const int ux = ny;
        const bool inX = (ux >= ox && ux < x1);
        uint8_t *dst = buf + ny * kTileW;
        for (int bx = 0; bx < kTileW; ++bx) {
            uint8_t bits = 0;
            const int nx0 = bx << 3;
            for (int b = 0; b < 8; ++b) {
                const int nx = nx0 + b;
                if (nx >= kNativeW) {
                    break;
                }
                const int uy = (kUserH - 1) - nx;
                bool ink;
                if (inX && uy >= oy && uy < y1) {
                    ink = isInk(src[(uy - oy) * srcW + (ux - ox)]);
                } else {
                    ink = letterInk;
                }
                if (ink) {
                    bits |= static_cast<uint8_t>(0x80 >> b);
                }
            }
            dst[bx] = bits;
        }
    }
}

static void convertBlock(uint8_t out[6], const uint8_t *row0, const uint8_t *row1) {
    out[0] = static_cast<uint8_t>(kMap1[row0[0] >> 4] | kMap2[row1[0] >> 4]);
    out[1] = static_cast<uint8_t>(kMap1[row0[0] & 0x0f] | kMap2[row1[0] & 0x0f]);
    out[2] = static_cast<uint8_t>(kMap1[row0[1] >> 4] | kMap2[row1[1] >> 4]);
    out[3] = static_cast<uint8_t>(kMap1[row0[1] & 0x0f] | kMap2[row1[1] & 0x0f]);
    out[4] = static_cast<uint8_t>(kMap1[row0[2] >> 4] | kMap2[row1[2] >> 4]);
    out[5] = static_cast<uint8_t>(kMap1[row0[2] & 0x0f] | kMap2[row1[2] & 0x0f]);
}

/**
 * U8g2 sendBuffer issues ~2600×6-byte SPI writes for this panel.
 * Keep CS asserted and send one converted dual-row (~78 bytes) per address set.
 */
static void sendSt7305Batched(uint8_t *tileBuf) {
    u8x8_t *u8x8 = g_u8g2->getU8x8();
    uint8_t line[78];

    u8x8_cad_StartTransfer(u8x8);
    for (int tileY = 0; tileY < kTileH; ++tileY) {
        uint8_t *base = tileBuf + tileY * kTileW * 8;
        const int y0 = tileY * 4;
        for (int i = 0; i < 4; ++i) {
            u8x8_cad_SendCmd(u8x8, 0x2a);
            u8x8_cad_SendArg(u8x8, 0x01);
            u8x8_cad_SendArg(u8x8, 0x2a);
            u8x8_cad_SendCmd(u8x8, 0x2b);
            u8x8_cad_SendArg(u8x8, static_cast<uint8_t>(y0 + i));
            u8x8_cad_SendArg(u8x8, static_cast<uint8_t>(y0 + i));
            u8x8_cad_SendCmd(u8x8, 0x2c);

            const uint8_t *row0 = base + kTileW * i * 2;
            const uint8_t *row1 = row0 + kTileW;
            int out = 0;
            for (int blk = 0; blk < 13; ++blk) {
                convertBlock(line + out, row0, row1);
                out += 6;
                row0 += 3;
                row1 += 3;
            }
            u8x8_cad_SendData(u8x8, static_cast<uint8_t>(out), line);
        }
    }
    u8x8_cad_EndTransfer(u8x8);
}

static void ensureDisplaySpi() {
    SPI.begin(PIN_RLCD42_SCK, -1, PIN_RLCD42_MOSI, PIN_RLCD42_CS);
}

static void ensureHpm() {
    if (!g_u8g2 || g_hpm) {
        return;
    }
    g_u8g2->sendF("c", 0x38);
    g_hpm = true;
}

static void flushMono(uint8_t *buf) {
    ensureHpm();
    sendSt7305Batched(buf);
}

/** U8G2_R1: userMono → native tile buffer. */
static void transposeUserToNative(uint8_t *buf, const uint8_t *userMono) {
    for (int ny = 0; ny < kNativeH; ++ny) {
        const int ux = ny;
        const uint8_t uxMask = static_cast<uint8_t>(0x80 >> (ux & 7));
        const int uxByte = ux >> 3;
        uint8_t *dst = buf + ny * kTileW;
        for (int bx = 0; bx < 37; ++bx) {
            uint8_t bits = 0;
            const int nx0 = bx << 3;
            for (int b = 0; b < 8; ++b) {
                const int uy = (kUserH - 1) - (nx0 + b);
                if (userMono[uy * kUserRowBytes + uxByte] & uxMask) {
                    bits |= static_cast<uint8_t>(0x80 >> b);
                }
            }
            dst[bx] = bits;
        }
        {
            uint8_t bits = 0;
            for (int b = 0; b < 4; ++b) {
                const int uy = (kUserH - 1) - (296 + b);
                if (userMono[uy * kUserRowBytes + uxByte] & uxMask) {
                    bits |= static_cast<uint8_t>(0x80 >> b);
                }
            }
            dst[37] = bits;
        }
    }
}

/** Fractional full-panel fit (optional; slower / softer). */
static void packClassic280Full(uint8_t *buf, const uint16_t *src280) {
    using esp_bracket::ClassicPresent;
    int outW = 0, outH = 0, ox = 0, oy = 0;
    ClassicPresent::computeOutSize(kUserW, kUserH, &outW, &outH, &ox, &oy);
    const int x1 = ox + outW;
    const int y1 = oy + outH;
    const bool letterInk = isInk(0);

    static uint16_t mapSx[kUserW];
    static uint16_t mapSy[kUserH];
    static int sOutW = -1, sOutH = -1, sOx = -1, sOy = -1;
    if (sOutW != outW || sOutH != outH || sOx != ox || sOy != oy) {
        sOutW = outW;
        sOutH = outH;
        sOx = ox;
        sOy = oy;
        for (int ux = 0; ux < kUserW; ++ux) {
            mapSx[ux] = (ux >= ox && ux < x1)
                            ? static_cast<uint16_t>(ClassicPresent::nnMapPreserveEdges(
                                  ux - ox, ClassicPresent::kSrcW, outW))
                            : 0;
        }
        for (int uy = 0; uy < kUserH; ++uy) {
            mapSy[uy] = (uy >= oy && uy < y1)
                            ? static_cast<uint16_t>(ClassicPresent::nnMapPreserveEdges(
                                  uy - oy, ClassicPresent::kSrcH, outH))
                            : 0;
        }
    }

    static uint8_t userMono[kUserH * kUserRowBytes];
    memset(userMono, letterInk ? 0xFFu : 0x00u, sizeof(userMono));
    for (int uy = oy; uy < y1; ++uy) {
        const uint16_t *srcRow = src280 + mapSy[uy] * ClassicPresent::kSrcW;
        uint8_t *dst = userMono + uy * kUserRowBytes;
        for (int ux = ox; ux < x1; ++ux) {
            const bool ink = isInk(srcRow[mapSx[ux]]);
            if (letterInk) {
                if (!ink) {
                    dst[ux >> 3] &= static_cast<uint8_t>(~(0x80 >> (ux & 7)));
                }
            } else if (ink) {
                dst[ux >> 3] |= static_cast<uint8_t>(0x80 >> (ux & 7));
            }
        }
    }
    transposeUserToNative(buf, userMono);
}

/**
 * Integer classic present (default): 1× → 280×192 centered on 400×300.
 * Exact s×s blocks when a larger integer scale fits.
 */
static void packClassic280Integer(uint8_t *buf, const uint16_t *src280) {
    using esp_bracket::ClassicPresent;
    int outW = 0, outH = 0, ox = 0, oy = 0;
    ClassicPresent::computeOutSizeInteger(kUserW, kUserH, &outW, &outH, &ox, &oy);
    const int scale = outW / ClassicPresent::kSrcW;
    const bool letterInk = isInk(0);

    static uint8_t userMono[kUserH * kUserRowBytes];
    memset(userMono, letterInk ? 0xFFu : 0x00u, sizeof(userMono));

    if (scale <= 1) {
        for (int sy = 0; sy < ClassicPresent::kSrcH; ++sy) {
            const uint16_t *srcRow = src280 + sy * ClassicPresent::kSrcW;
            uint8_t *dst = userMono + (oy + sy) * kUserRowBytes;
            for (int sx = 0; sx < ClassicPresent::kSrcW; ++sx) {
                const int ux = ox + sx;
                const bool ink = isInk(srcRow[sx]);
                if (letterInk) {
                    if (!ink) {
                        dst[ux >> 3] &= static_cast<uint8_t>(~(0x80 >> (ux & 7)));
                    }
                } else if (ink) {
                    dst[ux >> 3] |= static_cast<uint8_t>(0x80 >> (ux & 7));
                }
            }
        }
    } else {
        for (int sy = 0; sy < ClassicPresent::kSrcH; ++sy) {
            const uint16_t *srcRow = src280 + sy * ClassicPresent::kSrcW;
            for (int dy = 0; dy < scale; ++dy) {
                uint8_t *dst = userMono + (oy + sy * scale + dy) * kUserRowBytes;
                for (int sx = 0; sx < ClassicPresent::kSrcW; ++sx) {
                    const bool ink = isInk(srcRow[sx]);
                    for (int dx = 0; dx < scale; ++dx) {
                        const int ux = ox + sx * scale + dx;
                        if (letterInk) {
                            if (!ink) {
                                dst[ux >> 3] &= static_cast<uint8_t>(~(0x80 >> (ux & 7)));
                            }
                        } else if (ink) {
                            dst[ux >> 3] |= static_cast<uint8_t>(0x80 >> (ux & 7));
                        }
                    }
                }
            }
        }
    }

    transposeUserToNative(buf, userMono);
}

static void packClassic280(uint8_t *buf, const uint16_t *src280) {
    if (g_classicFitFull) {
        packClassic280Full(buf, src280);
    } else {
        packClassic280Integer(buf, src280);
    }
}

} // namespace

bool esp2DisplayInit(Arduino_DataBus **busOut, Arduino_GFX **gfxOut) {
    if (!busOut || !gfxOut) {
        return false;
    }
    *busOut = nullptr;
    *gfxOut = nullptr;

    ensureDisplaySpi();
    g_u8g2 = new U8G2_ST7305_300X400_F_4W_HW_SPI(U8G2_R1, PIN_RLCD42_CS, PIN_RLCD42_DC,
                                                  PIN_RLCD42_RST);
    if (!g_u8g2->begin()) {
        Serial.println("[DISPLAY][FAIL] ST7305 U8g2 begin");
        delete g_u8g2;
        g_u8g2 = nullptr;
        return false;
    }
    g_u8g2->setBusClock(40000000);
    g_u8g2->sendF("c", 0x38);
    g_hpm = true;

    g_canvas = new Arduino_Canvas(RLCD42_LCD_WIDTH, RLCD42_LCD_HEIGHT, nullptr);
    if (!g_canvas->begin()) {
        Serial.println("[DISPLAY][FAIL] Canvas begin");
        delete g_canvas;
        g_canvas = nullptr;
        return false;
    }
    g_canvas->fillScreen(0x0000);
    g_panelReady = true;
    *gfxOut = g_canvas;

    int ow = 0, oh = 0, ox = 0, oy = 0;
    esp_bracket::ClassicPresent::computeOutSizeInteger(RLCD42_LCD_WIDTH, RLCD42_LCD_HEIGHT, &ow,
                                                       &oh, &ox, &oy);
    Serial.printf("[DISPLAY] ST7305 canvas OK %dx%d classic_default=1x %dx%d@%d,%d\n",
                  RLCD42_LCD_WIDTH, RLCD42_LCD_HEIGHT, ow, oh, ox, oy);
    return true;
}

void esp2DisplaySleep(Arduino_GFX * /*gfx*/) {
    if (!g_u8g2) {
        return;
    }
    g_u8g2->sendF("c", 0x39);
    g_hpm = false;
}

void esp2DisplayWake(Arduino_GFX * /*gfx*/, uint8_t /*brightness*/) {
    if (!g_u8g2) {
        return;
    }
    g_u8g2->setPowerSave(0);
    g_u8g2->sendF("c", 0x38);
    g_hpm = true;
}

void esp2DisplaySetBrightness(Arduino_GFX * /*gfx*/, uint8_t /*brightness*/) {
}

void esp2DisplaySetInvert(bool invert) {
    g_invert = invert;
}

bool esp2DisplayInvert() {
    return g_invert;
}

void esp2DisplaySetClassicFitFull(bool full) {
    g_classicFitFull = full;
}

bool esp2DisplayClassicFitFull() {
    return g_classicFitFull;
}

void esp2DisplayFlush(Arduino_GFX *gfx) {
    if (!g_panelReady || !g_u8g2 || !gfx) {
        return;
    }
    auto *canvas = static_cast<Arduino_Canvas *>(gfx);
    uint16_t *fb = reinterpret_cast<uint16_t *>(canvas->getFramebuffer());
    if (!fb) {
        return;
    }
    uint8_t *buf = g_u8g2->getBufferPtr();
    if (!buf) {
        return;
    }
    packFullUserFb(buf, fb);
    flushMono(buf);
}

void esp2DisplayFlushRgb565(const uint16_t *src, int srcW, int srcH, int ox, int oy) {
    if (!g_panelReady || !g_u8g2 || !src || srcW <= 0 || srcH <= 0) {
        return;
    }
    uint8_t *buf = g_u8g2->getBufferPtr();
    if (!buf) {
        return;
    }
    if (ox == 0 && oy == 0 && srcW == kUserW && srcH == kUserH) {
        packFullUserFb(buf, src);
    } else {
        packRectUserFb(buf, src, srcW, srcH, ox, oy);
    }
    flushMono(buf);
}

void esp2DisplayFlushClassic280(const uint16_t *src280x192) {
    if (!g_panelReady || !g_u8g2 || !src280x192) {
        return;
    }
    uint8_t *buf = g_u8g2->getBufferPtr();
    if (!buf) {
        return;
    }
    const uint32_t t0 = micros();
    packClassic280(buf, src280x192);
    const uint32_t t1 = micros();
    flushMono(buf);
    const uint32_t t2 = micros();
    static uint32_t sLastLog;
    const uint32_t now = millis();
    if (now - sLastLog >= 5000) {
        sLastLog = now;
        Serial.printf("[DISPLAY] rlcd pack_us=%u send_us=%u total_us=%u\n",
                      static_cast<unsigned>(t1 - t0), static_cast<unsigned>(t2 - t1),
                      static_cast<unsigned>(t2 - t0));
    }
}
