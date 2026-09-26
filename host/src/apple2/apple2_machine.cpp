#include "esp_bracket/apple2_machine_host.hpp"

#include "esp_bracket/hgr_decoder.hpp"
#include "esp_bracket/lores_decoder.hpp"
#include "esp_bracket/text_decoder.hpp"

#include <cstring>

namespace esp_bracket {

HostAppleIIMachine::HostAppleIIMachine() {
    cpu_.setCallbacks(&bus_, Apple2Bus::busRead, Apple2Bus::busWrite);
}

void HostAppleIIMachine::syncBusCycle() {
    bus_.setAccessCycle(static_cast<uint32_t>(cpu_.cycles() & 0xFFFFFFFFu));
}

void HostAppleIIMachine::reset() {
    // Apple II reset: soft switches / keyboard / speaker / CPU.
    // Disks remain mounted. ROM image remains loaded.
    const VirtualDriveState d1 = drive1_.state();
    const VirtualDriveState d2 = drive2_.state();
    bus_.reset();
    if (d1.inserted) {
        drive1_.mount(d1.imageId, d1.format);
    }
    if (d2.inserted) {
        drive2_.mount(d2.imageId, d2.format);
    }
    syncBusCycle();
    cpu_.reset();
}

void HostAppleIIMachine::resetCpuOnly() {
    syncBusCycle();
    cpu_.reset();
}

void HostAppleIIMachine::runCycles(uint32_t cycles) {
    syncBusCycle();
    cpu_.runCycles(cycles);
    syncBusCycle();
}

void HostAppleIIMachine::keyDown(uint8_t normalizedKey) {
    bus_.keyboard().keyDown(normalizedKey);
}

void HostAppleIIMachine::keyUp(uint8_t normalizedKey) {
    bus_.keyboard().keyUp(normalizedKey);
}

void HostAppleIIMachine::setJoystick(const JoystickState& state) {
    joystick_ = state;
    // Map buttons to PB0/PB1; stick X/Y → paddle 0/1 (0..255 from -32768..32767)
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

void HostAppleIIMachine::setPaddle(const PaddleState& state) {
    paddle_ = state;
    bus_.gameIo().setPaddle(0, static_cast<uint8_t>(state.p0 > 255 ? 255 : state.p0));
    bus_.gameIo().setPaddle(1, static_cast<uint8_t>(state.p1 > 255 ? 255 : state.p1));
    bus_.gameIo().setButton(0, state.button0);
    bus_.gameIo().setButton(1, state.button1);
}

MediaResult HostAppleIIMachine::mountDisk(DriveId id, const char* imageId) {
    return drive(id).mount(imageId);
}

MediaResult HostAppleIIMachine::unmountDisk(DriveId id) {
    return drive(id).unmount();
}

VideoFrameState HostAppleIIMachine::getVideoState() const {
    return toVideoFrameState(videoStateFromSoftSwitches(bus_.softSwitches()),
                             colorMode_);
}

size_t HostAppleIIMachine::consumeAudioEvents(SpeakerEvent* out, size_t max) {
    return bus_.speaker().consumeEvents(out, max);
}

RomError HostAppleIIMachine::loadSyntheticRom() {
    return bus_.rom().loadSynthetic();
}

RomError HostAppleIIMachine::loadRom(const uint8_t* data, size_t size) {
    return bus_.rom().load(data, size);
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

    // ROM
    r.rom = bus_.rom().isLoaded() ? DiagResult::Pass : DiagResult::Fail;

    // RAM poke
    bus_.write(0x0300, 0xA5);
    r.ram = (bus_.read(0x0300) == 0xA5) ? DiagResult::Pass : DiagResult::Fail;

    // Soft switches
    bus_.softSwitches().reset();
    bus_.write(0xC050, 0);
    const bool graphics = bus_.softSwitches().isGraphics();
    bus_.write(0xC051, 0);
    const bool text = bus_.softSwitches().isText();
    r.softswitch = (graphics && text) ? DiagResult::Pass : DiagResult::Fail;

    // Keyboard
    bus_.keyboard().reset();
    bus_.keyboard().keyDown('A');
    const bool strobe = bus_.keyboard().strobePending();
    bus_.keyboard().clearStrobe();
    r.keyboard = (strobe && !bus_.keyboard().strobePending()) ? DiagResult::Pass
                                                              : DiagResult::Fail;

    // Speaker
    bus_.speaker().reset();
    bus_.setAccessCycle(100);
    bus_.read(0xC030);
    bus_.read(0xC030);
    SpeakerEvent ev[4];
    const size_t n = bus_.speaker().consumeEvents(ev, 4);
    r.speaker = (n == 2 && ev[0].level != ev[1].level) ? DiagResult::Pass
                                                       : DiagResult::Fail;

    // Text mapping
    TextDecoder::writeTextScreen(bus_.ram(), 0x0400, "ESP][", 0, 0);
    uint8_t chars[40 * 24];
    TextDecoder::decodeScreen(bus_.ram(), 0x0400, chars);
    r.text = ((chars[0] & 0x7F) == 'E') ? DiagResult::Pass : DiagResult::Fail;

    // LoRes
    LoresDecoder::writeTestPattern(bus_.ram(), 0x0400);
    uint8_t blocks[40 * 48];
    LoresDecoder::decode(bus_.ram(), 0x0400, blocks);
    r.lores = (blocks[0] <= 15) ? DiagResult::Pass : DiagResult::Fail;

    // HGR line address non-linear check: y=0 and y=1 differ by 0x400
    const uint16_t a0 = HgrDecoder::lineAddress(0x2000, 0);
    const uint16_t a1 = HgrDecoder::lineAddress(0x2000, 1);
    r.hgr = (a0 == 0x2000 && a1 == 0x2400) ? DiagResult::Pass : DiagResult::Fail;

    // CPU: step a NOP after synthetic reset if ROM loaded
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
