/**
 * Waveshare / Luckfox ESP32-S3-RLCD-4.2 — ST7305 display probe.
 *
 * Draws orientation marks + greyscale bars. User must visually confirm.
 * Ambient light required (reflective panel, no backlight).
 */
#include <Arduino.h>
#include <SPI.h>
#include <U8g2lib.h>

#include "boards/waveshare_rlcd42.h"

// Landscape logical 400×300 (U8G2_R1). CS=40 DC=5 RST=41.
static U8G2_ST7305_300X400_F_4W_HW_SPI g_u8g2(U8G2_R1, PIN_RLCD42_CS, PIN_RLCD42_DC,
                                               PIN_RLCD42_RST);

static void drawProbe() {
    g_u8g2.clearBuffer();
    g_u8g2.setFont(u8g2_font_6x12_tf);

    const int w = RLCD42_LCD_WIDTH;
    const int h = RLCD42_LCD_HEIGHT;
    g_u8g2.drawFrame(0, 0, w, h);

    g_u8g2.drawStr(w / 2 - 12, 14, "TOP");
    g_u8g2.drawStr(w / 2 - 24, h - 6, "BOTTOM");
    g_u8g2.drawStr(4, h / 2, "L");
    g_u8g2.drawStr(w - 12, h / 2, "R");

    const int barH = (h - 40) / 4;
    for (int i = 0; i < 4; ++i) {
        const int y0 = 20 + i * barH;
        if ((i % 2) == 0) {
            g_u8g2.drawBox(40, y0, w - 80, barH - 2);
        } else {
            g_u8g2.drawFrame(40, y0, w - 80, barH - 2);
            for (int x = 40; x < w - 40; x += 4) {
                g_u8g2.drawVLine(x, y0, barH - 2);
            }
        }
    }

    g_u8g2.drawStr(48, 32, "BLACK");
    g_u8g2.drawStr(48, 32 + barH, "HATCH");
    g_u8g2.drawStr(48, 32 + 2 * barH, "SOLID");
    g_u8g2.drawStr(48, 32 + 3 * barH, "HATCH");
    g_u8g2.drawStr(48, h - 28, "RLCD42 probe — confirm marks");

    g_u8g2.sendBuffer();
}

void setup() {
    Serial.begin(115200);
    delay(600);
    Serial.println("[BOOT] ESP32-S3 rlcd42_display_probe");
    Serial.println("[PLATFORM] waveshare-rlcd42 product=ESP32-S3-RLCD-4.2");
    Serial.printf("[DISPLAY] ST7305 %dx%d SPI sck=%d mosi=%d cs=%d dc=%d rst=%d\n",
                  RLCD42_LCD_WIDTH, RLCD42_LCD_HEIGHT, PIN_RLCD42_SCK, PIN_RLCD42_MOSI,
                  PIN_RLCD42_CS, PIN_RLCD42_DC, PIN_RLCD42_RST);

    SPI.begin(PIN_RLCD42_SCK, -1, PIN_RLCD42_MOSI, PIN_RLCD42_CS);
    if (!g_u8g2.begin()) {
        Serial.println("[DISPLAY][FAIL] U8g2 ST7305 begin");
        Serial.println("[SELFTEST] rlcd42_display_probe FAIL");
        return;
    }
    g_u8g2.setBusClock(12000000);
    // HPM before first paint; short settle (some panels show noise on first frame).
    g_u8g2.sendF("c", 0x38);
    delay(100);
    Serial.println("[DISPLAY] ST7305 begin OK");
    drawProbe();
    delay(50);
    drawProbe(); // second full refresh clears residual GRAM noise on some units
    // Low-power mode after static image (Waveshare RLCD guide).
    g_u8g2.sendF("c", 0x39);
    Serial.println("[SELFTEST] rlcd42_display_probe PASS (begin OK — VISUAL confirm required)");
    Serial.println("[TODO] user must confirm TOP/BOTTOM/LEFT/RIGHT + bars in ambient light");
}

void loop() {
    delay(5000);
    Serial.printf("[HB] uptime_ms=%lu heap=%u\n", static_cast<unsigned long>(millis()),
                  static_cast<unsigned>(ESP.getFreeHeap()));
}
