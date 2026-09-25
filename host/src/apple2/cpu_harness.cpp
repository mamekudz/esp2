#include "esp_bracket/cpu_harness.hpp"

namespace esp_bracket {

CpuHarness::CpuHarness() {
    clear(0x00);
}

void CpuHarness::clear(uint8_t fill) {
    for (size_t i = 0; i < kMemSize; ++i) {
        mem_[i] = fill;
    }
    traceCount_ = 0;
}

void CpuHarness::write8(uint16_t addr, uint8_t v) {
    mem_[addr] = v;
}

void CpuHarness::write16(uint16_t addr, uint16_t v) {
    mem_[addr] = static_cast<uint8_t>(v & 0xFF);
    mem_[static_cast<uint16_t>(addr + 1)] = static_cast<uint8_t>((v >> 8) & 0xFF);
}

uint8_t CpuHarness::read8(uint16_t addr) const {
    return mem_[addr];
}

void CpuHarness::setResetVector(uint16_t addr) {
    write16(0xFFFC, addr);
}

void CpuHarness::setIrqVector(uint16_t addr) {
    write16(0xFFFE, addr);
}

void CpuHarness::setNmiVector(uint16_t addr) {
    write16(0xFFFA, addr);
}

void CpuHarness::load(uint16_t addr, const uint8_t* bytes, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        mem_[static_cast<uint16_t>(addr + i)] = bytes[i];
    }
}

uint8_t CpuHarness::harnessRead(void* ctx, uint16_t address) {
    auto* h = static_cast<CpuHarness*>(ctx);
    if (h->trace_) {
        ++h->traceCount_;
    }
    return h->mem_[address];
}

void CpuHarness::harnessWrite(void* ctx, uint16_t address, uint8_t value) {
    auto* h = static_cast<CpuHarness*>(ctx);
    if (h->trace_) {
        ++h->traceCount_;
    }
    h->mem_[address] = value;
}

} // namespace esp_bracket
