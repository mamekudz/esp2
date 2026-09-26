#include "esp_bracket/apple2_bus.hpp"
#include "esp_bracket/apple2_machine_host.hpp"
#include "esp_bracket/artifact_renderer.hpp"
#include "esp_bracket/hgr_decoder.hpp"
#include "esp_bracket/lores_decoder.hpp"
#include "esp_bracket/ppm.hpp"
#include "esp_bracket/slot_device.hpp"
#include "esp_bracket/text_decoder.hpp"
#include "esp_bracket/video_state.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace esp_bracket;

static int g_failures = 0;
static int g_passes = 0;

static void expect(bool ok, const char* name) {
    if (!ok) {
        std::fprintf(stderr, "FAIL  %s\n", name);
        ++g_failures;
    } else {
        std::printf("PASS  %s\n", name);
        ++g_passes;
    }
}

static void testKeyboardMirrors() {
    Apple2Bus bus;
    bus.keyboard().keyDown('A');
    for (uint16_t a = 0xC000; a <= 0xC00F; ++a) {
        expect(bus.read(a) == static_cast<uint8_t>('A' | 0x80), "kbd mirror read");
    }
    // write to data range must not clear strobe
    bus.write(0xC005, 0x00);
    expect(bus.keyboard().strobePending(), "kbd write no clear");
    bus.read(0xC01A);
    expect(!bus.keyboard().strobePending(), "strobe clear mid-mirror");
    expect((bus.keyboard().ascii7()) == 'A', "strobe clear keeps ascii");
    bus.keyboard().keyDown('B');
    bus.write(0xC01F, 0xFF);
    expect(!bus.keyboard().strobePending(), "strobe clear via write");
    // neighbor outside range
    const size_t before = bus.speaker().pendingCount();
    bus.read(0xC020); // cassette — not strobe
    expect(bus.keyboard().ascii7() == 'B', "cassette does not clear kbd ascii");
    (void)before;
}

static void testSpeakerMirrors() {
    Apple2Bus bus;
    bus.setAccessCycle(1000);
    for (uint16_t a = 0xC030; a <= 0xC03F; ++a) {
        bus.speaker().reset();
        bus.setAccessCycle(1000 + a);
        bus.read(a);
        SpeakerEvent ev[2];
        expect(bus.speaker().consumeEvents(ev, 2) == 1, "speaker one event per access");
        expect(ev[0].cycle == 1000 + a, "speaker cycle stamp");
    }
    bus.speaker().reset();
    bus.write(0xC03F, 0);
    bus.write(0xC03F, 0);
    SpeakerEvent ev[4];
    expect(bus.speaker().consumeEvents(ev, 4) == 2, "speaker write toggles");
    // neighbor before/after
    bus.speaker().reset();
    bus.read(0xC02F); // cassette
    bus.read(0xC040); // utility
    expect(bus.speaker().pendingCount() == 0, "neighbors do not toggle speaker");
}

static void testPeekNoSideEffects() {
    Apple2Bus bus;
    bus.keyboard().keyDown('Z');
    bus.setAccessCycle(50);
    (void)bus.peek(0xC030);
    (void)bus.peek(0xC010);
    (void)bus.peek(0xC070);
    expect(bus.speaker().pendingCount() == 0, "peek no speaker");
    expect(bus.keyboard().strobePending(), "peek no strobe clear");
    expect(bus.gameIo().readPaddleSense(0, 50) == 0x00, "peek no paddle trigger");
}

static void testVideoSwitchMatrix() {
    struct Row {
        uint16_t offAddr;
        uint16_t onAddr;
        bool (*isOn)(const SoftSwitches&);
    };
    SoftSwitches& (*sw)(Apple2Bus&) = [](Apple2Bus& b) -> SoftSwitches& {
        return b.softSwitches();
    };
    (void)sw;

    Apple2Bus bus;
    // TEXT default on
    expect(bus.softSwitches().isText(), "video reset TEXT");
    bus.read(0xC050);
    expect(bus.softSwitches().isGraphics(), "GRAPHICS via read");
    bus.write(0xC051, 0);
    expect(bus.softSwitches().isText(), "TEXT via write");
    bus.read(0xC053);
    expect(bus.softSwitches().isMixed(), "MIXED");
    bus.read(0xC052);
    expect(!bus.softSwitches().isMixed(), "FULL");
    bus.read(0xC055);
    expect(bus.softSwitches().isPage2(), "PAGE2");
    bus.read(0xC054);
    expect(!bus.softSwitches().isPage2(), "PAGE1");
    bus.read(0xC057);
    expect(bus.softSwitches().isHires(), "HIRES");
    bus.read(0xC056);
    expect(bus.softSwitches().isLores(), "LORES");
    // neighbor $C058 is annunciator, not video
    bus.softSwitches().setText(true);
    bus.read(0xC058);
    expect(bus.softSwitches().isText(), "AN0 does not clear TEXT");
}

static void testVideoMemorySurvivesPageSwitch() {
    Apple2Bus bus;
    TextDecoder::writeTextScreen(bus.ram(), 0x0400, "PAGE1");
    TextDecoder::writeTextScreen(bus.ram(), 0x0800, "PAGE2");
    bus.read(0xC055); // page2
    uint8_t c1[40 * 24];
    uint8_t c2[40 * 24];
    TextDecoder::decodeScreen(bus.ram(), 0x0400, c1);
    TextDecoder::decodeScreen(bus.ram(), 0x0800, c2);
    expect((c1[0] & 0x7F) == 'P', "page1 ram intact");
    expect((c2[0] & 0x7F) == 'P', "page2 ram intact");
    bus.read(0xC054);
    TextDecoder::decodeScreen(bus.ram(), 0x0800, c2);
    expect((c2[0] & 0x7F) == 'P', "page2 survives PAGE1 select");
}

static void testAnnunciatorsCassetteButtons() {
    Apple2Bus bus;
    bus.read(0xC059); // AN0 on
    expect(bus.gameIo().annunciator(0), "AN0 on");
    bus.read(0xC058);
    expect(!bus.gameIo().annunciator(0), "AN0 off");
    bus.read(0xC05F);
    expect(bus.gameIo().annunciator(3), "AN3 on");

    bus.gameIo().setCassetteIn(true);
    expect((bus.read(0xC060) & 0x80) != 0, "cassette in");
    expect((bus.read(0xC068) & 0x80) != 0, "cassette in mirror");

    bus.gameIo().setButton(0, true);
    expect((bus.read(0xC061) & 0x80) != 0, "PB0");
    expect((bus.read(0xC069) & 0x80) != 0, "PB0 mirror");

    const bool before = bus.gameIo().cassetteOut();
    bus.read(0xC020);
    expect(bus.gameIo().cassetteOut() != before, "cassette out toggle");
    bus.read(0xC040);
    expect(bus.gameIo().utilityStrobeCount() == 1, "utility strobe");
}

static void testPaddles() {
    Apple2Bus bus;
    bus.gameIo().setPaddle(0, 1); // near-minimum (0 expires immediately)
    bus.gameIo().setPaddle(1, 128);
    bus.gameIo().setPaddle(2, 255);
    bus.gameIo().setPaddle(3, 64);
    bus.setAccessCycle(0);
    bus.read(0xC070); // trigger
    bus.setAccessCycle(0);
    expect((bus.read(0xC064) & 0x80) != 0, "pdl0 active at t0");
    bus.setAccessCycle(1 * 11 + 1);
    expect((bus.read(0xC064) & 0x80) == 0, "pdl0 min expired");
    bus.setAccessCycle(0);
    bus.write(0xC07F, 0); // retrigger via mirror write
    bus.setAccessCycle(128 * 11 / 2);
    expect((bus.read(0xC065) & 0x80) != 0, "pdl1 center still active");
    bus.setAccessCycle(128 * 11 + 1);
    expect((bus.read(0xC065) & 0x80) == 0, "pdl1 center done");
    bus.setAccessCycle(0);
    bus.read(0xC070);
    bus.setAccessCycle(255 * 11 - 1);
    expect((bus.read(0xC066) & 0x80) != 0, "pdl2 max still at threshold");
    bus.setAccessCycle(255 * 11);
    expect((bus.read(0xC066) & 0x80) == 0, "pdl2 max done");
    // independent channel
    bus.setAccessCycle(0);
    bus.read(0xC070);
    bus.setAccessCycle(64 * 11 + 1);
    expect((bus.read(0xC067) & 0x80) == 0, "pdl3 independent");
    expect((bus.read(0xC066) & 0x80) != 0, "pdl2 still running");
}

static void testSlotDispatch() {
    Apple2Bus bus;
    SpySlotDevice spy;
    uint8_t rom[256];
    std::memset(rom, 0xEA, sizeof(rom));
    rom[0] = 0x6D; // marker
    spy.setRom(rom, sizeof(rom));
    bus.setSlotDevice(6, &spy);

    bus.setAccessCycle(99);
    const uint8_t v = bus.read(0xC0E0);
    expect(spy.readCount() == 1, "slot6 io read routed");
    expect(spy.lastOffset() == 0x00, "slot6 offset 0");
    expect(v == 0xA5, "slot6 read value");

    bus.write(0xC0EF, 0x42);
    expect(spy.writeCount() == 1, "slot6 io write");
    expect(spy.lastOffset() == 0x0F, "slot6 offset F");
    expect(spy.lastWriteValue() == 0x42, "slot6 write value");

    // Before/after range
    SpySlotDevice spy5;
    bus.setSlotDevice(5, &spy5);
    bus.read(0xC0DF);
    expect(spy5.readCount() == 1, "slot5 last");
    bus.read(0xC0F0);
    // slot7 empty → floating
    expect(bus.read(0xC0F0) == bus.floatingBusApprox(), "slot7 empty");

    expect(bus.read(0xC600) == 0x6D, "slot6 rom window");
    expect(spy.romReadCount() == 1, "slot rom read");
    expect(bus.expansionRomSlot() == 6, "C800 select latch");

    const int latchBefore = bus.expansionRomSlot();
    (void)bus.peek(0xC601);
    expect(bus.expansionRomSlot() == latchBefore, "peek does not change C800 latch");
}

static void testIoFuzzNoCrash() {
    Apple2Bus bus;
    SpySlotDevice spies[8];
    for (int s = 0; s < 8; ++s) {
        bus.setSlotDevice(s, &spies[s]);
    }
    bus.keyboard().keyDown('X');
    bus.setAccessCycle(0);
    for (uint16_t a = 0xC000; a <= 0xC0FF; ++a) {
        bus.setAccessCycle(a);
        (void)bus.read(a);
        bus.write(a, static_cast<uint8_t>(a & 0xFF));
        (void)bus.peek(a);
    }
    expect(true, "fuzz C000-C0FF read/write/peek");
}

static void testIoTraceOptional() {
    Apple2Bus bus;
    static int hits = 0;
    hits = 0;
    bus.setIoTrace(
        [](void*, const IoTraceEvent& ev) {
            if (ev.tag && std::strcmp(ev.tag, "speaker-toggle") == 0) {
                ++hits;
            }
        },
        nullptr);
    bus.read(0xC030);
    bus.clearIoTrace();
    bus.read(0xC030);
    expect(hits == 1, "trace only when enabled");
}

static void testRangeNeighbors() {
    Apple2Bus bus;
    // speaker range neighbors already covered; video:
    bus.softSwitches().reset();
    bus.read(0xC04F); // utility
    expect(bus.softSwitches().isText(), "before C050 intact");
    bus.read(0xC050);
    expect(bus.softSwitches().isGraphics(), "first video");
    bus.read(0xC057);
    expect(bus.softSwitches().isHires(), "last video");
    bus.softSwitches().setHires(false);
    bus.read(0xC058);
    expect(!bus.softSwitches().isHires(), "after C057 not video hires");
}

static void measureIoDecode() {
    Apple2Bus bus;
    bus.keyboard().keyDown('Q');
    const auto t0 = std::chrono::steady_clock::now();
    for (int n = 0; n < 10000; ++n) {
        for (uint16_t a = 0xC000; a <= 0xC0FF; ++a) {
            (void)bus.peek(a); // no side-effect flood
        }
    }
    const auto t1 = std::chrono::steady_clock::now();
    std::printf(
        "HOST MEASUREMENT  io_peek_page_x10000_ms=%.3f\n",
        std::chrono::duration<double, std::milli>(t1 - t0).count());
}

static void regenerateVisuals(const char* outDir) {
    HostAppleIIMachine m;
    m.loadSyntheticRom();
    uint8_t* ram = m.bus().ram();
    TextDecoder::writeTextScreen(ram, 0x0400, "ESP][ IO AUDIT\nTEXT OK");
    uint8_t chars[40 * 24];
    TextDecoder::decodeScreen(ram, 0x0400, chars);
    std::vector<uint8_t> rgb(static_cast<size_t>(TextDecoder::kRgbW * TextDecoder::kRgbH * 3));
    TextDecoder::renderRgb888(chars, rgb.data(), rgb.size());
    std::string p = std::string(outDir) + "/host_text_test.ppm";
    expect(Ppm::writeP6(p.c_str(), TextDecoder::kRgbW, TextDecoder::kRgbH, rgb.data(),
                        rgb.size()),
           "visual text ppm");

    LoresDecoder::writeTestPattern(ram, 0x0400);
    uint8_t blocks[40 * 48];
    LoresDecoder::decode(ram, 0x0400, blocks);
    std::vector<uint8_t> lrgb(static_cast<size_t>(LoresDecoder::kRgbW * LoresDecoder::kRgbH * 3));
    LoresDecoder::renderRgb888(blocks, lrgb.data(), lrgb.size());
    p = std::string(outDir) + "/host_lores_test.ppm";
    expect(Ppm::writeP6(p.c_str(), LoresDecoder::kRgbW, LoresDecoder::kRgbH, lrgb.data(),
                        lrgb.size()),
           "visual lores ppm");

    HgrDecoder::writePattern(ram, 0x2000, "alt1010");
    uint8_t bits[280 * 192];
    uint8_t hb[40 * 192];
    HgrDecoder::decode(ram, 0x2000, bits, hb);
    std::vector<uint8_t> hrgb(280 * 192 * 3);
    ArtifactRenderer::render(bits, hb, VideoColorMode::CompositeColor, hrgb.data(),
                             hrgb.size());
    p = std::string(outDir) + "/host_hgr_artifact.ppm";
    expect(Ppm::writeP6(p.c_str(), 280, 192, hrgb.data(), hrgb.size()), "visual hgr ppm");
}

int main(int argc, char** argv) {
    const char* outDir = "host/.out";
    if (argc > 1) {
        outDir = argv[1];
    }

    testKeyboardMirrors();
    testSpeakerMirrors();
    testPeekNoSideEffects();
    testVideoSwitchMatrix();
    testVideoMemorySurvivesPageSwitch();
    testAnnunciatorsCassetteButtons();
    testPaddles();
    testSlotDispatch();
    testIoFuzzNoCrash();
    testIoTraceOptional();
    testRangeNeighbors();
    regenerateVisuals(outDir);
    measureIoDecode();

    std::printf("\nio_page passes=%d failures=%d\n", g_passes, g_failures);
    if (g_failures) {
        return 1;
    }
    std::printf("All I/O page host tests passed\n");
    return 0;
}
