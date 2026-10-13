#pragma once

/**
 * Waveshare / Luckfox ESP32-S3-RLCD-4.2
 * Platform ID: waveshare-rlcd42
 *
 * 4.2" reflective ST7305 (300×400 native, 400×300 landscape via U8g2 R1).
 * No touch. Buttons: BOOT (GPIO0), KEY (GPIO18). PWR is hardware power.
 *
 * Pin map: Waveshare ESP32-S3-RLCD-4.2 docs / ESPHome device notes.
 */

#define ESP2_BOARD_ID_WAVESHARE_RLCD42 1
#define ESP2_BOARD_PRODUCT_CODE "ESP32-S3-RLCD-4.2"

/*-- @<BUILD_ONLY_AT_PLATFORMS:waveshare-rlcd42 --*/
#define ESP2_PLATFORM_TAG_RLCD42 1
/*-- @>BUILD_ONLY_AT_PLATFORMS --*/

/* ST7305 RLCD — logical landscape after U8G2_R1 */
#define RLCD42_LCD_CONTROLLER_ST7305 1
#define RLCD42_LCD_NATIVE_W 300
#define RLCD42_LCD_NATIVE_H 400
#define RLCD42_LCD_WIDTH 400
#define RLCD42_LCD_HEIGHT 300

#define PIN_RLCD42_SCK 11
#define PIN_RLCD42_MOSI 12
#define PIN_RLCD42_CS 40
#define PIN_RLCD42_DC 5
#define PIN_RLCD42_RST 41

/* User buttons (active low) */
#define PIN_RLCD42_BOOT 0
#define PIN_RLCD42_KEY 18

/* I2C bus (SHTC3 / RTC / audio codecs — unused in Milestone 1–2) */
#define PIN_RLCD42_I2C_SDA 13
#define PIN_RLCD42_I2C_SCL 14

/* microSD — SDMMC 1-bit (schematic); R7 CS→GPIO17 unpopulated */
#define PIN_RLCD42_SD_CLK 38
#define PIN_RLCD42_SD_CMD 21
#define PIN_RLCD42_SD_D0 39
/* Do NOT use AMOLED SPI SD pins 40/41 — those are ST7305 CS/RST on this board. */
