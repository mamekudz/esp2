# diymore ESP32-S3 2.8" Touch (ES3C28P)

Status: **BRING-UP / IN DEVELOPMENT** — not an ESP][ production target yet.

| Field | Value |
| --- | --- |
| Platform ID | `diymore-es3c28p` |
| Part number | `AM03-FBBA0125-AAA` |
| Product code | `ES3C28P` (from listing) |
| Marketing name | diymore ESP32-S3 2.8" capacitive Touch / XiaoZhi AI |
| µGulp Project Control | Target → **diymore 2.8"** |

## Verified vs unknown

| Item | Status |
| --- | --- |
| MCU | ESP32-S3 (QFN56) rev v0.2 — confirmed on COM9 |
| MAC (this unit) | `b8:1f:3f:c4:c3:3c` |
| Flash size | **16 MB** (esptool + `[FLASH] 16777216`) |
| PSRAM | **8 MB embedded** (esptool Features) — enable `BOARD_HAS_PSRAM` for firmware probe |
| USB | Espressif USB-Serial/JTAG `VID:PID=303A:1001` (currently **COM9** on this host) |
| Display controller | **ILI9341V** — **VISUALLY_VERIFIED** (`diymore_display_probe` color bars) |
| Display bus / pins | SPI CS=10 DC=46 SCK=12 MOSI=11 MISO=13 BL=45 RST=CHIP_PU — **PASS** |
| Touch controller / pins | **FT6336G** I2C SDA=16 SCL=15 RST=18 INT=17 (vendor; not probed yet) |
| microSD | **SDIO 4-bit** CLK=38 CMD=40 D0=39 D1=41 D2=48 D3=47 — **VERIFIED** (`SDMMC_4bit`, 7456 MB card, `[SELFTEST] sd=PASS`) |
| Audio | **ES8311** I2C 0x18 (shared touch bus) + **FM8002E**; EN=1 MCLK=4 BCLK=5 DOUT=8 WS=7 DIN=6 — codec **PROBED**, PCM playback **TODO** |

Note: Waveshare on this host previously used **COM5** (MAC `10:20:ba:45:6a:44`). Ports change when boards are replugged — never hardcode COM numbers.

Product-page text is **not** sufficient to fill pin maps.

## PlatformIO

Environments: `diymore_bringup` · `diymore_display_probe` · `diymore_apple2`.

`diymore_apple2` shares portable Apple II core + `ESP2_SD` (SD_MMC on this board, SPI `SD` on Waveshare).

## Display probe

µGulp (Target **diymore 2.8"**): **Display Probe (ILI9341 color bars)**  
or `pio run -e diymore_display_probe -t upload --upload-port COMx`

Expect serial `[DISPLAY] ILI9341 begin OK` and on panel: RED/GREEN/BLUE/WHITE/YELLOW/CYAN bars + TOP/BOTTOM/L/R.

**2026-09-30:** User visual confirm — color bars and orientation marks **PASS**.

## microSD

LCDWIKI ES3C28P SDIO pins in `include/boards/diymore_es3c28p.h`. Mount via `esp2SdBusBegin()` → `SD_MMC`.

**2026-09-30:** Physical card on COM9 — `[SD] mounted bus=SDMMC_4bit type=3 size_MB=7456`, tree prepared, `[SELFTEST] sd=PASS`.

Galaxian media uploaded via `esp2-upload.mjs`; boot reaches HGR (`bootFromDisk ok`, no task-WDT after strip-yield present).

**2026-10-01:** „PREPARE YOURSELF“ stuck: menu **A** is joystick Galaxian (needs PB0, not SPACE). Pressing **A** after early HGR caused TEXT+motor hang. Startup macro is now `waitHires` → repeated `pb0` (no menu key). Serial: HGR stays after `MACRO DONE`.

## Next

1. Touch probe (FT6336G) — later shared special program (also Waveshare).
2. User visual confirm: clearer text + Galaxian past boot menu.

## First physical test sequence

1. USB-C to the diymore board; note the COM port (`pio device list` — look for `303A:1001`).
2. µGulp Project Control → Target **diymore 2.8"** (or `ESP2_PORT=COMx`).
3. Run **Identify Chip / Flash / PSRAM** (or `pio run -e diymore_bringup -t upload --upload-port COMx`).
4. Open Serial Monitor at 115200; confirm `[BOOT]`, `[CHIP]`, `[FLASH]`, `[PSRAM]`.
5. Next: obtain schematic / vendor Arduino example for **exact** LCD + touch ICs and pins.

## What we need next (for display/touch)

- Confirmed display IC part number
- Confirmed touch IC part number
- Pin table (CLK/MOSI/MISO/CS/DC/RST/BL, SDA/SCL/INT, …)
- Working vendor PlatformIO or Arduino sketch for this exact module
