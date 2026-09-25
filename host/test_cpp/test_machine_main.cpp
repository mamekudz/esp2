#include "esp_bracket/apple2_machine_host.hpp"
#include "esp_bracket/artifact_renderer.hpp"
#include "esp_bracket/display_effect.hpp"
#include "esp_bracket/hgr_decoder.hpp"
#include "esp_bracket/lores_decoder.hpp"
#include "esp_bracket/ppm.hpp"
#include "esp_bracket/soft_switches.hpp"
#include "esp_bracket/text_decoder.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace esp_bracket;

static int g_failures = 0;

static void expect(bool ok, const char* name) {
    if (!ok) {
        std::fprintf(stderr, "FAIL  %s\n", name);
        ++g_failures;
    } else {
        std::printf("PASS  %s\n", name);
    }
}

static void testMemoryRomBus() {
    Apple2Bus bus;
    bus.rom().loadSynthetic();
    bus.write(0x1234, 0x5A);
    expect(bus.read(0x1234) == 0x5A, "RAM read/write");
    const uint8_t before = bus.read(0xF800);
    bus.write(0xF800, 0x00);
    expect(bus.read(0xF800) == before, "ROM write-protect");
    expect(bus.read(0xFFFC) != 0xFF || bus.read(0xFFFD) != 0xFF, "RESET vector present");
}

static void testSoftSwitches() {
    SoftSwitches sw;
    sw.access(0xC050);
    expect(sw.isGraphics(), "GRAPHICS");
    sw.access(0xC051);
    expect(sw.isText(), "TEXT");
    sw.access(0xC053);
    expect(sw.isMixed(), "MIXED");
    sw.access(0xC055);
    expect(sw.isPage2(), "PAGE2");
    sw.access(0xC057);
    expect(sw.isHires(), "HIRES");
    sw.access(0xC056);
    expect(sw.isLores(), "LORES");
    sw.access(0xC054);
    expect(!sw.isPage2(), "PAGE1");
}

static void testKeyboardSpeaker() {
    Apple2Bus bus;
    bus.keyboard().keyDown('Z');
    expect(bus.read(0xC000) == static_cast<uint8_t>('Z' | 0x80), "kbd latch");
    bus.read(0xC010);
    expect(!bus.keyboard().strobePending(), "kbd strobe clear");
    bus.setAccessCycle(42);
    bus.read(0xC030);
    bus.read(0xC030);
    SpeakerEvent ev[4];
    const size_t n = bus.speaker().consumeEvents(ev, 4);
    expect(n == 2, "speaker events count");
    expect(ev[0].cycle == 42 && ev[1].cycle == 42, "speaker cycles");
    expect(ev[0].level != ev[1].level, "speaker toggle");
}

static void testTextLayout() {
    uint8_t ram[0xC000]{};
    TextDecoder::writeTextScreen(ram, 0x0400,
                                 "ESP][ HOST TEST\nCPU      OK\nRAM      OK\nROM      OK\nVIDEO    OK");
    expect(TextDecoder::cellAddress(0x0400, 0, 0) == 0x0400, "text addr r0c0");
    expect(TextDecoder::cellAddress(0x0400, 1, 0) == 0x0480, "text addr r1c0");
    expect(TextDecoder::cellAddress(0x0400, 8, 0) == 0x0428, "text addr r8c0");
    uint8_t chars[40 * 24];
    TextDecoder::decodeScreen(ram, 0x0400, chars);
    expect((chars[0] & 0x7F) == 'E', "text char E");
}

static void testLoresHgrPages() {
    uint8_t ram[0xC000]{};
    LoresDecoder::writeTestPattern(ram, 0x0400);
    uint8_t b1[40 * 48];
    LoresDecoder::decode(ram, 0x0400, b1);
    expect(b1[0] == 0, "lores nibble0");

    HgrDecoder::writePattern(ram, 0x2000, "vline");
    HgrDecoder::writePattern(ram, 0x4000, "hline");
    uint8_t bits[280 * 192];
    uint8_t hb[40 * 192];
    HgrDecoder::decode(ram, 0x2000, bits, hb);
    expect(bits[96 * 280 + 140] == 1, "hgr page1 vline");
    HgrDecoder::decode(ram, 0x4000, bits, hb);
    expect(bits[96 * 280 + 0] == 1, "hgr page2 hline");
}

static void testMixedModeBoundary() {
    SoftSwitches sw;
    sw.access(0xC050); // graphics
    sw.access(0xC053); // mixed
    sw.access(0xC057); // hires
    AppleIIVideoState vs = videoStateFromSoftSwitches(sw);
    expect(!vs.text && vs.mixed && vs.hires, "mixed hgr state");
}

static void testMachineResetKeepsDisks() {
    HostAppleIIMachine m;
    m.loadSyntheticRom();
    expect(m.mountDisk(DriveId::Drive1, "test-disk") == MediaResult::Ok, "mount");
    m.reset();
    expect(m.drive(DriveId::Drive1).state().inserted, "disk stays after Apple reset");
}

static void testSelfTest() {
    HostAppleIIMachine m;
    m.loadSyntheticRom();
    DiagReport d = m.runSelfTest();
    expect(d.cpu == DiagResult::Pass, "diag cpu");
    expect(d.ram == DiagResult::Pass, "diag ram");
    expect(d.rom == DiagResult::Pass, "diag rom");
    expect(d.softswitch == DiagResult::Pass, "diag softswitch");
    expect(d.keyboard == DiagResult::Pass, "diag keyboard");
    expect(d.speaker == DiagResult::Pass, "diag speaker");
    expect(d.text == DiagResult::Pass, "diag text");
    expect(d.lores == DiagResult::Pass, "diag lores");
    expect(d.hgr == DiagResult::Pass, "diag hgr");
}

static void writeVisuals(const char* outDir) {
    HostAppleIIMachine m;
    m.loadSyntheticRom();
    uint8_t* ram = m.bus().ram();

    TextDecoder::writeTextScreen(
        ram, 0x0400,
        "ESP][ HOST TEST\n\n6502       OK\nRAM        OK\nROM        OK\n"
        "SOFTSWITCH OK\nKEYBOARD   OK\nSPEAKER    OK\nTEXT       OK\n"
        "LORES      OK\nHGR        OK");
    uint8_t chars[40 * 24];
    TextDecoder::decodeScreen(ram, 0x0400, chars);
    std::vector<uint8_t> rgb(static_cast<size_t>(TextDecoder::kRgbW * TextDecoder::kRgbH * 3));
    TextDecoder::renderRgb888(chars, rgb.data(), rgb.size());
    std::string textPath = std::string(outDir) + "/host_text_test.ppm";
    expect(Ppm::writeP6(textPath.c_str(), TextDecoder::kRgbW, TextDecoder::kRgbH, rgb.data(),
                        rgb.size()),
           "write text ppm");

    LoresDecoder::writeTestPattern(ram, 0x0400);
    uint8_t blocks[40 * 48];
    LoresDecoder::decode(ram, 0x0400, blocks);
    std::vector<uint8_t> lrgb(static_cast<size_t>(LoresDecoder::kRgbW * LoresDecoder::kRgbH * 3));
    LoresDecoder::renderRgb888(blocks, lrgb.data(), lrgb.size());
    std::string loresPath = std::string(outDir) + "/host_lores_test.ppm";
    expect(Ppm::writeP6(loresPath.c_str(), LoresDecoder::kRgbW, LoresDecoder::kRgbH,
                        lrgb.data(), lrgb.size()),
           "write lores ppm");

    HgrDecoder::writePattern(ram, 0x2000, "alt1010");
    uint8_t bits[280 * 192];
    uint8_t hb[40 * 192];
    HgrDecoder::decode(ram, 0x2000, bits, hb);
    std::vector<uint8_t> hrgb(280 * 192 * 3);
    ArtifactRenderer::render(bits, hb, VideoColorMode::CompositeColor, hrgb.data(),
                             hrgb.size());
    DisplayEffect::apply(hrgb.data(), 280, 192, hrgb.size(), DisplayEffectMode::Sharp,
                         EffectStrength::Off);
    std::string hgrPath = std::string(outDir) + "/host_hgr_artifact.ppm";
    expect(Ppm::writeP6(hgrPath.c_str(), 280, 192, hrgb.data(), hrgb.size()),
           "write hgr ppm");

    // Mono green from luminance bits
    ArtifactRenderer::render(bits, hb, VideoColorMode::MonochromeGreen, hrgb.data(),
                             hrgb.size());
    std::string monoPath = std::string(outDir) + "/host_hgr_mono_green.ppm";
    expect(Ppm::writeP6(monoPath.c_str(), 280, 192, hrgb.data(), hrgb.size()),
           "write mono ppm");
}

static void measureHostPerf() {
    HostAppleIIMachine m;
    m.loadSyntheticRom();
    m.reset();
    const auto t0 = std::chrono::steady_clock::now();
    m.runCycles(1'000'000);
    const auto t1 = std::chrono::steady_clock::now();
    const double ms =
        std::chrono::duration<double, std::milli>(t1 - t0).count();
    std::printf("HOST MEASUREMENT  cpu_1e6_cycles_ms=%.3f\n", ms);

    uint8_t ram[0xC000]{};
    HgrDecoder::writePattern(ram, 0x2000, "checker");
    uint8_t bits[280 * 192];
    uint8_t hb[40 * 192];
    const auto d0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 50; ++i) {
        HgrDecoder::decode(ram, 0x2000, bits, hb);
    }
    const auto d1 = std::chrono::steady_clock::now();
    std::printf("HOST MEASUREMENT  hgr_decode_50_ms=%.3f\n",
                std::chrono::duration<double, std::milli>(d1 - d0).count());

    std::vector<uint8_t> rgb(280 * 192 * 3);
    const auto a0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 50; ++i) {
        ArtifactRenderer::render(bits, hb, VideoColorMode::CompositeColor, rgb.data(),
                                 rgb.size());
    }
    const auto a1 = std::chrono::steady_clock::now();
    std::printf("HOST MEASUREMENT  artifact_50_ms=%.3f\n",
                std::chrono::duration<double, std::milli>(a1 - a0).count());
}

static void testGoldenChecksums(const char* goldenDir) {
    // Lightweight golden: text cell address table + first HGR line address list
    uint16_t addrs[24];
    for (int r = 0; r < 24; ++r) {
        addrs[r] = TextDecoder::cellAddress(0x0400, r, 0);
    }
    expect(addrs[0] == 0x0400 && addrs[8] == 0x0428 && addrs[16] == 0x0450,
           "golden text row bases");
    expect(HgrDecoder::lineAddress(0x2000, 0) == 0x2000, "golden hgr y0");
    expect(HgrDecoder::lineAddress(0x2000, 64) == 0x2028, "golden hgr y64");
    expect(HgrDecoder::lineAddress(0x2000, 128) == 0x2050, "golden hgr y128");
    (void)goldenDir;
}

int main(int argc, char** argv) {
    const char* outDir = "host/.out";
    if (argc > 1) {
        outDir = argv[1];
    }

    testMemoryRomBus();
    testSoftSwitches();
    testKeyboardSpeaker();
    testTextLayout();
    testLoresHgrPages();
    testMixedModeBoundary();
    testMachineResetKeepsDisks();
    testSelfTest();
    testGoldenChecksums("fixtures/golden/apple2");
    writeVisuals(outDir);
    measureHostPerf();

    if (g_failures) {
        std::fprintf(stderr, "\n%d machine/video test(s) failed\n", g_failures);
        return 1;
    }
    std::printf("\nAll machine/video host tests passed\n");
    return 0;
}
