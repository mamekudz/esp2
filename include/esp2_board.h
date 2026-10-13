#pragma once

/**
 * Active board selection for ESP][ firmware (one place for platform #ifdef).
 * Emulator code includes this instead of board_pins.h directly.
 */
#if defined(ESP2_PLATFORM_DIYMORE_ES3C28P)
#include "boards/diymore_es3c28p.h"
#define ESP2_PANEL_WIDTH DIYMORE_LCD_WIDTH
#define ESP2_PANEL_HEIGHT DIYMORE_LCD_HEIGHT
/** Classic 1:1 letterbox Y is Waveshare-only; diymore uses centered scale. */
#define ESP2_CLASSIC_LETTERBOX_Y 0
#define ESP2_PANEL_NEEDS_CLASSIC_SCALE 1
#elif defined(ESP2_PLATFORM_WAVESHARE_RLCD42)
#include "boards/waveshare_rlcd42.h"
#define ESP2_PANEL_WIDTH RLCD42_LCD_WIDTH
#define ESP2_PANEL_HEIGHT RLCD42_LCD_HEIGHT
#define ESP2_CLASSIC_LETTERBOX_Y 0
#define ESP2_PANEL_NEEDS_CLASSIC_SCALE 1
#else
#include "board_pins.h"
#include "boards/waveshare_amoled_s3.h"
#define ESP2_PANEL_WIDTH LCD_WIDTH
#define ESP2_PANEL_HEIGHT LCD_HEIGHT
#define ESP2_CLASSIC_LETTERBOX_Y 48
#define ESP2_PANEL_NEEDS_CLASSIC_SCALE 0
#endif
