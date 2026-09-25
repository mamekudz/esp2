# Third-party sources (Phase 1)

## Hardware documentation

- Waveshare product docs: https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.64
- Waveshare Arduino guide: https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.64/Arduino
- Official Arduino board variant pins:
  https://github.com/espressif/arduino-esp32/blob/master/variants/waveshare_esp32_s3_touch_amoled_164/pins_arduino.h

## Proven PlatformIO ports used as reference

| Project | URL | Relevance | License notes |
| --- | --- | --- | --- |
| jaapp/waveshare-amoled164-lvgl9-platformio | https://github.com/jaapp/waveshare-amoled164-lvgl9-platformio | Exact board; Arduino_GFX CO5300 + FT3168 pin/init pattern | No LICENSE file in repo at clone time; used as reference only (not copied wholesale) |
| trane77/Waveshare-ESP32-S3-Touch-AMOLED-1.64-PlatformIO | https://github.com/trane77/Waveshare-ESP32-S3-Touch-AMOLED-1.64-PlatformIO | Exact board; pioarduino + CO5300 offsets / board JSON | No LICENSE file in repo at clone time; used as reference only |
| moononournation/Arduino_GFX | https://github.com/moononournation/Arduino_GFX | CO5300 QSPI display driver | MIT (upstream) |
| pioarduino/platform-espressif32 | https://github.com/pioarduino/platform-espressif32 | Preferred Arduino-ESP32 3.x PlatformIO package (deferred on this host due to tool reinstall loop) | Apache-2.0 (Espressif platform lineage) |
| platformio/platform-espressif32 | https://github.com/platformio/platform-espressif32 | Stock PlatformIO platform used for Phase 1 baseline | Apache-2.0 |
| espressif/arduino-esp32 | https://github.com/espressif/arduino-esp32 | Arduino-ESP32 3.3.x framework override for QSPI/CO5300 | LGPL-2.1 |

## Why not invent a new CO5300 driver

Working Arduino_GFX `Arduino_CO5300` + `Arduino_ESP32QSPI` initialization for this panel is already proven. Phase 1 prefers that known-good path over a from-scratch driver.
