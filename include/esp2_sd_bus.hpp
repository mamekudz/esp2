#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <stdint.h>

/**
 * microSD SPI bus helper for Waveshare ESP32-S3-Touch-AMOLED-1.64.
 *
 * CO5300 QSPI (Arduino_ESP32QSPI) owns SPI2_HOST. The Arduino default
 * SPIClass (FSPI) is also SPI2 — sharing it with the display breaks mid-run
 * FAT remounts (#NAK sd / mkdir_esp2). microSD therefore uses SPI3 (HSPI).
 */
SPIClass &esp2SdSpi();

/** End FAT mount and release the SD SPI bus. */
void esp2SdBusEnd();

/**
 * Begin SPI3 + mount FAT at /sd.
 * @param hz SPI clock (default 4 MHz — matches verified bring-up).
 */
bool esp2SdBusBegin(uint32_t hz = 4000000);
