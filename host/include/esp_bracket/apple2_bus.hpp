#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_bracket/keyboard.hpp"
#include "esp_bracket/rom.hpp"
#include "esp_bracket/soft_switches.hpp"
#include "esp_bracket/speaker.hpp"

namespace esp_bracket {

/**
 * Apple II+ style 64K bus decode.
 *
 *  RAM  $0000–$BFFF  (48K)
 *  I/O  $C000–$C0FF
 *  ROM  $D000–$FFFF  (12K)
 *
 * Language card / $C100–$CFFF slot ROM not required yet.
 * CPU must not know Apple addresses — bus decodes.
 */
class Apple2Bus {
  public:
    static constexpr size_t kRamBytes = 0xC000; // 48 KiB

    Apple2Bus();

    void reset();

    uint8_t read(uint16_t address);
    void write(uint16_t address, uint8_t value);

    SoftSwitches &softSwitches() { return softSwitches_; }
    const SoftSwitches &softSwitches() const { return softSwitches_; }

    Keyboard &keyboard() { return keyboard_; }
    const Keyboard &keyboard() const { return keyboard_; }

    Speaker &speaker() { return speaker_; }
    const Speaker &speaker() const { return speaker_; }

    Rom &rom() { return rom_; }
    const Rom &rom() const { return rom_; }

    uint8_t *ram() { return ram_; }
    const uint8_t *ram() const { return ram_; }

    /** Cycle stamp used for speaker events (set by machine each access). */
    void setAccessCycle(uint32_t cycle) { accessCycle_ = cycle; }
    uint32_t accessCycle() const { return accessCycle_; }

    static uint8_t busRead(void *ctx, uint16_t address);
    static void busWrite(void *ctx, uint16_t address, uint8_t value);

  private:
    uint8_t handleIoRead(uint16_t address);
    void handleIoWrite(uint16_t address, uint8_t value);

    uint8_t ram_[kRamBytes]{};
    SoftSwitches softSwitches_;
    Keyboard keyboard_;
    Speaker speaker_;
    Rom rom_;
    uint32_t accessCycle_ = 0;
};

} // namespace esp_bracket
