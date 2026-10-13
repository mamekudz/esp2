#pragma once

#include <Arduino.h>
#include <FS.h>
#include <stdint.h>

#if defined(ESP2_PLATFORM_DIYMORE_ES3C28P) || defined(ESP2_PLATFORM_WAVESHARE_RLCD42)
#include <SD_MMC.h>
/** Active FAT filesystem object (SD_MMC on diymore/RLCD, SD SPI on Waveshare AMOLED). */
#define ESP2_SD SD_MMC
#else
#include <SD.h>
#include <SPI.h>
#define ESP2_SD SD
SPIClass &esp2SdSpi();
#endif

/** End FAT mount and release the SD bus (SPI or SDMMC). */
void esp2SdBusEnd();

/**
 * Mount FAT.
 * Waveshare AMOLED: SPI3 + CS from board_pins.h (default 4 MHz).
 * diymore ES3C28P: SDMMC 4-bit (LCDWIKI pins); hz ignored by SD_MMC host.
 * waveshare-rlcd42: SDMMC 1-bit CLK=38 CMD=21 D0=39 (must not touch ST7305 CS/RST).
 */
bool esp2SdBusBegin(uint32_t hz = 4000000);
