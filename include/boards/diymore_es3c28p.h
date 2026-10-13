#pragma once

/**
 * diymore ESP32-S3 2.8" Touch (ES3C28P / AM03-FBBA0125-AAA)
 * Platform ID: diymore-es3c28p
 *
 * Pin map source: LCDWIKI ES3C28P user manual / Arduino demo instructions
 *   https://www.lcdwiki.com/res/ES3C28P/
 * LCD status: VISUALLY_VERIFIED (diymore_display_probe color bars + orientation marks)
 *
 * Measured on one unit (esptool): Flash 16 MB, Embedded PSRAM 8 MB.
 */

#define ESP2_BOARD_ID_DIYMORE_ES3C28P 1
#define ESP2_BOARD_PART_NUMBER "AM03-FBBA0125-AAA"
#define ESP2_BOARD_PRODUCT_CODE "ES3C28P"

/*-- @<BUILD_ONLY_AT_PLATFORMS:diymore-es3c28p --*/
#define ESP2_PLATFORM_TAG_DIYMORE 1
/*-- @>BUILD_ONLY_AT_PLATFORMS --*/

/* LCD — ILI9341V, 240×320 IPS, 4-wire SPI (LCDWIKI) */
#define DIYMORE_LCD_CONTROLLER_ILI9341V 1
#define DIYMORE_LCD_WIDTH 240
#define DIYMORE_LCD_HEIGHT 320
#define PIN_DIYMORE_TFT_CS 10
#define PIN_DIYMORE_TFT_DC 46 /* TFT_RS in vendor docs */
#define PIN_DIYMORE_TFT_SCK 12
#define PIN_DIYMORE_TFT_MOSI 11
#define PIN_DIYMORE_TFT_MISO 13
/* RST shared with ESP32 CHIP_PU — not a dedicated GPIO */
#define PIN_DIYMORE_TFT_BL 45

/* Capacitive touch — FT6336G I2C (LCDWIKI); probe later */
#define DIYMORE_TOUCH_CONTROLLER_FT6336G 1
#define PIN_DIYMORE_TP_SDA 16
#define PIN_DIYMORE_TP_SCL 15
#define PIN_DIYMORE_TP_RST 18
#define PIN_DIYMORE_TP_INT 17
#define DIYMORE_FT6336G_I2C_ADDR 0x38

/* microSD — SDIO 4-bit (LCDWIKI ES3C28P); VERIFIED mount SDMMC_4bit on unit */
#define PIN_DIYMORE_SD_CLK 38
#define PIN_DIYMORE_SD_CMD 40
#define PIN_DIYMORE_SD_D0 39
#define PIN_DIYMORE_SD_D1 41
#define PIN_DIYMORE_SD_D2 48
#define PIN_DIYMORE_SD_D3 47

/* Audio — ES8311 codec + FM8002E amp (LCDWIKI); I2C shared with touch (0x18) */
#define DIYMORE_AUDIO_CODEC_ES8311 1
#define DIYMORE_ES8311_I2C_ADDR 0x18
#define PIN_DIYMORE_AUDIO_EN 1 /* LOW = amp/codec path enabled */
#define PIN_DIYMORE_I2S_MCLK 4
#define PIN_DIYMORE_I2S_BCLK 5
#define PIN_DIYMORE_I2S_DOUT 8 /* ESP → codec */
#define PIN_DIYMORE_I2S_WS 7
#define PIN_DIYMORE_I2S_DIN 6 /* codec → ESP (mic) */

/*
 * TODO(diymore): ES8311 register init + I2S speaker PCM path
 */

