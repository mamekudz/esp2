#include "esp2_sd_bus.hpp"

#if defined(ESP2_PLATFORM_DIYMORE_ES3C28P)

#include "boards/diymore_es3c28p.h"

void esp2SdBusEnd() {
    SD_MMC.end();
}

bool esp2SdBusBegin(uint32_t /*hz*/) {
    // LCDWIKI ES3C28P: SDIO 4-bit — CLK/CMD/D0..D3
    if (!SD_MMC.setPins(PIN_DIYMORE_SD_CLK, PIN_DIYMORE_SD_CMD, PIN_DIYMORE_SD_D0, PIN_DIYMORE_SD_D1,
                        PIN_DIYMORE_SD_D2, PIN_DIYMORE_SD_D3)) {
        Serial.println("[SD][FAIL] SD_MMC.setPins");
        return false;
    }
    // Mount at /sd — same VFS root convention as Waveshare SPI SD.begin().
    // mode1bit=false → 4-bit
    if (!SD_MMC.begin("/sd", false)) {
        Serial.println("[SD][FAIL] SD_MMC.begin 4-bit — trying 1-bit");
        if (!SD_MMC.begin("/sd", true)) {
            Serial.println("[SD][FAIL] SD_MMC.begin 1-bit");
            return false;
        }
        Serial.printf("[SD] mounted bus=SDMMC_1bit type=%u size_MB=%llu\n",
                      static_cast<unsigned>(SD_MMC.cardType()),
                      static_cast<unsigned long long>(SD_MMC.cardSize() / (1024ULL * 1024ULL)));
        return true;
    }
    Serial.printf("[SD] mounted bus=SDMMC_4bit type=%u size_MB=%llu\n",
                  static_cast<unsigned>(SD_MMC.cardType()),
                  static_cast<unsigned long long>(SD_MMC.cardSize() / (1024ULL * 1024ULL)));
    return true;
}

#elif defined(ESP2_PLATFORM_WAVESHARE_RLCD42)

#include "boards/waveshare_rlcd42.h"

void esp2SdBusEnd() {
    SD_MMC.end();
}

bool esp2SdBusBegin(uint32_t /*hz*/) {
    // Schematic: TF is SDMMC 1-bit (CLK/CMD/D0). R7 (CS→GPIO17) is unpopulated.
    // NEVER use Waveshare AMOLED SPI SD pins (40/41) — those are ST7305 CS/RST here.
    if (!SD_MMC.setPins(PIN_RLCD42_SD_CLK, PIN_RLCD42_SD_CMD, PIN_RLCD42_SD_D0)) {
        Serial.println("[SD][FAIL] SD_MMC.setPins rlcd42");
        return false;
    }
    if (!SD_MMC.begin("/sd", true)) {
        Serial.println("[SD][FAIL] SD_MMC.begin 1-bit");
        return false;
    }
    Serial.printf("[SD] mounted bus=SDMMC_1bit type=%u size_MB=%llu\n",
                  static_cast<unsigned>(SD_MMC.cardType()),
                  static_cast<unsigned long long>(SD_MMC.cardSize() / (1024ULL * 1024ULL)));
    return true;
}

#else

#include <SD.h>

#include "board_pins.h"

#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32S2)
// HSPI → SPI3_HOST on S2/S3 (FSPI is SPI2, claimed by CO5300 QSPI).
static SPIClass g_sdSpi(HSPI);
#else
static SPIClass g_sdSpi(FSPI);
#endif

SPIClass &esp2SdSpi() {
    return g_sdSpi;
}

void esp2SdBusEnd() {
    SD.end();
    g_sdSpi.end();
}

bool esp2SdBusBegin(uint32_t hz) {
    g_sdSpi.begin(PIN_SD_SCLK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
    delay(10);
    return SD.begin(PIN_SD_CS, g_sdSpi, hz);
}

#endif
