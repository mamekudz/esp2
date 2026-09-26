#pragma once

#include "esp_bracket/apple2_bus.hpp"
#include "esp_bracket/apple2_machine.hpp"
#include "esp_bracket/cpu6502.hpp"
#include "esp_bracket/disk_ii_controller.hpp"
#include "esp_bracket/disk_ii_media.hpp"
#include "esp_bracket/errors.hpp"
#include "esp_bracket/types.hpp"
#include "esp_bracket/video_state.hpp"
#include "esp_bracket/virtual_drive.hpp"

namespace esp_bracket {

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

    Apple2Bus &bus() { return bus_; }
    const Apple2Bus &bus() const { return bus_; }
    Cpu6502 &cpu() { return cpu_; }
    const Cpu6502 &cpu() const { return cpu_; }

    VirtualDrive &drive(DriveId id) { return id == DriveId::Drive2 ? drive2_ : drive1_; }

    DiskIIController &diskII() { return diskII_; }
    const DiskIIController &diskII() const { return diskII_; }

    Dos33NibbleImage &diskImage(DriveId id) {
        return id == DriveId::Drive2 ? diskImage2_ : diskImage1_;
    }

    void setColorMode(VideoColorMode mode) { colorMode_ = mode; }
    VideoColorMode colorMode() const { return colorMode_; }

    DiagReport runSelfTest();

    /** CPU-only reset vs Apple II reset (disks stay mounted). */
    void resetCpuOnly();

  private:
    void syncBusCycle();
    void wireDiskII();

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
};

} // namespace esp_bracket
