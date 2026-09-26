#pragma once

#include <cstddef>
#include <cstdint>

namespace esp_bracket {

/**
 * Future peripheral card abstraction (Disk II, serial, …).
 * Empty slots return open-bus approximation via the bus.
 */
class SlotDevice {
  public:
    virtual ~SlotDevice() = default;
    virtual uint8_t ioRead(uint8_t offset, uint32_t cycle) = 0;
    virtual void ioWrite(uint8_t offset, uint8_t value, uint32_t cycle) = 0;
    /**
     * Diagnostic I/O observe — must not mutate device state (rotation, switches).
     * Default: open-bus style 0xFF.
     */
    virtual uint8_t ioPeek(uint8_t /*offset*/) const { return 0xFF; }
    virtual uint8_t romRead(uint16_t offset) = 0;
    virtual const char *name() const { return "slot"; }
};

/** Records accesses for host tests (no Disk II behavior). */
class SpySlotDevice : public SlotDevice {
  public:
    uint8_t ioRead(uint8_t offset, uint32_t cycle) override {
        lastOffset_ = offset;
        lastCycle_ = cycle;
        ++readCount_;
        return lastReadValue_;
    }
    void ioWrite(uint8_t offset, uint8_t value, uint32_t cycle) override {
        lastOffset_ = offset;
        lastWriteValue_ = value;
        lastCycle_ = cycle;
        ++writeCount_;
    }
    uint8_t romRead(uint16_t offset) override {
        lastRomOffset_ = offset;
        ++romReadCount_;
        if (rom_ && offset < romSize_) {
            return rom_[offset];
        }
        return 0xFF;
    }
    const char *name() const override { return "spy"; }

    void setRom(const uint8_t *data, size_t size) {
        rom_ = data;
        romSize_ = size;
    }
    void setReadValue(uint8_t v) { lastReadValue_ = v; }

    uint8_t lastOffset() const { return lastOffset_; }
    uint8_t lastWriteValue() const { return lastWriteValue_; }
    uint16_t lastRomOffset() const { return lastRomOffset_; }
    uint32_t readCount() const { return readCount_; }
    uint32_t writeCount() const { return writeCount_; }
    uint32_t romReadCount() const { return romReadCount_; }

  private:
    const uint8_t *rom_ = nullptr;
    size_t romSize_ = 0;
    uint8_t lastReadValue_ = 0xA5;
    uint8_t lastOffset_ = 0;
    uint8_t lastWriteValue_ = 0;
    uint16_t lastRomOffset_ = 0;
    uint32_t lastCycle_ = 0;
    uint32_t readCount_ = 0;
    uint32_t writeCount_ = 0;
    uint32_t romReadCount_ = 0;
};

} // namespace esp_bracket
