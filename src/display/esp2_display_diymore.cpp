#include "esp2_display_hal.h"
#include "boards/diymore_es3c28p.h"

#include <Arduino.h>
#include <SPI.h>

namespace {

constexpr int kBlPwmFreqHz = 5000;
constexpr int kBlPwmBits = 8;
uint8_t g_blLevel = 180;
bool g_blPwmReady = false;

void ensureBlPwm() {
  if (g_blPwmReady) {
    return;
  }
  // Arduino-ESP32 3.x: pin-based LEDC API.
  if (!ledcAttach(PIN_DIYMORE_TFT_BL, kBlPwmFreqHz, kBlPwmBits)) {
    pinMode(PIN_DIYMORE_TFT_BL, OUTPUT);
    digitalWrite(PIN_DIYMORE_TFT_BL, g_blLevel > 0 ? HIGH : LOW);
    return;
  }
  g_blPwmReady = true;
}

void writeBl(uint8_t level) {
  g_blLevel = level;
  ensureBlPwm();
  if (g_blPwmReady) {
    ledcWrite(PIN_DIYMORE_TFT_BL, level);
  } else {
    digitalWrite(PIN_DIYMORE_TFT_BL, level > 0 ? HIGH : LOW);
  }
}

} // namespace

bool esp2DisplayInit(Arduino_DataBus **busOut, Arduino_GFX **gfxOut) {
  if (!busOut || !gfxOut) {
    return false;
  }
  writeBl(g_blLevel);

  *busOut = new Arduino_ESP32SPI(PIN_DIYMORE_TFT_DC, PIN_DIYMORE_TFT_CS, PIN_DIYMORE_TFT_SCK,
                                 PIN_DIYMORE_TFT_MOSI, PIN_DIYMORE_TFT_MISO, HSPI);
  auto *gfx = new Arduino_ILI9341(*busOut, GFX_NOT_DEFINED, 0 /* rotation */, true /* IPS */,
                                  DIYMORE_LCD_WIDTH, DIYMORE_LCD_HEIGHT);
  if (!gfx->begin(40000000)) {
    Serial.println("[DISPLAY][FAIL] ILI9341 begin");
    *gfxOut = nullptr;
    return false;
  }
  *gfxOut = gfx;
  Serial.printf("[DISPLAY] ILI9341 init OK %dx%d SPI@40MHz bl=%u\n", gfx->width(), gfx->height(),
                static_cast<unsigned>(g_blLevel));
  return true;
}

void esp2DisplaySleep(Arduino_GFX *gfx) {
  if (gfx) {
    gfx->displayOff();
  }
  writeBl(0);
}

void esp2DisplayWake(Arduino_GFX *gfx, uint8_t brightness) {
  writeBl(brightness);
  if (gfx) {
    gfx->displayOn();
  }
}

void esp2DisplaySetBrightness(Arduino_GFX * /*gfx*/, uint8_t brightness) {
  writeBl(brightness);
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

