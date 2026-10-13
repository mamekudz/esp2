#pragma once

#include <Arduino_GFX_Library.h>

#include <cstdint>

/** On-device settings / BLE input device management (touch + optional keyboard nav). */
namespace esp2_settings {

bool isOpen();
void open();
void close();

/** Draw full-screen overlay; call when isOpen() from display task. */
void draw(Arduino_GFX *gfx, int panelW, int panelH);

/**
 * Touch tap at panel coordinates. Returns true if consumed.
 * wakeConsumed: first touch after screensaver wake — ignore activation.
 */
bool onTouchTap(uint16_t x, uint16_t y, int panelW, int panelH, bool wakeConsumed);

/** Optional keyboard navigation when settings open (BLE keyboard). */
void onKeyboardNav(bool up, bool down, bool activate, bool back);

/** Periodic tick (scan progress, auto-return after pair success). */
void tick(uint32_t nowMs);

} // namespace esp2_settings
