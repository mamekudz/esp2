#include "esp_bracket/apple2_bus.hpp"

#include <cstring>

namespace esp_bracket {

Apple2Bus::Apple2Bus() {
    reset();
}

void Apple2Bus::reset() {
    std::memset(ram_, 0x00, sizeof(ram_));
    softSwitches_.reset();
    keyboard_.reset();
    speaker_.reset();
    // ROM retained across Apple II reset (user-loaded image stays).
    accessCycle_ = 0;
}

uint8_t Apple2Bus::read(uint16_t address) {
    if (address < 0xC000u) {
        return ram_[address];
    }
    if (address <= 0xC0FFu) {
        return handleIoRead(address);
    }
    if (address >= Rom::kMapBase) {
        return rom_.readAbsolute(address);
    }
    // $C100–$CFFF: open bus / empty slots — return floating 0xFF
    return 0xFF;
}

void Apple2Bus::write(uint16_t address, uint8_t value) {
    if (address < 0xC000u) {
        ram_[address] = value;
        return;
    }
    if (address <= 0xC0FFu) {
        handleIoWrite(address, value);
        return;
    }
    // ROM / slots: write ignored (ROM write-protect)
}

uint8_t Apple2Bus::handleIoRead(uint16_t address) {
    softSwitches_.access(address);

    // II+ mirrors: kbd data $C000–$C00F, strobe $C010–$C01F, speaker $C030–$C03F
    if (address >= 0xC000u && address <= 0xC00Fu) {
        return keyboard_.readData();
    }
    if (address >= 0xC010u && address <= 0xC01Fu) {
        keyboard_.clearStrobe();
        return keyboard_.readData();
    }
    if (address >= 0xC030u && address <= 0xC03Fu) {
        speaker_.toggle(accessCycle_);
        return 0x00;
    }
    return 0x00;
}

void Apple2Bus::handleIoWrite(uint16_t address, uint8_t value) {
    (void)value;
    softSwitches_.access(address);

    if (address >= 0xC010u && address <= 0xC01Fu) {
        keyboard_.clearStrobe();
        return;
    }
    if (address >= 0xC030u && address <= 0xC03Fu) {
        speaker_.toggle(accessCycle_);
    }
}

uint8_t Apple2Bus::busRead(void *ctx, uint16_t address) {
    return static_cast<Apple2Bus *>(ctx)->read(address);
}

void Apple2Bus::busWrite(void *ctx, uint16_t address, uint8_t value) {
    static_cast<Apple2Bus *>(ctx)->write(address, value);
}

} // namespace esp_bracket
