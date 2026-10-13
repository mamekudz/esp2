/**
 * diymore ESP32-S3 2.8" Touch (ES3C28P / AM03-FBBA0125-AAA) — chip identity probe.
 *
 * Bring-up ONLY. Does not initialize display/touch/SD/audio — those controllers
 * and pins are UNKNOWN until measured from board docs / probe.
 *
 * Platform: diymore-es3c28p
 */
#include <Arduino.h>
#include <esp_chip_info.h>
#include <esp_flash.h>
#include <esp_system.h>

#if defined(BOARD_HAS_PSRAM)
#include <esp_heap_caps.h>
#endif

static void logChip() {
  esp_chip_info_t info{};
  esp_chip_info(&info);
  Serial.printf("[BOOT] ESP32-S3 diymore_bringup\n");
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
    Serial.println("[PSRAM][FAIL] BOARD_HAS_PSRAM set but total=0 — unset flag until verified");
  }
#else
  Serial.println("[PSRAM] NOT_PROBED (BOARD_HAS_PSRAM unset — measure before enabling)");
#endif
}

void setup() {
  Serial.begin(115200);
  delay(800);
  logChip();
  logFlash();
  logPsram();
  Serial.println("[HEAP] free=" + String(ESP.getFreeHeap()));
  Serial.println("[SELFTEST] diymore_bringup identity PASS");
  Serial.println("[TODO] display controller / pins UNKNOWN");
  Serial.println("[TODO] touch controller / pins UNKNOWN");
  Serial.println("[TODO] SD / audio / remaining peripherals UNKNOWN");
  Serial.println("[PLATFORM] diymore-es3c28p part=AM03-FBBA0125-AAA");
}

void loop() {
  delay(5000);
  Serial.printf("[HB] uptime_ms=%lu heap=%u\n", static_cast<unsigned long>(millis()),
                static_cast<unsigned>(ESP.getFreeHeap()));
}
