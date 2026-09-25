#pragma once

#include <cstdint>
#include <cstddef>

namespace esp_bracket {

/** Fixed 64 KiB flat memory for CPU unit tests (no Apple II). */
class CpuHarness {
public:
    static constexpr size_t kMemSize = 65536;

    CpuHarness();

    void clear(uint8_t fill = 0x00);
    void write8(uint16_t addr, uint8_t v);
    void write16(uint16_t addr, uint16_t v);
    uint8_t read8(uint16_t addr) const;
    void setResetVector(uint16_t addr);
    void setIrqVector(uint16_t addr);
    void setNmiVector(uint16_t addr);
    void load(uint16_t addr, const uint8_t* bytes, size_t len);

    uint8_t* memory() { return mem_; }
    const uint8_t* memory() const { return mem_; }

    void enableTrace(bool on) { trace_ = on; }
    size_t traceCount() const { return traceCount_; }
    void clearTrace() { traceCount_ = 0; }

    static uint8_t harnessRead(void* ctx, uint16_t address);
    static void harnessWrite(void* ctx, uint16_t address, uint8_t value);

private:
    uint8_t mem_[kMemSize]{};
    bool trace_ = false;
    size_t traceCount_ = 0;
};

} // namespace esp_bracket
