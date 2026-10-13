#include "esp2_display_hal.h"
#include "board_pins.h"

#include <Arduino.h>

bool esp2DisplayInit(Arduino_DataBus **busOut, Arduino_GFX **gfxOut) {
  if (!busOut || !gfxOut) {
    return false;
  }
  *busOut = new Arduino_ESP32QSPI(PIN_LCD_CS, PIN_LCD_SCK, PIN_LCD_D0, PIN_LCD_D1, PIN_LCD_D2,
                                  PIN_LCD_D3);
  auto *gfx = new Arduino_CO5300(*busOut, PIN_LCD_RST, 0, LCD_WIDTH, LCD_HEIGHT, LCD_COL_OFFSET1,
                                 LCD_ROW_OFFSET1, LCD_COL_OFFSET2, LCD_ROW_OFFSET2);
  if (!gfx->begin()) {
    Serial.println("[DISPLAY][FAIL] CO5300 begin");
    *gfxOut = nullptr;
    return false;
  }
  *gfxOut = gfx;
  Serial.printf("[DISPLAY] CO5300 init OK %dx%d\n", gfx->width(), gfx->height());
  return true;
}

void esp2DisplaySleep(Arduino_GFX *gfx) {
  if (gfx) {
    gfx->displayOff();
  }
}

void esp2DisplayWake(Arduino_GFX *gfx, uint8_t brightness) {
  if (!gfx) {
    return;
  }
  gfx->displayOn();
  // CO5300 supports setBrightness; base Arduino_GFX may not — cast via CO5300 path.
  static_cast<Arduino_CO5300 *>(gfx)->setBrightness(brightness);
}

void esp2DisplaySetBrightness(Arduino_GFX *gfx, uint8_t brightness) {
  if (!gfx) {
    return;
  }
  static_cast<Arduino_CO5300 *>(gfx)->setBrightness(brightness);
}

void esp2DisplayFlush(Arduino_GFX * /*gfx*/) {}

void esp2DisplayFlushRgb565(const uint16_t * /*src*/, int /*srcW*/, int /*srcH*/, int /*ox*/,
                            int /*oy*/) {}

void esp2DisplayFlushClassic280(const uint16_t * /*src280x192*/) {}

void esp2DisplaySetClassicFitFull(bool /*full*/) {}

bool esp2DisplayClassicFitFull() {
    return false;
}


void esp2DisplaySetInvert(bool /*invert*/) {}

bool esp2DisplayInvert() {
    return false;
}

