#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_bracket/errors.hpp"

namespace esp_bracket {

struct CpuRegisters {
    uint8_t a;
    uint8_t x;
    uint8_t y;
    uint8_t sp;
    uint8_t status;
    uint16_t pc;
};

using CpuReadFn = uint8_t (*)(void *ctx, uint16_t address);
using CpuWriteFn = void (*)(void *ctx, uint16_t address, uint8_t value);

/**
 * Project-owned 6502 façade over vendored fake6502.
 * Application code must not call fake6502 APIs directly.
 */
class Cpu6502 {
  public:
    Cpu6502();
    ~Cpu6502();

    Cpu6502(const Cpu6502 &) = delete;
    Cpu6502 &operator=(const Cpu6502 &) = delete;

    void setCallbacks(void *ctx, CpuReadFn readFn, CpuWriteFn writeFn);
    void reset();
    uint32_t step();
    uint32_t runCycles(uint32_t cycles);
    void irq();
    void nmi();

    CpuRegisters registers() const;
    void setRegisters(const CpuRegisters &regs);
    uint64_t cycles() const { return totalCycles_; }
    /** Cycles including progress inside the current exec6502/step6502 batch. */
    uint64_t liveCycles() const;
    void clearCycleCounter() { totalCycles_ = 0; }

    CpuError lastError() const { return lastError_; }

    uint8_t busRead(uint16_t address) const { return readFn_ ? readFn_(ctx_, address) : 0xFF; }
    void busWrite(uint16_t address, uint8_t value) const {
        if (writeFn_) {
            writeFn_(ctx_, address, value);
        }
    }

    static Cpu6502 *active();

  private:
    void *ctx_ = nullptr;
    CpuReadFn readFn_ = nullptr;
    CpuWriteFn writeFn_ = nullptr;
    uint64_t totalCycles_ = 0;
    uint64_t execBaseCycles_ = 0;
    bool inExec_ = false;
    CpuError lastError_ = CpuError::Uninitialized;
    static Cpu6502 *s_active;
};

} // namespace esp_bracket
