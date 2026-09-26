#include "esp_bracket/cpu6502.hpp"
#include "fake6502_api.hpp"

namespace esp_bracket {

Cpu6502 *Cpu6502::s_active = nullptr;

Cpu6502::Cpu6502() = default;

Cpu6502::~Cpu6502() {
    if (s_active == this) {
        s_active = nullptr;
    }
}

Cpu6502 *Cpu6502::active() {
    return s_active;
}

void Cpu6502::setCallbacks(void *ctx, CpuReadFn readFn, CpuWriteFn writeFn) {
    ctx_ = ctx;
    readFn_ = readFn;
    writeFn_ = writeFn;
    lastError_ = (readFn && writeFn) ? CpuError::Ok : CpuError::NoMemoryCallbacks;
}

void Cpu6502::reset() {
    if (!readFn_ || !writeFn_) {
        lastError_ = CpuError::NoMemoryCallbacks;
        return;
    }
    s_active = this;
    reset6502();
    totalCycles_ = 0;
    execBaseCycles_ = 0;
    inExec_ = false;
    lastError_ = CpuError::Ok;
}

uint32_t Cpu6502::step() {
    if (!readFn_ || !writeFn_) {
        lastError_ = CpuError::NoMemoryCallbacks;
        return 0;
    }
    s_active = this;
    execBaseCycles_ = totalCycles_;
    inExec_ = true;
    const uint32_t ticks = step6502();
    inExec_ = false;
    totalCycles_ += ticks;
    return ticks;
}

uint32_t Cpu6502::runCycles(uint32_t cycles) {
    if (!readFn_ || !writeFn_) {
        lastError_ = CpuError::NoMemoryCallbacks;
        return 0;
    }
    s_active = this;
    execBaseCycles_ = totalCycles_;
    inExec_ = true;
    const uint32_t ticks = exec6502(cycles);
    inExec_ = false;
    totalCycles_ += ticks;
    return ticks;
}

uint64_t Cpu6502::liveCycles() const {
    if (inExec_) {
        return execBaseCycles_ + static_cast<uint64_t>(clockticks6502);
    }
    return totalCycles_;
}

void Cpu6502::irq() {
    s_active = this;
    irq6502();
}

void Cpu6502::nmi() {
    s_active = this;
    nmi6502();
}

CpuRegisters Cpu6502::registers() const {
    CpuRegisters r{};
    r.a = a;
    r.x = x;
    r.y = y;
    r.sp = sp;
    r.status = status;
    r.pc = pc;
    return r;
}

void Cpu6502::setRegisters(const CpuRegisters &regs) {
    a = regs.a;
    x = regs.x;
    y = regs.y;
    sp = regs.sp;
    status = regs.status;
    pc = regs.pc;
}

} // namespace esp_bracket

extern "C" uint8 read6502(ushort address) {
    esp_bracket::Cpu6502 *cpu = esp_bracket::Cpu6502::active();
    if (!cpu) {
        return 0xFF;
    }
    return cpu->busRead(address);
}

extern "C" void write6502(ushort address, uint8 value) {
    esp_bracket::Cpu6502 *cpu = esp_bracket::Cpu6502::active();
    if (!cpu) {
        return;
    }
    cpu->busWrite(address, value);
}
