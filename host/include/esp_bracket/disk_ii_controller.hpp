#pragma once

#include "esp_bracket/disk_ii_media.hpp"
#include "esp_bracket/slot_device.hpp"

#include <cstddef>
#include <cstdint>

namespace esp_bracket {

enum class DiskIIActivity : uint8_t {
    None = 0,
    MotorOn,
    MotorOff,
    DriveSelect,
    Step,
    Read,
    WriteAttempt,
    Insert,
    Eject
};

struct DiskIIDriveState {
    bool inserted = false;
    bool writeProtected = true;
    bool dirty = false;
    int quarterTrack = 0;
    int wholeTrack() const { return quarterTrack / 4; }
};

struct DiskIIDiagState {
    bool romPresent = false;
    bool romSynthetic = false;
    bool romCleanRoom = false;
    bool motorOn = false;
    int selectedDrive = 1;
    bool q6 = false;
    bool q7 = false;
    uint8_t phases = 0;
    uint8_t latch = 0;
    uint32_t rotationIndex = 0;
    DiskIIDriveState drive1{};
    DiskIIDriveState drive2{};
};

enum class DiskIIError : uint8_t {
    Ok = 0,
    NoDisk,
    WriteProtected,
    TrackNotFound,
    AddressFieldNotFound,
    DataFieldNotFound,
    ChecksumError,
    InvalidNibble,
    RotationTimeout,
    InvalidImage
};

enum class DiskIIBootTraceEvent : uint8_t {
    SlotRomEntry = 0,
    MotorOn,
    DriveSelect,
    Phase,
    Track,
    AddressMatch,
    SectorMatch,
    DataMatch,
    DecodeOk,
    TransferControl,
    Fail
};

struct DiskIIBootTraceEntry {
    DiskIIBootTraceEvent event;
    uint32_t cycle;
    uint16_t detail;
};

class DiskIIController : public SlotDevice {
  public:
    static constexpr int kSlotRomSize = 256;
    static constexpr int kExpansionRomSize = 2048;
    static constexpr int kMaxQuarterTrack = 139;
    static constexpr uint32_t kCyclesPerNibble = 32;

    DiskIIController();

    const char *name() const override { return "diskii"; }

    uint8_t ioRead(uint8_t offset, uint32_t cycle) override;
    void ioWrite(uint8_t offset, uint8_t value, uint32_t cycle) override;
    uint8_t ioPeek(uint8_t offset) const override;
    uint8_t romRead(uint16_t offset) override;
    uint8_t expansionRomRead(uint16_t offset) const override;
    bool hasExpansionRom() const override { return expansionLoaded_; }

    uint8_t access(uint8_t offset, uint32_t cycle, bool isWrite, uint8_t writeValue);
    void reset();

    enum class RomKind : uint8_t { None, Synthetic, CleanRoom, UserSupplied };
    bool loadSyntheticRom(const uint8_t *data, size_t size);
    bool loadCleanRoomRom(const uint8_t *prom256, const uint8_t *expansion2048);
    bool loadUserRom(const uint8_t *data, size_t size);
    void clearRom();
    RomKind romKind() const { return romKind_; }
    const uint8_t *romBytes() const { return rom_; }
    uint32_t romHash() const;

    void attachMedia(int drive, NibbleTrackMedia *media);
    NibbleTrackMedia *media(int drive) const;
    void ejectDrive(int drive);

    DiskIIDiagState diagState() const;
    DiskIIActivity lastActivity() const { return lastActivity_; }
    void clearLastActivity() { lastActivity_ = DiskIIActivity::None; }

    bool traceEnabled() const { return trace_; }
    void setTraceEnabled(bool on) { trace_ = on; }

    void setWritesEnabled(bool on) { writesEnabled_ = on; }
    bool writesEnabled() const { return writesEnabled_; }

    void setRotationIndex(uint32_t idx) {
        rotationPhaseOffset_ = idx;
        lastServedNibbleIndex_ = UINT32_MAX;
    }
    uint32_t rotationIndex() const { return rotationIndex_; }

    DiskIIDriveState &driveState(int drive);
    const DiskIIDriveState &driveState(int drive) const;

    void clearBootTrace();
    void pushBootTrace(DiskIIBootTraceEvent ev, uint32_t cycle, uint16_t detail = 0);
    size_t bootTraceCount() const { return bootTraceCount_; }
    const DiskIIBootTraceEntry *bootTrace() const { return bootTrace_; }

    DiskIIError lastError() const { return lastError_; }
    void setLastError(DiskIIError e) { lastError_ = e; }

  private:
    void applySwitch(uint8_t offset, uint32_t cycle);
    void updateStepper(uint8_t phase, bool on);
    void advanceRotation(uint32_t cycle);
    uint8_t readLatch(uint32_t cycle);
    uint8_t senseWriteProtect() const;
    NibbleTrackMedia *activeMedia() const;
    DiskIIDriveState &activeDriveState();
    const DiskIIDriveState &activeDriveState() const;
    void emitActivity(DiskIIActivity a);
    void traceLine(uint32_t cycle, const char *what, int value) const;

    uint8_t rom_[kSlotRomSize]{};
    uint8_t expansion_[kExpansionRomSize]{};
    bool expansionLoaded_ = false;
    RomKind romKind_ = RomKind::None;

    NibbleTrackMedia *media1_ = nullptr;
    NibbleTrackMedia *media2_ = nullptr;
    DiskIIDriveState drive1_{};
    DiskIIDriveState drive2_{};

    bool motorOn_ = false;
    int selectedDrive_ = 1;
    bool q6_ = false;
    bool q7_ = false;
    uint8_t phases_ = 0;
    int lastPhase_ = -1;

    uint8_t latch_ = 0;
    uint32_t lastCycle_ = 0;
    uint32_t rotationIndex_ = 0;
    uint32_t rotationPhaseOffset_ = 0;
    uint32_t motorStartCycle_ = 0;
    /** Last nibble index returned with bit7 set; next read in same window clears bit7. */
    uint32_t lastServedNibbleIndex_ = UINT32_MAX;

    bool writesEnabled_ = false;
    bool trace_ = false;
    DiskIIActivity lastActivity_ = DiskIIActivity::None;
    DiskIIError lastError_ = DiskIIError::Ok;

    static constexpr size_t kBootTraceCap = 64;
    DiskIIBootTraceEntry bootTrace_[kBootTraceCap]{};
    size_t bootTraceCount_ = 0;
};

size_t generateSyntheticDiskIISlotRom(uint8_t *dst, size_t dstCap);
bool generateEsp2DiskTestImage(uint8_t *dst, size_t dstCap);
bool generateCleanRoomDiskIICard(uint8_t *prom256, uint8_t *expansion2048);
bool generateEsp2BootTestImage(uint8_t *dst, size_t dstCap);

} // namespace esp_bracket
