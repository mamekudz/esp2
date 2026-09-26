#include "esp_bracket/apple2_bus.hpp"

#include <cstring>

namespace esp_bracket {

Apple2Bus::Apple2Bus() {
    reset();
}

void Apple2Bus::reset() {
    // Soft switches / peripherals only — Apple II RESET does not wipe RAM.
    softSwitches_.reset();
    keyboard_.reset();
    speaker_.reset();
    gameIo_.reset();
    // ROM + slot device pointers retained across Apple II reset.
    accessCycle_ = 0;
    expansionRomSlot_ = -1;
}

void Apple2Bus::clearRam() {
    std::memset(ram_, 0x00, sizeof(ram_));
}

void Apple2Bus::setSlotDevice(int slot, SlotDevice *device) {
    if (slot >= 0 && slot < kSlotCount) {
        slots_[slot] = device;
    }
}

SlotDevice *Apple2Bus::slotDevice(int slot) const {
    if (slot < 0 || slot >= kSlotCount) {
        return nullptr;
    }
    return slots_[slot];
}

void Apple2Bus::setIoTrace(IoTraceFn fn, void *ctx) {
    traceFn_ = fn;
    traceCtx_ = ctx;
}

void Apple2Bus::clearIoTrace() {
    traceFn_ = nullptr;
    traceCtx_ = nullptr;
}

void Apple2Bus::trace(bool isWrite, uint16_t address, uint8_t value, const char *tag) {
    if (!traceFn_) {
        return;
    }
    IoTraceEvent ev{};
    ev.cycle = accessCycle_;
    ev.isWrite = isWrite;
    ev.address = address;
    ev.value = value;
    ev.tag = tag;
    traceFn_(traceCtx_, ev);
}

uint8_t Apple2Bus::read(uint16_t address) {
    if (address < 0xC000u) {
        return ram_[address];
    }
    if (address <= 0xC0FFu) {
        return handleIoRead(address, true);
    }
    if (address >= 0xC100u && address <= 0xC7FFu) {
        return handleSlotRomRead(address, true);
    }
    if (address >= 0xC800u && address <= 0xCFFFu) {
        // Expansion ROM window — empty unless future device maps it.
        return floatingBusApprox_;
    }
    if (address >= Rom::kMapBase) {
        return rom_.readAbsolute(address);
    }
    return floatingBusApprox_;
}

void Apple2Bus::write(uint16_t address, uint8_t value) {
    if (address < 0xC000u) {
        ram_[address] = value;
        return;
    }
    if (address <= 0xC0FFu) {
        handleIoWrite(address, value, true);
        return;
    }
    // Slot ROM / expansion / motherboard ROM: write ignored
}

uint8_t Apple2Bus::peek(uint16_t address) const {
    if (address < 0xC000u) {
        return ram_[address];
    }
    if (address <= 0xC0FFu) {
        // Const path: duplicate decode without mutation via const_cast helper
        return const_cast<Apple2Bus *>(this)->handleIoRead(address, false);
    }
    if (address >= 0xC100u && address <= 0xC7FFu) {
        return handleSlotRomRead(address, false);
    }
    if (address >= 0xC800u && address <= 0xCFFFu) {
        return floatingBusApprox_;
    }
    if (address >= Rom::kMapBase) {
        return rom_.readAbsolute(address);
    }
    return floatingBusApprox_;
}

uint8_t Apple2Bus::handleSlotRomRead(uint16_t address, bool sideEffects) const {
    // $Cn00–$CnFF → slot n (n = 1..7). Example: $C600 → slot 6.
    const int slot = static_cast<int>((address >> 8) & 0x0F);
    if (slot < 1 || slot > 7) {
        return floatingBusApprox_;
    }
    const uint16_t offset = static_cast<uint16_t>(address & 0xFFu);
    SlotDevice *dev = slots_[slot];
    if (!dev) {
        return floatingBusApprox_;
    }
    // Real bus access selects $C800 expansion ROM for that slot; peek must not.
    if (sideEffects) {
        const_cast<Apple2Bus *>(this)->expansionRomSlot_ = slot;
    }
    return dev->romRead(offset);
}

uint8_t Apple2Bus::handleIoRead(uint16_t address, bool sideEffects) {
    const uint8_t pageOff = static_cast<uint8_t>(address & 0xFFu);

    // $C000–$C00F keyboard data
    if (pageOff <= 0x0F) {
        const uint8_t v = keyboard_.readData();
        if (sideEffects) {
            trace(false, address, v, "keyboard");
        }
        return v;
    }

    // $C010–$C01F strobe clear
    if (pageOff <= 0x1F) {
        if (sideEffects) {
            keyboard_.clearStrobe();
        }
        const uint8_t v = keyboard_.readData();
        if (sideEffects) {
            trace(false, address, v, "strobe-clear");
        }
        return v;
    }

    // $C020–$C02F cassette out
    if (pageOff <= 0x2F) {
        if (sideEffects) {
            gameIo_.cassetteOutToggle();
            trace(false, address, 0, "cassette-out");
        }
        return 0x00;
    }

    // $C030–$C03F speaker
    if (pageOff <= 0x3F) {
        if (sideEffects) {
            speaker_.toggle(accessCycle_);
            trace(false, address, 0, "speaker-toggle");
        }
        return 0x00;
    }

    // $C040–$C04F utility strobe
    if (pageOff <= 0x4F) {
        if (sideEffects) {
            gameIo_.utilityStrobe();
            trace(false, address, 0, "utility-strobe");
        }
        return 0x00;
    }

    // $C050–$C057 video soft switches
    if (pageOff >= 0x50 && pageOff <= 0x57) {
        if (sideEffects) {
            softSwitches_.access(address);
            trace(false, address, 0, "video-sw");
        }
        return 0x00;
    }

    // $C058–$C05F annunciators
    if (pageOff >= 0x58 && pageOff <= 0x5F) {
        if (sideEffects) {
            gameIo_.accessAnnunciator(address);
            trace(false, address, 0, "annunciator");
        }
        return 0x00;
    }

    // $C060–$C06F game inputs / paddles / cassette in
    if (pageOff >= 0x60 && pageOff <= 0x6F) {
        const uint8_t v = gameIo_.readGameInput(address, accessCycle_);
        if (sideEffects) {
            trace(false, address, v, "game-in");
        }
        return v;
    }

    // $C070–$C07F paddle trigger
    if (pageOff >= 0x70 && pageOff <= 0x7F) {
        if (sideEffects) {
            gameIo_.triggerPaddles(accessCycle_);
            trace(false, address, 0, "paddle-trigger");
        }
        return 0x00;
    }

    // $C080–$C0FF slot soft switches: slot = (offset >> 4) & 7
    if (pageOff >= 0x80) {
        const int slot = (pageOff >> 4) & 0x07;
        const uint8_t off = static_cast<uint8_t>(pageOff & 0x0Fu);
        SlotDevice *dev = slots_[slot];
        if (dev) {
            const uint8_t v = sideEffects ? dev->ioRead(off, accessCycle_) : dev->ioPeek(off);
            if (sideEffects) {
                trace(false, address, v, "slot-io");
            }
            return v;
        }
        if (sideEffects) {
            trace(false, address, floatingBusApprox_, "slot-empty");
        }
        return floatingBusApprox_;
    }

    return floatingBusApprox_;
}

void Apple2Bus::handleIoWrite(uint16_t address, uint8_t value, bool sideEffects) {
    if (!sideEffects) {
        return;
    }
    const uint8_t pageOff = static_cast<uint8_t>(address & 0xFFu);

    // $C000–$C00F: keyboard data — write has no II+ side effect
    if (pageOff <= 0x0F) {
        trace(true, address, value, "keyboard-write-ignored");
        return;
    }

    if (pageOff <= 0x1F) {
        keyboard_.clearStrobe();
        trace(true, address, value, "strobe-clear");
        return;
    }

    if (pageOff <= 0x2F) {
        gameIo_.cassetteOutToggle();
        trace(true, address, value, "cassette-out");
        return;
    }

    if (pageOff <= 0x3F) {
        speaker_.toggle(accessCycle_);
        trace(true, address, value, "speaker-toggle");
        return;
    }

    if (pageOff <= 0x4F) {
        gameIo_.utilityStrobe();
        trace(true, address, value, "utility-strobe");
        return;
    }

    if (pageOff >= 0x50 && pageOff <= 0x57) {
        softSwitches_.access(address);
        trace(true, address, value, "video-sw");
        return;
    }

    if (pageOff >= 0x58 && pageOff <= 0x5F) {
        gameIo_.accessAnnunciator(address);
        trace(true, address, value, "annunciator");
        return;
    }

    // $C060–$C06F reads are sensing — writes typically no-op on II+
    if (pageOff >= 0x60 && pageOff <= 0x6F) {
        trace(true, address, value, "game-in-write-ignored");
        return;
    }

    if (pageOff >= 0x70 && pageOff <= 0x7F) {
        gameIo_.triggerPaddles(accessCycle_);
        trace(true, address, value, "paddle-trigger");
        return;
    }

    if (pageOff >= 0x80) {
        const int slot = (pageOff >> 4) & 0x07;
        const uint8_t off = static_cast<uint8_t>(pageOff & 0x0Fu);
        SlotDevice *dev = slots_[slot];
        if (dev) {
            dev->ioWrite(off, value, accessCycle_);
            trace(true, address, value, "slot-io");
        } else {
            trace(true, address, value, "slot-empty");
        }
    }
}

uint8_t Apple2Bus::busRead(void *ctx, uint16_t address) {
    return static_cast<Apple2Bus *>(ctx)->read(address);
}

void Apple2Bus::busWrite(void *ctx, uint16_t address, uint8_t value) {
    static_cast<Apple2Bus *>(ctx)->write(address, value);
}

} // namespace esp_bracket
