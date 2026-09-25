#pragma once

// Waveshare ESP32-S3-Touch-AMOLED-1.64 pin map (PCB V1 / Arduino variant defaults)
// Sources:
//   https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.64
//   https://github.com/espressif/arduino-esp32/blob/master/variants/waveshare_esp32_s3_touch_amoled_164/pins_arduino.h
//   https://github.com/jaapp/waveshare-amoled164-lvgl9-platformio
//
// NOTE: PCB V2 swaps LCD_CS (GPIO46) and IMU_INT1 (GPIO9).
// Phase 1 uses V1 defaults (LCD_CS=GPIO9). If the display fails to init on a
// V2 board, rebuild with -DBOARD_PCB_V2.

#if defined(BOARD_PCB_V2)
#define PIN_LCD_CS 46
#else
#define PIN_LCD_CS 9
#endif

#define PIN_LCD_SCK 10
#define PIN_LCD_D0 11
#define PIN_LCD_D1 12
#define PIN_LCD_D2 13
#define PIN_LCD_D3 14
#define PIN_LCD_RST 21

#define LCD_WIDTH 280
#define LCD_HEIGHT 456

// CO5300 GRAM alignment offsets used by working Arduino_GFX ports for this panel
#define LCD_COL_OFFSET1 20
#define LCD_ROW_OFFSET1 0
#define LCD_COL_OFFSET2 180
#define LCD_ROW_OFFSET2 24

#define PIN_TOUCH_SDA 47
#define PIN_TOUCH_SCL 48
#define FT3168_I2C_ADDR 0x38

// Waveshare Rev1.1 schematic evidence:
//   TP_RESET --R35 0R--> LCD_RESET (GPIO21); R34 to IO8 is NC
//   TP_INT   --R38 0R--> GPIO18 (with R39 4.7k)
// V2 docs add dedicated TP_INT/TP_RESET/LCD_TE; V1/Rev1.1 ties TP_RESET to LCD_RESET.
#define PIN_TP_INT 18
#define PIN_TP_RST_SHARED_WITH_LCD PIN_LCD_RST

#define PIN_SD_CS 38
#define PIN_SD_MOSI 39
#define PIN_SD_MISO 40
#define PIN_SD_SCLK 41
// microSD: SPI (not SDMMC). Waveshare Arduino demo 03_SD_Card + jaapp README.
// Bus is separate from QSPI display (GPIO9–14) and touch I2C (GPIO47/48).

// QMI8658 shares FT3168 I2C bus (SDA=47 SCL=48). Address 0x6A or 0x6B.
// WHO_AM_I register 0 == 0x05. V1: IMU_INT1 typically GPIO46 (V2 swaps with LCD_CS).
#define QMI8658_I2C_ADDR_L 0x6A
#define QMI8658_I2C_ADDR_H 0x6B
#define QMI8658_WHO_AM_I_VALUE 0x05
#if defined(BOARD_PCB_V2)
#define PIN_IMU_INT1 9
#else
#define PIN_IMU_INT1 46
#endif
