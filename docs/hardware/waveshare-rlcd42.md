# Waveshare / Luckfox ESP32-S3-RLCD-4.2

Status: **IN DEVELOPMENT** — additional ESP][ mono target (not enclosure production).
`rlcd42_apple2` boots Galaxian on physical hardware (PHYSICALLY_VERIFIED path).

| Field | Value |
| --- | --- |
| Platform ID | `waveshare-rlcd42` |
| Product | ESP32-S3-RLCD-4.2 (Luckfox listing = Waveshare family) |
| µGulp Project Control | Target → **RLCD 4.2** |
| Display | ST7305 reflective LCD 300×400 (logical 400×300 landscape) |
| Touch | **None** — Control Center via KEY / BOOT |
| Buttons | BOOT GPIO0, KEY GPIO18 (active low); PWR = hardware |

## Verified vs unknown

| Item | Status |
| --- | --- |
| MCU | ESP32-S3 (QFN56) rev v0.2 — **confirmed** COM13 |
| MAC (this unit) | `94:a9:90:cd:51:50` |
| Flash size | **16 MB** (esptool auto-detect) |
| PSRAM | **8 MB embedded** (esptool Features AP_3v3) |
| USB | Espressif USB-Serial/JTAG `VID:PID=303A:1001` |
| Display SPI | SCK=11 MOSI=12 CS=40 DC=5 RST=41 — vendor/ESPHome |
| Touch | N/A |
| Buttons | BOOT GPIO0 / KEY GPIO18 — firmware polls `[BTN]` / `[HB]` |
| microSD | SDMMC 1-bit: CLK=38 CMD=21 D0=39 (never use AMOLED SPI 40/41) |
| Audio / mics | Out of ESP][ scope for this platform |

## PlatformIO

Environments: `rlcd42_bringup` · `rlcd42_display_probe` · `rlcd42_apple2`.

| Env | Status |
| --- | --- |
| `rlcd42_bringup` | Flashed COM13 — esptool 16 MB / 8 MB PSRAM; `[HB]` live |
| `rlcd42_display_probe` | Flashed — `[DISPLAY] ST7305 begin OK` (user visual confirm pending) |
| `rlcd42_apple2` | Mono Apple II — Galaxian boot PHYSICALLY_VERIFIED; KEY/BOOT Control Center; default classic 1× + optional full fit |

## First physical test (no SD required)

1. USB-C to the board; note COM (`pio device list` — Espressif `303A:1001`).
2. `ESP2_PLATFORM=waveshare-rlcd42` or Project Control Target **RLCD 4.2**.
3. `pio run -e rlcd42_bringup -t upload --upload-port COMx`
4. Serial 115200: `[BOOT]`, `[FLASH]`, `[PSRAM]`, `[BTN]` on KEY/BOOT.
5. Next: `rlcd42_display_probe` — user must visually confirm TOP/BOTTOM/L/R.

## Notes

- Reflective panel needs ambient light; no backlight.
- Apple II color modes map to greyscale / dither on this panel.
- Enclosure viewport / AMOLED geometry do not apply.
