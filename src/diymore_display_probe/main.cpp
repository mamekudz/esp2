/**
 * diymore ES3C28P — LCD color probe (ILI9341V SPI).
 *
 * Pins from LCDWIKI vendor docs (PENDING_VISUAL confirmation by user).
 * Draws labeled color bars + TOP/BOTTOM/LEFT/RIGHT markers.
 * Does not run Apple II emulation.
 */
#include <Arduino.h>
#include <SPI.h>
#include <Arduino_GFX_Library.h>

#include "boards/diymore_es3c28p.h"

static Arduino_DataBus *g_bus = nullptr;
static Arduino_ILI9341 *g_gfx = nullptr;

static void logLine(const char *line) {
  Serial.println(line);
}

static void drawLabeledBar(int16_t y, int16_t h, uint16_t color, const char *label) {
  g_gfx->fillRect(0, y, DIYMORE_LCD_WIDTH, h, color);
  g_gfx->setTextColor(RGB565_WHITE);
  g_gfx->setCursor(8, y + (h / 2) - 4);
  g_gfx->print(label);
}

static void drawOrientationMarks() {
  const int16_t w = DIYMORE_LCD_WIDTH;
  const int16_t h = DIYMORE_LCD_HEIGHT;
  g_gfx->setTextSize(2);
  g_gfx->setTextColor(RGB565_YELLOW);
  g_gfx->setCursor(w / 2 - 20, 8);
  g_gfx->print("TOP");
  g_gfx->setCursor(w / 2 - 36, h - 24);
  g_gfx->print("BOTTOM");
  g_gfx->setCursor(4, h / 2 - 8);
  g_gfx->print("L");
  g_gfx->setCursor(w - 20, h / 2 - 8);
  g_gfx->print("R");
  g_gfx->drawRect(0, 0, w, h, RGB565_WHITE);
}

void setup() {
  Serial.begin(115200);
  delay(600);

  logLine("[BOOT] ESP32-S3 diymore_display_probe");
  logLine("[PLATFORM] diymore-es3c28p part=AM03-FBBA0125-AAA");
  logLine("[DISPLAY] source=LCDWIKI_ES3C28P status=VENDOR_DOCUMENTED");
  Serial.printf("[DISPLAY] ILI9341V %dx%d SPI cs=%d dc=%d sck=%d mosi=%d miso=%d bl=%d rst=CHIP_PU\n",
                DIYMORE_LCD_WIDTH, DIYMORE_LCD_HEIGHT, PIN_DIYMORE_TFT_CS, PIN_DIYMORE_TFT_DC,
                PIN_DIYMORE_TFT_SCK, PIN_DIYMORE_TFT_MOSI, PIN_DIYMORE_TFT_MISO, PIN_DIYMORE_TFT_BL);

  pinMode(PIN_DIYMORE_TFT_BL, OUTPUT);
  digitalWrite(PIN_DIYMORE_TFT_BL, HIGH);

  g_bus = new Arduino_ESP32SPI(PIN_DIYMORE_TFT_DC, PIN_DIYMORE_TFT_CS, PIN_DIYMORE_TFT_SCK,
                               PIN_DIYMORE_TFT_MOSI, PIN_DIYMORE_TFT_MISO, HSPI);
  // RST shared with CHIP_PU → GFX_NOT_DEFINED; IPS=true per vendor IPS panel.
  g_gfx = new Arduino_ILI9341(g_bus, GFX_NOT_DEFINED, 0 /* rotation */, true /* IPS */,
                              DIYMORE_LCD_WIDTH, DIYMORE_LCD_HEIGHT);

  if (!g_gfx->begin()) {
    logLine("[DISPLAY][FAIL] ILI9341 begin() returned false");
    logLine("[SELFTEST] diymore_display_probe FAIL");
    return;
  }

  logLine("[DISPLAY] ILI9341 begin OK");
  g_gfx->fillScreen(RGB565_BLACK);

  const int16_t barH = DIYMORE_LCD_HEIGHT / 6;
  drawLabeledBar(0 * barH, barH, RGB565_RED, "RED");
  drawLabeledBar(1 * barH, barH, RGB565_GREEN, "GREEN");
  drawLabeledBar(2 * barH, barH, RGB565_BLUE, "BLUE");
  drawLabeledBar(3 * barH, barH, RGB565_WHITE, "WHITE");
  drawLabeledBar(4 * barH, barH, RGB565_YELLOW, "YELLOW");
  drawLabeledBar(5 * barH, DIYMORE_LCD_HEIGHT - 5 * barH, RGB565_CYAN, "CYAN");
  drawOrientationMarks();

  g_gfx->setTextSize(1);
  g_gfx->setTextColor(RGB565_BLACK);
  g_gfx->setCursor(8, DIYMORE_LCD_HEIGHT - 40);
  g_gfx->print("ES3C28P probe — confirm colors/orientation");

  logLine("[SELFTEST] diymore_display_probe PASS (begin OK — VISUAL confirm required)");
  logLine("[TODO] user must confirm color bars + TOP/BOTTOM/LEFT/RIGHT");
}

void loop() {
  delay(5000);
  Serial.printf("[HB] uptime_ms=%lu heap=%u\n", static_cast<unsigned long>(millis()),
                static_cast<unsigned>(ESP.getFreeHeap()));
}
