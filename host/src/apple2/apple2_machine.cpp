#include "esp_bracket/apple2_machine_host.hpp"

#include "esp_bracket/disk_ii_cleanroom.hpp"
#include "esp_bracket/hgr_decoder.hpp"
#include "esp_bracket/lores_decoder.hpp"
#include "esp_bracket/text_decoder.hpp"

#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace esp_bracket {

HostAppleIIMachine::HostAppleIIMachine() {
    cpu_.setCallbacks(&bus_, Apple2Bus::busRead, Apple2Bus::busWrite);
    wireDiskII();
}

void HostAppleIIMachine::wireDiskII() {
    bus_.setSlotDevice(6, &diskII_);
    // Default: no Slot-6 ROM so Autostart/user ROM is not diverted to synthetic boot.
    diskII_.clearRom();
    slot6RomMode_ = Slot6RomMode::None;
}

void HostAppleIIMachine::applySlot6RomMode() {
    switch (slot6RomMode_) {
    case Slot6RomMode::None:
        diskII_.clearRom();
        break;
    case Slot6RomMode::Synthetic: {
        uint8_t slotRom[DiskIIController::kSlotRomSize];
        if (generateSyntheticDiskIISlotRom(slotRom, sizeof(slotRom)) ==
            DiskIIController::kSlotRomSize) {
            diskII_.loadSyntheticRom(slotRom, sizeof(slotRom));
        }
        break;
    }
    case Slot6RomMode::CleanRoom: {
        uint8_t prom[DiskIIController::kSlotRomSize];
        uint8_t exp[DiskIIController::kExpansionRomSize];
        if (generateCleanRoomDiskIICard(prom, exp)) {
            diskII_.loadCleanRoomRom(prom, exp);
        }
        break;
    }
    case Slot6RomMode::UserSupplied:
        break;
    }
}

void HostAppleIIMachine::setSlot6RomMode(Slot6RomMode mode) {
    slot6RomMode_ = mode;
    applySlot6RomMode();
}

RomError HostAppleIIMachine::loadSlot6UserRom(const uint8_t *data, size_t size,
                                              Slot6RomIdentity *outId) {
    Slot6RomIdentity id = Slot6RomDatabase::identify(data, size);
    if (outId) {
        *outId = id;
    }
    slot6RomIdentity_ = id;
    if (id.status == RomIdStatus::IoError) {
        return RomError::IoError;
    }
    if (id.status == RomIdStatus::InvalidSize || id.status == RomIdStatus::Truncated ||
        id.status == RomIdStatus::Unsupported) {
        return RomError::InvalidSize;
    }
    if (!diskII_.loadUserRom(data, size)) {
        return RomError::InvalidSize;
    }
    slot6RomMode_ = Slot6RomMode::UserSupplied;
    return RomError::Ok;
}

void HostAppleIIMachine::syncBusCycle() {
    bus_.setAccessCycle(static_cast<uint32_t>(cpu_.cycles() & 0xFFFFFFFFu));
}

void HostAppleIIMachine::powerOn(RamInitMode ramInit) {
    if (ramInit == RamInitMode::Zero) {
        bus_.clearRam();
    } else if (ramInit == RamInitMode::Ones) {
        // $FF fill approximates common cold DRAM appearance better than zeros.
        // Software that reads uncleared main RAM (e.g. some boot obfuscators)
        // can depend on non-zero bytes. Keep deterministic: all $FF.
        std::memset(bus_.ram(), 0xFF, Apple2Bus::kRamBytes);
    } else if (ramInit == RamInitMode::Random) {
        // Deterministic LCG — reproducible "garbage" for titles that read
        // uncleared RAM. Not cryptographic.
        uint32_t s = 0xA5A5F00Du;
        uint8_t *ram = bus_.ram();
        for (size_t i = 0; i < Apple2Bus::kRamBytes; ++i) {
            s = s * 1664525u + 1013904223u;
            ram[i] = static_cast<uint8_t>((s >> 24) & 0xFFu);
        }
    }
    reset();
}

void HostAppleIIMachine::reset() {
    // Apple II reset: soft switches / keyboard / speaker / CPU.
    // Disks remain mounted. ROM image remains loaded. Slot ROM mode retained.
    const VirtualDriveState d1 = drive1_.state();
    const VirtualDriveState d2 = drive2_.state();
    bus_.reset();
    if (d1.inserted) {
        drive1_.mount(d1.imageId, d1.format);
    }
    if (d2.inserted) {
        drive2_.mount(d2.imageId, d2.format);
    }
    applySlot6RomMode();
    syncBusCycle();
    cpu_.reset();
}

void HostAppleIIMachine::resetCpuOnly() {
    syncBusCycle();
    cpu_.reset();
}

void HostAppleIIMachine::runCycles(uint32_t cycles) {
    syncBusCycle();
    // Service clean-room denibble handshake without bypassing Disk II reads.
    const uint32_t slice = 64;
    uint32_t left = cycles;
    while (left > 0) {
        const uint32_t step = left > slice ? slice : left;
        cpu_.runCycles(step);
        syncBusCycle();
        if (diskII_.romKind() == DiskIIController::RomKind::CleanRoom) {
            serviceCleanRoomCardRequests(bus_.ram(), diskII_);
        }
        left -= step;
    }
    syncBusCycle();
}

void HostAppleIIMachine::keyDown(uint8_t normalizedKey) {
    bus_.keyboard().keyDown(normalizedKey);
}

void HostAppleIIMachine::keyUp(uint8_t normalizedKey) {
    bus_.keyboard().keyUp(normalizedKey);
}

void HostAppleIIMachine::setJoystick(const JoystickState &state) {
    joystick_ = state;
    bus_.gameIo().setButton(0, state.button0);
    bus_.gameIo().setButton(1, state.button1);
    const auto toPdl = [](int16_t v) -> uint8_t {
        const int scaled = (static_cast<int>(v) + 32768) * 255 / 65535;
        if (scaled < 0) {
            return 0;
        }
        if (scaled > 255) {
            return 255;
        }
        return static_cast<uint8_t>(scaled);
    };
    bus_.gameIo().setPaddle(0, toPdl(state.x));
    bus_.gameIo().setPaddle(1, toPdl(state.y));
}

void HostAppleIIMachine::setPaddle(const PaddleState &state) {
    paddle_ = state;
    bus_.gameIo().setPaddle(0, static_cast<uint8_t>(state.p0 > 255 ? 255 : state.p0));
    bus_.gameIo().setPaddle(1, static_cast<uint8_t>(state.p1 > 255 ? 255 : state.p1));
    bus_.gameIo().setButton(0, state.button0);
    bus_.gameIo().setButton(1, state.button1);
}

MediaResult HostAppleIIMachine::mountDisk(DriveId id, const char *imageId) {
    if (!imageId || !imageId[0]) {
        return MediaResult::InvalidImage;
    }
    Dos33NibbleImage &img = diskImage(id);
    const int driveNo = (id == DriveId::Drive2) ? 2 : 1;

    if (std::strcmp(imageId, "Esp2BootTest") == 0 ||
        std::strcmp(imageId, "Esp2BootTest.dsk") == 0) {
        uint8_t raw[kDos33ImageBytes];
        if (!generateEsp2BootTestImage(raw, sizeof(raw))) {
            return MediaResult::InvalidImage;
        }
        if (!img.load(raw, sizeof(raw), false)) {
            return MediaResult::InvalidImage;
        }
        img.setWriteProtected(true);
        diskII_.attachMedia(driveNo, &img);
        setSlot6RomMode(Slot6RomMode::CleanRoom);
        return drive(id).mount(imageId, DiskFormat::Dsk);
    }

    if (std::strcmp(imageId, "Esp2BootTest.po") == 0) {
        uint8_t raw[kDos33ImageBytes];
        if (!generateEsp2BootTestImage(raw, sizeof(raw))) {
            return MediaResult::InvalidImage;
        }
        // T0S0 occupies file sector 0 in both DO and PO layouts.
        if (!img.load(raw, sizeof(raw), true)) {
            return MediaResult::InvalidImage;
        }
        img.setWriteProtected(true);
        diskII_.attachMedia(driveNo, &img);
        setSlot6RomMode(Slot6RomMode::CleanRoom);
        return drive(id).mount(imageId, DiskFormat::Po);
    }

    if (std::strcmp(imageId, "Esp2DiskTest") == 0 ||
        std::strcmp(imageId, "Esp2DiskTest.dsk") == 0) {
        uint8_t raw[kDos33ImageBytes];
        if (!generateEsp2DiskTestImage(raw, sizeof(raw))) {
            return MediaResult::InvalidImage;
        }
        if (!img.load(raw, sizeof(raw), false)) {
            return MediaResult::InvalidImage;
        }
        img.setWriteProtected(true);
        diskII_.attachMedia(driveNo, &img);
        // Disk boot path needs synthetic Slot-6 ROM.
        setSlot6RomMode(Slot6RomMode::Synthetic);
        return drive(id).mount(imageId, DiskFormat::Dsk);
    }

    // Host path: load a local .dsk / .do / .po image from the filesystem.
    // Used by apple2:compat with gitignored local/apple2/disks/ media.
    {
        std::ifstream in(imageId, std::ios::binary);
        if (in) {
            in.seekg(0, std::ios::end);
            const std::streamoff len = in.tellg();
            in.seekg(0, std::ios::beg);
            if (len == static_cast<std::streamoff>(kDos33ImageBytes)) {
                std::vector<uint8_t> raw(static_cast<size_t>(len));
                if (in.read(reinterpret_cast<char *>(raw.data()), len)) {
                    std::string path(imageId);
                    for (char &c : path) {
                        if (c >= 'A' && c <= 'Z') {
                            c = static_cast<char>(c - 'A' + 'a');
                        }
                    }
                    const bool poOrder =
                        path.size() >= 3 && path.compare(path.size() - 3, 3, ".po") == 0;
                    if (!img.load(raw.data(), raw.size(), poOrder)) {
                        return MediaResult::InvalidImage;
                    }
                    img.setWriteProtected(true);
                    diskII_.attachMedia(driveNo, &img);
                    if (slot6RomMode_ == Slot6RomMode::None) {
                        setSlot6RomMode(Slot6RomMode::CleanRoom);
                    }
                    return drive(id).mount(imageId, poOrder ? DiskFormat::Po : DiskFormat::Dsk);
                }
            }
            return MediaResult::InvalidImage;
        }
    }

    diskII_.ejectDrive(driveNo);
    return drive(id).mount(imageId, DiskFormat::Unknown);
}

MediaResult HostAppleIIMachine::unmountDisk(DriveId id) {
    const int driveNo = (id == DriveId::Drive2) ? 2 : 1;
    diskII_.ejectDrive(driveNo);
    diskImage(id).eject();
    return drive(id).unmount();
}

VideoFrameState HostAppleIIMachine::getVideoState() const {
    return toVideoFrameState(videoStateFromSoftSwitches(bus_.softSwitches()), colorMode_);
}

size_t HostAppleIIMachine::consumeAudioEvents(SpeakerEvent *out, size_t max) {
    return bus_.speaker().consumeEvents(out, max);
}

RomError HostAppleIIMachine::loadSyntheticRom() {
    const RomError err = bus_.rom().loadSynthetic();
    if (err == RomError::Ok) {
        romIdentity_ = RomDatabase::identify(bus_.rom().data(), bus_.rom().size());
        if (romIdentity_.profile != MachineProfile::Unknown) {
            profile_ = romIdentity_.profile;
        }
    }
    return err;
}

RomError HostAppleIIMachine::loadRom(const uint8_t *data, size_t size) {
    return loadRomIdentified(data, size, nullptr);
}

RomError HostAppleIIMachine::loadRomIdentified(const uint8_t *data, size_t size,
                                               RomIdentity *outId) {
    RomIdentity id{};
    const RomError err = loadAndIdentifyRom(bus_.rom(), data, size, &id);
    romIdentity_ = id;
    if (outId) {
        *outId = id;
    }
    if (err == RomError::Ok) {
        if (id.profile != MachineProfile::Unknown) {
            profile_ = id.profile;
        }
        // Unknown hash: leave profile as previously set / Unknown — caller selects.
    }
    return err;
}

HostMachineSnapshot HostAppleIIMachine::snapshot() const {
    HostMachineSnapshot s{};
    const CpuRegisters r = cpu_.registers();
    s.pc = r.pc;
    s.a = r.a;
    s.x = r.x;
    s.y = r.y;
    s.sp = r.sp;
    s.status = r.status;
    s.cycles = cpu_.cycles();
    s.text = bus_.softSwitches().isText();
    s.mixed = bus_.softSwitches().isMixed();
    s.page2 = bus_.softSwitches().isPage2();
    s.hires = bus_.softSwitches().isHires();
    s.kbdLatch = bus_.keyboard().latch();
    const DiskIIDiagState d = diskII_.diagState();
    s.motorOn = d.motorOn;
    s.selectedDrive = d.selectedDrive;
    s.slot6RomMode = static_cast<int>(slot6RomMode_);
    s.profile = profile_;
    return s;
}

DiagReport HostAppleIIMachine::runSelfTest() {
    DiagReport r{};
    r.cpu = DiagResult::NotTested;
    r.ram = DiagResult::NotTested;
    r.rom = DiagResult::NotTested;
    r.softswitch = DiagResult::NotTested;
    r.keyboard = DiagResult::NotTested;
    r.speaker = DiagResult::NotTested;
    r.text = DiagResult::NotTested;
    r.lores = DiagResult::NotTested;
    r.hgr = DiagResult::NotTested;

    r.rom = bus_.rom().isLoaded() ? DiagResult::Pass : DiagResult::Fail;

    bus_.write(0x0300, 0xA5);
    r.ram = (bus_.read(0x0300) == 0xA5) ? DiagResult::Pass : DiagResult::Fail;

    bus_.softSwitches().reset();
    bus_.write(0xC050, 0);
    const bool graphics = bus_.softSwitches().isGraphics();
    bus_.write(0xC051, 0);
    const bool text = bus_.softSwitches().isText();
    r.softswitch = (graphics && text) ? DiagResult::Pass : DiagResult::Fail;

    bus_.keyboard().reset();
    bus_.keyboard().keyDown('A');
    const bool strobe = bus_.keyboard().strobePending();
    bus_.keyboard().clearStrobe();
    r.keyboard = (strobe && !bus_.keyboard().strobePending()) ? DiagResult::Pass : DiagResult::Fail;

    bus_.speaker().reset();
    bus_.setAccessCycle(100);
    bus_.read(0xC030);
    bus_.read(0xC030);
    SpeakerEvent ev[4];
    const size_t n = bus_.speaker().consumeEvents(ev, 4);
    r.speaker = (n == 2 && ev[0].level != ev[1].level) ? DiagResult::Pass : DiagResult::Fail;

    TextDecoder::writeTextScreen(bus_.ram(), 0x0400, "ESP][", 0, 0);
    uint8_t chars[40 * 24];
    TextDecoder::decodeScreen(bus_.ram(), 0x0400, chars);
    r.text = ((chars[0] & 0x7F) == 'E') ? DiagResult::Pass : DiagResult::Fail;

    LoresDecoder::writeTestPattern(bus_.ram(), 0x0400);
    uint8_t blocks[40 * 48];
    LoresDecoder::decode(bus_.ram(), 0x0400, blocks);
    r.lores = (blocks[0] <= 15) ? DiagResult::Pass : DiagResult::Fail;

    const uint16_t a0 = HgrDecoder::lineAddress(0x2000, 0);
    const uint16_t a1 = HgrDecoder::lineAddress(0x2000, 1);
    r.hgr = (a0 == 0x2000 && a1 == 0x2400) ? DiagResult::Pass : DiagResult::Fail;

    if (bus_.rom().isLoaded()) {
        cpu_.reset();
        const uint32_t ticks = cpu_.step();
        r.cpu = (ticks > 0) ? DiagResult::Pass : DiagResult::Fail;
    } else {
        r.cpu = DiagResult::Fail;
    }

    return r;
}

} // namespace esp_bracket
