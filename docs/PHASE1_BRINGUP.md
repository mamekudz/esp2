# Phase 1 bring-up evidence (ESP][)

## Result

Physical board is running diagnostic firmware on **COM5**.

| Check | Result |
| --- | --- |
| Build | SUCCESS |
| Upload | SUCCESS (esptool, COM5) |
| Chip | ESP32-S3 (QFN56) rev v0.2, MAC 10:20:ba:45:6a:44 |
| Flash | **16777216 bytes OK** (auto-detected 16MB) |
| PSRAM | **8388608 bytes OK** |
| Display | **CO5300 init OK**, 280×456, portrait native, diagnostic pattern drawn |
| Touch | **FAIL** — FT3168 not ACKing at 0x38; I2C scan sees `0x6B` (QMI8658) only (plus occasional ghost `0x7E`) |
| SD | no card / mount failed (non-fatal) |
| SELFTEST | SOFTWARE FAIL (touch required) |
| VISUAL | WAITING_FOR_USER_CONFIRMATION |

## PlatformIO environment

- Env: `bringup`
- Platform: pioarduino `55.03.32` (`platform-espressif32.zip`)
- Board definition: `esp32-s3-devkitc-1` (+ 16MB flash / QIO OPI PSRAM flags)
- Framework: Arduino-ESP32 **3.3.2** + libs **5.5.0**
- Toolchain: `toolchain-xtensa-esp-elf @ 14.2.0+20250730`
- esptool: `5.1.0`
- Upload/monitor port: **COM5** (USB VID:PID `303A:1001`)
- Important flags: `ARDUINO_USB_CDC_ON_BOOT=1`, `BOARD_HAS_PSRAM`, `board_build.arduino.memory_type=qio_opi`, `board_upload.flash_size=16MB`, `default_16MB.csv`

### Why pioarduino (and why pin 55.03.32)

Arduino_GFX CO5300/QSPI needs Arduino-ESP32 3.x. Stock PlatformIO `espressif32@6.12.0` still ships Arduino 2.0.x. pioarduino `stable` (55.03.312) entered an infinite tool reinstall loop on this Windows host; **55.03.32** builds cleanly. Local patches were applied under `%USERPROFILE%\.platformio\platforms\espressif32\` to skip the HTTPS↔file:// tool reinstall loop and to avoid forcing a PlatformIO Core zip downgrade into `penv`.

## Drivers / libraries

- Display: `moononournation/GFX Library for Arduino @ 1.6.8` → `Arduino_ESP32QSPI` + `Arduino_CO5300`
- Touch: minimal Wire FT3168 client (official Waveshare address/pins)
- SD: Arduino `SD` + `SPI` on GPIO 38–41

## Pins (V1 defaults)

- QSPI: CS=9, SCK=10, D0–D3=11–14, RST=21
- Touch I2C: SDA=47, SCL=48, addr=0x38; optional TP_RST retry on GPIO8
- SD SPI: CS=38, MOSI=39, MISO=40, SCLK=41
- CO5300 offsets: col1=20, row1=0, col2=180, row2=24 (jaapp proven values)

## Deviations from Waveshare official Arduino demo

- Uses Arduino_GFX `Arduino_CO5300` instead of Waveshare’s SH8601 wrapper + custom init table (display still initializes and draws).
- PlatformIO / pioarduino instead of Arduino IDE.
- Structured `[TAG]` serial diagnostics and a fixed diagnostic test pattern (no LVGL).

## Files created

- `platformio.ini`
- `include/board_pins.h`
- `src/main.cpp`
- `docs/THIRD_PARTY.md`
- `docs/PHASE1_BRINGUP.md`
- `.gitignore`

## Remaining warnings / open items

- Touch FT3168 absent on I2C despite matching official Waveshare pin/address config; needs visual/hardware follow-up (PCB V1 vs V2 silkscreen, factory firmware touch check).
- No microSD inserted during bring-up.
- Board JSON label still says “8MB Flash” for `esp32-s3-devkitc-1`; flash size override is via `board_upload.flash_size=16MB` (esptool confirmed 16MB).
