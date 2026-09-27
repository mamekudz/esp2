#pragma once

#include "esp_bracket/apple2_bus.hpp"
#include "esp_bracket/apple2_machine.hpp"
#include "esp_bracket/cpu6502.hpp"
#include "esp_bracket/disk_ii_controller.hpp"
#include "esp_bracket/disk_ii_media.hpp"
#include "esp_bracket/errors.hpp"
#include "esp_bracket/rom_identity.hpp"
#include "esp_bracket/types.hpp"
#include "esp_bracket/video_state.hpp"
#include "esp_bracket/virtual_drive.hpp"

namespace esp_bracket {

enum class RamInitMode : uint8_t {
    Zero = 0,  // deterministic tests
    Ones,      // 0xFF
    Random,    // deterministic LCG fill (cold-DRAM-like; not secure)
    Unchanged, // leave as-is (powerOn with Unchanged skips fill)
    /**
     * AppleWin / apple2js `allocMem` power-on pattern:
     *   (addr & 2) ? 0x00 : 0xFF, plus sparse deterministic "garbage"
     * at +0x28/+0x29/+0x68/+0x69 every 0x200 bytes.
     * Matches https://www.scullinsteel.com/apple2/ cold RAM model.
     */
    DramAppleWin
};

/** Machine-readable host diagnostic snapshot (language-neutral fields). */
struct HostMachineSnapshot {
    uint16_t pc = 0;
    uint8_t a = 0;
    uint8_t x = 0;
    uint8_t y = 0;
    uint8_t sp = 0;
    uint8_t status = 0;
    uint64_t cycles = 0;
    bool text = true;
    bool mixed = false;
    bool page2 = false;
    bool hires = false;
    uint8_t kbdLatch = 0;
    bool motorOn = false;
    int selectedDrive = 1;
    int slot6RomMode = 0;
    MachineProfile profile = MachineProfile::Unknown;
};

/**
 * Host-side concrete Apple II machine (transport-independent).
 * Not linked into ESP32 firmware in this milestone.
 */
class HostAppleIIMachine : public AppleIIMachine {
  public:
    HostAppleIIMachine();

    void reset() override;
    void runCycles(uint32_t cycles) override;
    void keyDown(uint8_t normalizedKey) override;
    void keyUp(uint8_t normalizedKey) override;
    void setJoystick(const JoystickState &state) override;
    void setPaddle(const PaddleState &state) override;
    MediaResult mountDisk(DriveId id, const char *imageId) override;
    MediaResult unmountDisk(DriveId id) override;
    VideoFrameState getVideoState() const override;
    size_t consumeAudioEvents(SpeakerEvent *out, size_t max) override;

    /** Load synthetic ROM (required before meaningful reset vector fetch). */
    RomError loadSyntheticRom();
    RomError loadRom(const uint8_t *data, size_t size);
    /** Load + identify; unknown hash still loads if size valid. */
    RomError loadRomIdentified(const uint8_t *data, size_t size, RomIdentity *outId);

    /**
     * Power-on: optional RAM init, Apple II reset path, CPU RESET via vector.
     * Does not jump to a hardcoded BASIC entry.
     */
    void powerOn(RamInitMode ramInit = RamInitMode::Zero);

    Apple2Bus &bus() { return bus_; }
    const Apple2Bus &bus() const { return bus_; }
    Cpu6502 &cpu() { return cpu_; }
    const Cpu6502 &cpu() const { return cpu_; }

    VirtualDrive &drive(DriveId id) { return id == DriveId::Drive2 ? drive2_ : drive1_; }
    const VirtualDrive &drive(DriveId id) const {
        return id == DriveId::Drive2 ? drive2_ : drive1_;
    }

    DiskIIController &diskII() { return diskII_; }
    const DiskIIController &diskII() const { return diskII_; }

    Dos33NibbleImage &diskImage(DriveId id) {
        return id == DriveId::Drive2 ? diskImage2_ : diskImage1_;
    }

    void setColorMode(VideoColorMode mode) { colorMode_ = mode; }
    VideoColorMode colorMode() const { return colorMode_; }

    void setMachineProfile(MachineProfile p) { profile_ = p; }
    MachineProfile machineProfile() const { return profile_; }
    const RomIdentity &romIdentity() const { return romIdentity_; }

    void setSlot6RomMode(Slot6RomMode mode);
    Slot6RomMode slot6RomMode() const { return slot6RomMode_; }

    /** Load user-supplied 256-byte Slot-6 PROM (identified by hash, not filename). */
    RomError loadSlot6UserRom(const uint8_t *data, size_t size, Slot6RomIdentity *outId = nullptr);
    const Slot6RomIdentity &slot6RomIdentity() const { return slot6RomIdentity_; }

    void setFlashPhase(int phase) { flashPhase_ = phase & 1; }
    int flashPhase() const { return flashPhase_; }

    DiagReport runSelfTest();
    HostMachineSnapshot snapshot() const;

    /** CPU-only reset vs Apple II reset (disks stay mounted). */
    void resetCpuOnly();

  private:
    void syncBusCycle();
    void wireDiskII();
    void applySlot6RomMode();

    Apple2Bus bus_;
    Cpu6502 cpu_;
    VirtualDrive drive1_;
    VirtualDrive drive2_;
    DiskIIController diskII_;
    Dos33NibbleImage diskImage1_;
    Dos33NibbleImage diskImage2_;
    JoystickState joystick_{};
    PaddleState paddle_{};
    VideoColorMode colorMode_ = VideoColorMode::CompositeColor;
    MachineProfile profile_ = MachineProfile::Unknown;
    RomIdentity romIdentity_{};
    Slot6RomIdentity slot6RomIdentity_{};
    Slot6RomMode slot6RomMode_ = Slot6RomMode::None;
    int flashPhase_ = 0;
};

} // namespace esp_bracket
