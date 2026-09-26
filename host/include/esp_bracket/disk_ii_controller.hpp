#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_bracket/disk_ii_media.hpp"
#include "esp_bracket/slot_device.hpp"

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
    int quarterTrack = 0; // 0..139 for 35 tracks
    int wholeTrack() const { return quarterTrack / 4; }
};

struct DiskIIDiagState {
    bool romPresent = false;
    bool romSynthetic = false;
    bool motorOn = false;
    int selectedDrive = 1; // 1 or 2
    bool q6 = false;
    bool q7 = false;    // write mode when true
    uint8_t phases = 0; // bit0..3
    uint8_t latch = 0;
    uint32_t rotationIndex = 0;
    DiskIIDriveState drive1{};
    DiskIIDriveState drive2{};
};

/**
 * Disk II interface card (SlotDevice). Host-side; no ESP32 deps.
 */
class DiskIIController : public SlotDevice {
  public:
    static constexpr int kSlotRomSize = 256;
    static constexpr int kMaxQuarterTrack = 139;     // track 34.75
    static constexpr uint32_t kCyclesPerNibble = 32; // APPROXIMATE

    DiskIIController();

    const char *name() const override { return "diskii"; }

    uint8_t ioRead(uint8_t offset, uint32_t cycle) override;
    void ioWrite(uint8_t offset, uint8_t value, uint32_t cycle) override;
    uint8_t ioPeek(uint8_t offset) const override;
    uint8_t romRead(uint16_t offset) override;

    /** Soft-switch access (read or write) — real Disk II is access-triggered. */
    uint8_t access(uint8_t offset, uint32_t cycle, bool isWrite, uint8_t writeValue);

    void reset();

    // --- Slot ROM ---
    enum class RomKind : uint8_t { None, Synthetic, UserSupplied };
    bool loadSyntheticRom(const uint8_t *data, size_t size);
    bool loadUserRom(const uint8_t *data, size_t size);
    void clearRom();
    RomKind romKind() const { return romKind_; }
    const uint8_t *romBytes() const { return rom_; }
    /** FNV-1a style hash for identification (not crypto). */
    uint32_t romHash() const;

    // --- Media ---
    void attachMedia(int drive /*1|2*/, NibbleTrackMedia *media);
    NibbleTrackMedia *media(int drive) const;
    void ejectDrive(int drive);

    // --- Observability (no mutation) ---
    DiskIIDiagState diagState() const;
    DiskIIActivity lastActivity() const { return lastActivity_; }
    void clearLastActivity() { lastActivity_ = DiskIIActivity::None; }

    bool traceEnabled() const { return trace_; }
    void setTraceEnabled(bool on) { trace_ = on; }

    /** Writes enabled only when explicitly set (V1 default: false). */
    void setWritesEnabled(bool on) { writesEnabled_ = on; }
    bool writesEnabled() const { return writesEnabled_; }

    DiskIIDriveState &driveState(int drive);
    const DiskIIDriveState &driveState(int drive) const;

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
    RomKind romKind_ = RomKind::None;

    NibbleTrackMedia *media1_ = nullptr;
    NibbleTrackMedia *media2_ = nullptr;
    DiskIIDriveState drive1_{};
    DiskIIDriveState drive2_{};

    bool motorOn_ = false;
    int selectedDrive_ = 1; // 1 or 2
    bool q6_ = false;
    bool q7_ = false;
    uint8_t phases_ = 0;
    int lastPhase_ = -1;

    uint8_t latch_ = 0;
    uint32_t lastCycle_ = 0;
    uint32_t rotationIndex_ = 0;
    uint32_t motorStartCycle_ = 0;

    bool writesEnabled_ = false;
    bool trace_ = false;
    DiskIIActivity lastActivity_ = DiskIIActivity::None;
};

/** Generate project-owned synthetic Slot-6 ROM (NOT Apple's Disk II ROM). */
size_t generateSyntheticDiskIISlotRom(uint8_t *dst, size_t dstCap);

/** Build Esp2DiskTest.dsk contents (143360 bytes) into dst. */
bool generateEsp2DiskTestImage(uint8_t *dst, size_t dstCap);

} // namespace esp_bracket
