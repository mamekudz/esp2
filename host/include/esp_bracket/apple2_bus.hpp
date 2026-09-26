#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_bracket/game_io.hpp"
#include "esp_bracket/io_trace.hpp"
#include "esp_bracket/keyboard.hpp"
#include "esp_bracket/rom.hpp"
#include "esp_bracket/slot_device.hpp"
#include "esp_bracket/soft_switches.hpp"
#include "esp_bracket/speaker.hpp"

namespace esp_bracket {

/**
 * Apple II+ style 64K bus decode.
 *
 *  RAM  $0000–$BFFF  (48K)
 *  I/O  $C000–$C0FF
 *  Slot ROM $C100–$C7FF
 *  Exp ROM  $C800–$CFFF (selection stub)
 *  ROM  $D000–$FFFF  (12K)
 *
 * CPU must not know Apple addresses — bus decodes.
 * See docs/apple2/io-page.md.
 */
class Apple2Bus {
public:
    static constexpr size_t kRamBytes = 0xC000; // 48 KiB
    static constexpr int kSlotCount = 8;

    Apple2Bus();

    void reset();

    /** Hardware read/write — full side effects. */
    uint8_t read(uint16_t address);
    void write(uint16_t address, uint8_t value);

    /**
     * Diagnostic peek — no soft-switch / speaker / strobe / paddle / cassette
     * side effects. Safe for memory viewers.
     */
    uint8_t peek(uint16_t address) const;

    SoftSwitches& softSwitches() { return softSwitches_; }
    const SoftSwitches& softSwitches() const { return softSwitches_; }

    Keyboard& keyboard() { return keyboard_; }
    const Keyboard& keyboard() const { return keyboard_; }

    Speaker& speaker() { return speaker_; }
    const Speaker& speaker() const { return speaker_; }

    GameIo& gameIo() { return gameIo_; }
    const GameIo& gameIo() const { return gameIo_; }

    Rom& rom() { return rom_; }
    const Rom& rom() const { return rom_; }

    uint8_t* ram() { return ram_; }
    const uint8_t* ram() const { return ram_; }

    void setAccessCycle(uint32_t cycle) { accessCycle_ = cycle; }
    uint32_t accessCycle() const { return accessCycle_; }

    void setSlotDevice(int slot, SlotDevice* device);
    SlotDevice* slotDevice(int slot) const;

    void setFloatingBusApprox(uint8_t v) { floatingBusApprox_ = v; }
    uint8_t floatingBusApprox() const { return floatingBusApprox_; }

    void setIoTrace(IoTraceFn fn, void* ctx);
    void clearIoTrace();

    /** Expansion ROM C800 latch: last slot that touched its Cx00 ROM. */
    int expansionRomSlot() const { return expansionRomSlot_; }

    static uint8_t busRead(void* ctx, uint16_t address);
    static void busWrite(void* ctx, uint16_t address, uint8_t value);

private:
    uint8_t handleIoRead(uint16_t address, bool sideEffects);
    void handleIoWrite(uint16_t address, uint8_t value, bool sideEffects);
    uint8_t handleSlotRomRead(uint16_t address, bool sideEffects) const;
    void trace(bool isWrite, uint16_t address, uint8_t value, const char* tag);

    uint8_t ram_[kRamBytes]{};
    SoftSwitches softSwitches_;
    Keyboard keyboard_;
    Speaker speaker_;
    GameIo gameIo_;
    Rom rom_;
    SlotDevice* slots_[kSlotCount]{};
    uint32_t accessCycle_ = 0;
    uint8_t floatingBusApprox_ = 0xFF;
    IoTraceFn traceFn_ = nullptr;
    void* traceCtx_ = nullptr;
    int expansionRomSlot_ = -1;
};

} // namespace esp_bracket
