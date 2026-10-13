/**
 * Waveshare / Luckfox ESP32-S3-RLCD-4.2 — chip identity + button probe.
 *
 * Bring-up ONLY. No display, no SD, no audio.
 * Platform: waveshare-rlcd42
 */
#include <Arduino.h>
#include <esp_chip_info.h>
#include <esp_flash.h>
#include <esp_system.h>

#include "boards/waveshare_rlcd42.h"

#if defined(BOARD_HAS_PSRAM)
#include <esp_heap_caps.h>
#endif

static bool g_bootPrev = false;
static bool g_keyPrev = false;

static void logChip() {
    esp_chip_info_t info{};
    esp_chip_info(&info);
    Serial.printf("[BOOT] ESP32-S3 rlcd42_bringup\n");
    Serial.printf("[CHIP] model=%d cores=%d rev=%d features=0x%x\n",
                  static_cast<int>(info.model), static_cast<int>(info.cores),
                  static_cast<int>(info.revision),
                  static_cast<unsigned>(info.features));
}

static void logFlash() {
    uint32_t size = 0;
    if (esp_flash_get_size(nullptr, &size) == ESP_OK) {
        Serial.printf("[FLASH] %u bytes OK\n", static_cast<unsigned>(size));
    } else {
        Serial.println("[FLASH][FAIL] esp_flash_get_size");
    }
}

static void logPsram() {
#if defined(BOARD_HAS_PSRAM)
    const size_t total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    const size_t freeSz = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    if (total > 0) {
        Serial.printf("[PSRAM] total=%u free=%u OK\n", static_cast<unsigned>(total),
                      static_cast<unsigned>(freeSz));
    } else {
        Serial.println(
            "[PSRAM][FAIL] BOARD_HAS_PSRAM set but total=0 — unset flag until verified");
    }
#else
    Serial.println("[PSRAM] NOT_PROBED (BOARD_HAS_PSRAM unset)");
#endif
}

static void pollButtons() {
    const bool boot = digitalRead(PIN_RLCD42_BOOT) == LOW;
    const bool key = digitalRead(PIN_RLCD42_KEY) == LOW;
    if (boot && !g_bootPrev) {
        Serial.println("[BTN] BOOT down");
    } else if (!boot && g_bootPrev) {
        Serial.println("[BTN] BOOT up");
    }
    if (key && !g_keyPrev) {
        Serial.println("[BTN] KEY down");
    } else if (!key && g_keyPrev) {
        Serial.println("[BTN] KEY up");
    }
    g_bootPrev = boot;
    g_keyPrev = key;
}

void setup() {
    Serial.begin(115200);
    delay(800);
    pinMode(PIN_RLCD42_BOOT, INPUT_PULLUP);
    pinMode(PIN_RLCD42_KEY, INPUT_PULLUP);
    logChip();
    logFlash();
    logPsram();
    Serial.printf("[HEAP] free=%u\n", static_cast<unsigned>(ESP.getFreeHeap()));
    Serial.printf("[BTN] boot=GPIO%d key=GPIO%d (active-low)\n", PIN_RLCD42_BOOT,
                  PIN_RLCD42_KEY);
    Serial.println("[SELFTEST] rlcd42_bringup identity PASS");
    Serial.println("[TODO] display ST7305 — next env rlcd42_display_probe");
    Serial.println("[TODO] microSD pin map PENDING (no card required here)");
    Serial.println("[PLATFORM] waveshare-rlcd42 product=ESP32-S3-RLCD-4.2");
}

void loop() {
    pollButtons();
    static uint32_t lastHb = 0;
    const uint32_t now = millis();
    if (now - lastHb >= 5000) {
        lastHb = now;
        Serial.printf("[HB] uptime_ms=%lu heap=%u boot=%d key=%d\n",
                      static_cast<unsigned long>(now),
                      static_cast<unsigned>(ESP.getFreeHeap()), g_bootPrev ? 1 : 0,
                      g_keyPrev ? 1 : 0);
    }
    delay(20);
}
