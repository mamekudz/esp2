#include "esp_bracket/artifact_renderer.hpp"
#include "esp_bracket/hgr_decoder.hpp"
#include "esp_bracket/lores_decoder.hpp"
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

int main() {
    testTextLayout();
    testLoresColors();
    testHgrMapping();
    testArtifact();
    testPageAndMixed();

    if (g_failures != 0) {
        std::fprintf(stderr, "\n%d failure(s)\n", g_failures);
        return 1;
    }
    std::printf("\nAll video tests passed.\n");
    return 0;
}
