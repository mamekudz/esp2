#pragma once

#include <Arduino_GFX_Library.h>
#include <stdint.h>

/**
 * Panel display HAL — Waveshare CO5300 / diymore ILI9341 / RLCD ST7305.
 * Apple II core talks only to Arduino_GFX blit APIs after init.
 */
bool esp2DisplayInit(Arduino_DataBus **busOut, Arduino_GFX **gfxOut);
void esp2DisplaySleep(Arduino_GFX *gfx);
void esp2DisplayWake(Arduino_GFX *gfx, uint8_t brightness);
/** Panel backlight / OLED brightness 0–255 (0 = off). */
void esp2DisplaySetBrightness(Arduino_GFX *gfx, uint8_t brightness);
/**
 * Push off-screen / canvas pixels to a panel that is not RGB565-native
 * (ST7305). No-op on CO5300 / ILI9341.
 */
void esp2DisplayFlush(Arduino_GFX *gfx);

/**
 * Push an RGB565 rect (letterboxed on the logical panel) without a canvas blit.
 * Used by RLCD present path. No-op on CO5300 / ILI9341.
 */
void esp2DisplayFlushRgb565(const uint16_t *src, int srcW, int srcH, int ox, int oy);

/**
 * RLCD: nearest-neighbor scale Apple II 280×192 RGB565 → mono panel in one pass
 * (skips the intermediate presentFb). No-op on color panels.
 * Uses integer 1× (default) or full-panel fit when esp2DisplaySetClassicFitFull(true).
 */
void esp2DisplayFlushClassic280(const uint16_t *src280x192);

/** Mono RLCD: swap paper/ink polarity. No-op on color panels. */
void esp2DisplaySetInvert(bool invert);
bool esp2DisplayInvert();

/**
 * RLCD classic present: false = integer 1× (280×192 + letterbox, default);
 * true = fractional full-panel fit. No-op on color panels.
 */
void esp2DisplaySetClassicFitFull(bool full);
bool esp2DisplayClassicFitFull();
