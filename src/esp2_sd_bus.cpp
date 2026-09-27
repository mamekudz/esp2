#include "esp2_sd_bus.hpp"

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
