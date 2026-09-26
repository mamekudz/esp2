/**
 * ESP32-S3 compile-smoke entry (COMPILE ONLY — do not flash for Level-5 prep).
 * Instantiates portable emulator-core types to force link of host apple2 sources.
 */
#include <Arduino.h>

#include "esp_bracket/apple2_bus.hpp"
#include "esp_bracket/cpu6502.hpp"
#include "esp_bracket/disk_ii_controller.hpp"
#include "esp_bracket/disk_ii_encoding.hpp"
#include "esp_bracket/gamepad_mapper.hpp"
#include "esp_bracket/hgr_decoder.hpp"
#include "esp_bracket/keyboard.hpp"
#include "esp_bracket/soft_switches.hpp"
#include "esp_bracket/speaker.hpp"
#include "esp_bracket/text_decoder.hpp"

#include <cstdio>

#if !defined(ESP_BRACKET_CORE_SMOKE)
#error "ESP_BRACKET_CORE_SMOKE must be defined for this translation unit"
#endif

static_assert(esp_bracket::DiskIIController::kSlotRomSize == 256, "slot rom size");
static_assert(esp_bracket::HgrDecoder::kWidth == 280, "hgr width");
static_assert(esp_bracket::HgrDecoder::kHeight == 192, "hgr height");

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("[SMOKE] ESP][ emulator-core compile smoke");

    // Large objects: static storage — FreeRTOS task stack must not hold Apple2Bus RAM.
    static esp_bracket::Cpu6502 cpu;
    static esp_bracket::Apple2Bus bus;
    static esp_bracket::DiskIIController disk;
    esp_bracket::Keyboard kbd;
    esp_bracket::Speaker spk;
    esp_bracket::SoftSwitches sw;
    esp_bracket::GamepadMapper map;

    Serial.printf("[SMOKE] sizeof(Cpu6502)=%u\n", (unsigned)sizeof(cpu));
    Serial.printf("[SMOKE] sizeof(Apple2Bus)=%u\n", (unsigned)sizeof(bus));
    Serial.printf("[SMOKE] sizeof(DiskIIController)=%u\n", (unsigned)sizeof(disk));
    Serial.printf("[SMOKE] sizeof(GamepadMapper)=%u\n", (unsigned)sizeof(map));
    Serial.printf("[SMOKE] kbd=%u spk=%u sw=%u\n", (unsigned)sizeof(kbd), (unsigned)sizeof(spk),
                  (unsigned)sizeof(sw));
    Serial.println("[SMOKE] PASS compile-link smoke (no runtime emulation asserted)");
}

void loop() {
    delay(10000);
}
