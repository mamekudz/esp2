#include "esp_bracket/apple2_bus.hpp"
#include "esp_bracket/artifact_renderer.hpp"
#include "esp_bracket/cpu6502.hpp"
#include "esp_bracket/display_effect.hpp"
#include "esp_bracket/hgr_decoder.hpp"
#include "esp_bracket/lores_decoder.hpp"
#include "esp_bracket/phosphor.hpp"
#include "esp_bracket/rom.hpp"
#include "esp_bracket/soft_switches.hpp"
#include "esp_bracket/text_decoder.hpp"
#include "esp_bracket/video_state.hpp"

#include <cstdio>
#include <cstring>

using namespace esp_bracket;

static int g_failures = 0;

static void check(bool ok, const char *name) {
    if (!ok) {
        std::fprintf(stderr, "FAIL  %s\n", name);
        ++g_failures;
    } else {
        std::printf("PASS  %s\n", name);
    }
}

static void testTextLayout() {
    uint8_t ram[0x10000]{};
    check(TextDecoder::cellAddress(0x0400, 0, 0) == 0x0400, "text addr r0c0");
    check(TextDecoder::cellAddress(0x0400, 0, 39) == 0x0427, "text addr r0c39");
    check(TextDecoder::cellAddress(0x0400, 1, 0) == 0x0480, "text addr r1c0");
    check(TextDecoder::cellAddress(0x0400, 8, 0) == 0x0428, "text addr r8c0");
    check(TextDecoder::cellAddress(0x0800, 0, 0) == 0x0800, "text page2 base");

    TextDecoder::writeTextScreen(ram, TextDecoder::kPage1Base, "ESP][", 0, 0);
    check(ram[0x0400] == static_cast<uint8_t>(0x80 | 'E'), "text write E");
    check(ram[0x0404] == static_cast<uint8_t>(0x80 | '['), "text write [");

    uint8_t chars[TextDecoder::kRows * TextDecoder::kCols];
    TextDecoder::decodeScreen(ram, TextDecoder::kPage1Base, chars);
    check((chars[0] & 0x7F) == 'E', "text decode E");
    check((chars[0] & 0x80) != 0, "text normal high bit");

    ram[0x0400] = 'A'; // inverse: high bit clear
    TextDecoder::decodeScreen(ram, TextDecoder::kPage1Base, chars);
    check((chars[0] & 0x80) == 0, "text inverse flag");
}

static void testLoresColors() {
    uint8_t ram[0x10000]{};
    ram[0x0400] = 0x1F; // low=15 white (top), high=1 magenta (bottom)
    uint8_t nib[LoresDecoder::kRows * LoresDecoder::kCols];
    LoresDecoder::decode(ram, 0x0400, nib);
    check(nib[0] == 15, "lores top nibble white");
    check(nib[LoresDecoder::kCols] == 1, "lores bottom nibble magenta");

    uint8_t r = 1, g = 1, b = 1;
    LoresDecoder::colorRgb(0, &r, &g, &b);
    check(r == 0 && g == 0 && b == 0, "lores black");
    LoresDecoder::colorRgb(15, &r, &g, &b);
    check(r == 255 && g == 255 && b == 255, "lores white");
    LoresDecoder::colorRgb(9, &r, &g, &b);
    check(r > 200, "lores orange present");

    uint8_t rgb[LoresDecoder::kRgbW * LoresDecoder::kRgbH * 3];
    LoresDecoder::renderRgb888(nib, rgb, sizeof(rgb));
    check(rgb[0] == 255 && rgb[1] == 255 && rgb[2] == 255, "lores render white pixel");
}

static void testHgrMapping() {
    check(HgrDecoder::lineAddress(0x2000, 0) == 0x2000, "hgr y0");
    check(HgrDecoder::lineAddress(0x2000, 1) == 0x2400, "hgr y1");
    check(HgrDecoder::lineAddress(0x2000, 8) == 0x2080, "hgr y8");
    check(HgrDecoder::lineAddress(0x2000, 64) == 0x2028, "hgr y64");
    check(HgrDecoder::lineAddress(HgrDecoder::kPage2Base, 0) == 0x4000, "hgr page2");

    uint8_t ram[0x10000]{};
    ram[0x2000] = static_cast<uint8_t>(0x80 | 0x55);

    uint8_t bits[HgrDecoder::kWidth * HgrDecoder::kHeight];
    uint8_t high[40 * HgrDecoder::kHeight];
    HgrDecoder::decode(ram, HgrDecoder::kPage1Base, bits, high);

    check(high[0] == 1, "hgr highbit phase");
    check(bits[0] == 1 && bits[1] == 0 && bits[2] == 1, "hgr bit pattern 101");
}

static void testArtifact() {
    using AR = ArtifactRenderer;
    const AR::Rgb black = AR::compositePair(false, false, false);
    const AR::Rgb white = AR::compositePair(true, true, false);
    const AR::Rgb purple = AR::compositePair(true, false, false);
    const AR::Rgb green = AR::compositePair(false, true, false);
    const AR::Rgb blue = AR::compositePair(true, false, true);
    const AR::Rgb orange = AR::compositePair(false, true, true);

    check(black.r == 0 && black.g == 0 && black.b == 0, "art black");
    check(white.r == 255 && white.g == 255 && white.b == 255, "art white");
    check(purple.r > purple.g && purple.b > purple.g, "art purple");
    check(green.g > green.r && green.g > green.b, "art green");
    check(blue.b > blue.r && blue.b > blue.g, "art blue");
    check(orange.r > orange.b && orange.g > 0, "art orange");

    uint8_t bits[AR::kWidth * AR::kHeight]{};
    uint8_t high[40 * AR::kHeight]{};
    bits[0] = 1;
    bits[1] = 0;
    uint8_t rgb[AR::kWidth * AR::kHeight * 3];
    AR::render(bits, high, VideoColorMode::CompositeColor, rgb, sizeof(rgb));
    check(rgb[0] == purple.r && rgb[1] == purple.g && rgb[2] == purple.b, "art composite purple");

    AR::render(bits, high, VideoColorMode::MonochromeWhite, rgb, sizeof(rgb));
    check(rgb[0] == 255 && rgb[1] == 255 && rgb[2] == 255, "art mono white on");
    check(rgb[3] == 0 && rgb[4] == 0 && rgb[5] == 0, "art mono black off");

    AR::render(bits, high, VideoColorMode::MonochromeGreen, rgb, sizeof(rgb));
    check(rgb[1] > rgb[0] && rgb[1] > rgb[2], "art mono green channel");

    AR::render(bits, high, VideoColorMode::MonochromeAmber, rgb, sizeof(rgb));
    check(rgb[0] > rgb[2], "art mono amber red>blue");
}

static void testArtifactRgb565Equivalence() {
    using AR = ArtifactRenderer;
    uint8_t ram[0x10000]{};
    HgrDecoder::writePattern(ram, 0x2000, "artifact_ref");
    uint8_t bits[AR::kWidth * AR::kHeight];
    uint8_t high[40 * AR::kHeight];
    HgrDecoder::decode(ram, 0x2000, bits, high);

    uint8_t rgb888[AR::kWidth * AR::kHeight * 3];
    AR::render(bits, high, VideoColorMode::CompositeColor, rgb888, sizeof(rgb888));

    uint16_t line565[AR::kWidth];
    int mismatches = 0;
    for (int y = 0; y < AR::kHeight; ++y) {
        AR::renderScanlineRgb565(bits + y * AR::kWidth, high + y * 40,
                                 VideoColorMode::CompositeColor, line565);
        for (int x = 0; x < AR::kWidth; ++x) {
            const uint8_t *p = rgb888 + (static_cast<size_t>(y) * AR::kWidth + x) * 3;
            const uint16_t expect = AR::toRgb565({p[0], p[1], p[2]});
            if (line565[x] != expect) {
                ++mismatches;
            }
        }
    }
    check(mismatches == 0, "art rgb565 scanline == rgb888+quantize");

    // Byte-boundary pair (pixels 6|7): last of byte0 + first of byte1.
    HgrDecoder::writePattern(ram, 0x2000, "byte_boundary");
    HgrDecoder::decode(ram, 0x2000, bits, high);
    AR::renderScanlineRgb565(bits, high, VideoColorMode::CompositeColor, line565);
    const AR::Rgb whitePair = AR::compositePair(true, true, false); // both on → white
    // 0x40|0x01 → bits 6 and 7 both set → white pair
    check(line565[6] == AR::toRgb565(whitePair) && line565[7] == AR::toRgb565(whitePair),
          "art byte-boundary pair white");

    // Phase from high bit of the byte containing the pair's first pixel (bx=0).
    std::memset(bits, 0, sizeof(bits));
    std::memset(high, 0, sizeof(high));
    bits[6] = 1;
    bits[7] = 0;
    high[0] = 0;
    AR::renderScanlineRgb565(bits, high, VideoColorMode::CompositeColor, line565);
    check(line565[6] == AR::toRgb565(AR::compositePair(true, false, false)),
          "art phase high-clear purple across boundary");
    high[0] = 1;
    AR::renderScanlineRgb565(bits, high, VideoColorMode::CompositeColor, line565);
    check(line565[6] == AR::toRgb565(AR::compositePair(true, false, true)),
          "art phase high-set blue across boundary");
}

static void testHgrClearExactCycles() {
    // Diagnose PART C ~8e6 figure: measure exact cycles to clear one 8 KiB page.
    uint8_t romImg[Rom::kApple2PlusRomBytes];
    check(generateSyntheticVideoPipelineRom(romImg, sizeof(romImg)) == RomError::Ok,
          "hgr clear rom gen");

    Apple2Bus bus;
    check(bus.rom().load(romImg, sizeof(romImg)) == RomError::Ok, "hgr clear rom load");
    Cpu6502 cpu;
    cpu.setCallbacks(&bus, Apple2Bus::busRead, Apple2Bus::busWrite);
    cpu.reset();

    std::memset(bus.ram() + 0x2000, 0xA5, 0x2000);

    CpuRegisters r = cpu.registers();
    r.pc = kEsp32HgrClearRoutine;
    cpu.setRegisters(r);
    const uint64_t c0 = cpu.cycles();

    // Done address: self-JMP after clear body (see synthetic_rom.cpp).
    const uint16_t donePc = static_cast<uint16_t>(kEsp32HgrClearRoutine + 0x19);
    bool parked = false;
    for (int i = 0; i < 200000; ++i) {
        cpu.runCycles(64);
        if (cpu.registers().pc == donePc) {
            parked = true;
            break;
        }
    }
    const uint64_t used = cpu.cycles() - c0;
    check(parked, "hgr clear parked at done JMP");
    const uint8_t b0 = bus.ram()[0x2000];
    const uint8_t b1 = bus.ram()[0x3FFF];
    const uint8_t z0 = bus.ram()[0];
    const uint8_t z1 = bus.ram()[1];
    int nonzero = 0;
    int firstNz = -1;
    for (int a = 0x2000; a < 0x4000; ++a) {
        if (bus.ram()[a] != 0) {
            ++nonzero;
            if (firstNz < 0) {
                firstNz = a;
            }
        }
    }
    std::printf("HOST DIAG  hgr_clear pc=$%04X cycles=%llu ram2000=$%02X ram3fff=$%02X "
                "zp=$%02X%02X nonzero=%d first_nz=$%04X peekE800=$%02X\n",
                cpu.registers().pc, static_cast<unsigned long long>(used), b0, b1, z1, z0, nonzero,
                firstNz, bus.peek(0xE800));
    check(b0 == 0 && b1 == 0 && nonzero == 0, "hgr clear page zeroed");
    // Expected ~90k cycles (8192 STA ind,Y + loops) — not millions.
    check(used > 50000 && used < 200000, "hgr clear cycle budget ~90k not 8e6");
    std::printf("HOST MEASUREMENT  hgr_clear_exact_cycles=%llu (done_pc=$%04X)\n",
                static_cast<unsigned long long>(used), donePc);
    // PART C firmware logged ~8e6 because that was the *timeout* when the done
    // detector used a loose PC window and continued executing JMP * idle until
    // the 8e6 budget — not because the clear loop itself costs 8e6 cycles.
    check(used < 8000000ull, "hgr clear not timeout-scale");
}

static void testPageAndMixed() {
    SoftSwitches sw;
    sw.reset();
    check(sw.isText() && !sw.isPage2() && !sw.isHires() && !sw.isMixed(), "sw default");

    sw.access(SoftSwitches::kAddrGraphics);
    sw.access(SoftSwitches::kAddrHires);
    sw.access(SoftSwitches::kAddrPage2);
    sw.access(SoftSwitches::kAddrMixed);
    check(!sw.isText() && sw.isHires() && sw.isPage2() && sw.isMixed(), "sw graphics mixed p2");

    AppleIIVideoState vs = videoStateFromSoftSwitches(sw);
    check(vs.hgrPageBase() == 0x4000, "vs hgr page2 base");
    check(vs.textPageBase() == 0x0800, "vs text page2 base");
    check(vs.mixed, "vs mixed");

    VideoFrameState frame = toVideoFrameState(vs, VideoColorMode::CompositeColor);
    check(frame.page2 && frame.mixed && frame.hires, "frame flags");
    check(frame.textPage == 2, "frame textPage");

    sw.access(SoftSwitches::kAddrText);
    vs = videoStateFromSoftSwitches(sw);
    frame = toVideoFrameState(vs, VideoColorMode::MonochromeGreen);
    check(!frame.hires && frame.colorMode == VideoColorMode::MonochromeGreen,
          "frame text clears hires");
}

static void testPhosphorLuminance() {
    uint8_t r = 0, g = 0, b = 0;
    mapLuminanceToPhosphor(VideoColorMode::MonochromeGreen, 0, &r, &g, &b);
    check(r == 0 && g == 0 && b == 0, "phosphor green black");
    mapLuminanceToPhosphor(VideoColorMode::MonochromeGreen, 255, &r, &g, &b);
    check(r == kPhosphorGreen.r && g == kPhosphorGreen.g && b == kPhosphorGreen.b,
          "phosphor green peak");
    mapLuminanceToPhosphor(VideoColorMode::MonochromeGreen, 128, &r, &g, &b);
    check(g > r && g > b && g < kPhosphorGreen.g, "phosphor green mid intensity");

    mapLuminanceToPhosphor(VideoColorMode::MonochromeAmber, 255, &r, &g, &b);
    check(r == kPhosphorAmber.r && g == kPhosphorAmber.g && b == kPhosphorAmber.b,
          "phosphor amber peak");
    mapLuminanceToPhosphor(VideoColorMode::MonochromeAmber, 64, &r, &g, &b);
    check(r > b && r < kPhosphorAmber.r, "phosphor amber low intensity");

    const uint8_t lum = luminanceFromRgb888(255, 255, 255);
    check(lum == 255, "luminance white");
    check(luminanceFromRgb888(0, 0, 0) == 0, "luminance black");
}

static void testCrtRgb565Deterministic() {
    constexpr int W = 8;
    constexpr int H = 4;
    uint16_t a[W * H];
    uint16_t b[W * H];
    for (int i = 0; i < W * H; ++i) {
        a[i] = b[i] = ArtifactRenderer::toRgb565({255, 128, 64});
    }
    a[0] = b[0] = ArtifactRenderer::toRgb565({0, 0, 0});
    a[1] = b[1] = ArtifactRenderer::toRgb565({255, 255, 255});

    DisplayEffect::applyRgb565(a, W, H, DisplayEffectMode::Sharp, EffectStrength::Low, false);
    check(a[1] == b[1], "CRT off leaves CLEAN pixels");

    DisplayEffect::applyRgb565(a, W, H, DisplayEffectMode::CrtTv, EffectStrength::Low, true);
    DisplayEffect::applyRgb565(b, W, H, DisplayEffectMode::CrtTv, EffectStrength::Low, true);
    int mism = 0;
    for (int i = 0; i < W * H; ++i) {
        if (a[i] != b[i]) {
            ++mism;
        }
    }
    check(mism == 0, "CRT RGB565 deterministic");
    // Odd scanlines are dimmed vs even (scanline modulation).
    check(a[W + 1] != a[1] || a[W] != a[0], "CRT scanline modulates odd rows");
}

static void testArtifactUnchangedByCleanPath() {
    using AR = ArtifactRenderer;
    uint8_t bits[AR::kWidth]{};
    uint8_t high[40]{};
    bits[0] = 1;
    bits[1] = 0;
    uint16_t lineA[AR::kWidth];
    uint16_t lineB[AR::kWidth];
    AR::renderScanlineRgb565(bits, high, VideoColorMode::CompositeColor, lineA);
    AR::renderScanlineRgb565(bits, high, VideoColorMode::CompositeColor, lineB);
    DisplayEffect::applyRgb565(lineB, AR::kWidth, 1, DisplayEffectMode::Sharp, EffectStrength::Off,
                               false);
    int mism = 0;
    for (int x = 0; x < AR::kWidth; ++x) {
        if (lineA[x] != lineB[x]) {
            ++mism;
        }
    }
    check(mism == 0, "Artifact unchanged when CRT/CLEAN off");
}

int main() {
    testTextLayout();
    testLoresColors();
    testHgrMapping();
    testArtifact();
    testArtifactRgb565Equivalence();
    testPhosphorLuminance();
    testCrtRgb565Deterministic();
    testArtifactUnchangedByCleanPath();
    testHgrClearExactCycles();
    testPageAndMixed();

    if (g_failures != 0) {
        std::fprintf(stderr, "\n%d failure(s)\n", g_failures);
        return 1;
    }
    std::printf("\nAll video tests passed.\n");
    return 0;
}
