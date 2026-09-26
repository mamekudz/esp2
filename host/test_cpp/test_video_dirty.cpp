#include "esp_bracket/apple2_bus.hpp"
#include "esp_bracket/hgr_decoder.hpp"
#include "esp_bracket/text_decoder.hpp"
#include "esp_bracket/video_dirty_tracker.hpp"

#include <cstdio>
#include <cstring>

static int g_fails = 0;

static void expect(bool cond, const char *name) {
    if (cond) {
        std::printf("PASS  %s\n", name);
    } else {
        std::printf("FAIL  %s\n", name);
        ++g_fails;
    }
}

int main() {
    using namespace esp_bracket;

    // Text row reverse mapping round-trip
    for (int row = 0; row < 24; ++row) {
        for (int col = 0; col < 40; ++col) {
            const uint16_t a = TextDecoder::cellAddress(0x0400, row, col);
            expect(VideoDirtyTracker::textRowFromAddress(a) == row, "text row map");
            if (VideoDirtyTracker::textRowFromAddress(a) != row) {
                std::printf("  row=%d col=%d addr=%04X got=%d\n", row, col, a,
                            VideoDirtyTracker::textRowFromAddress(a));
                return 1;
            }
        }
    }
    std::printf("PASS  text row reverse map all cells\n");

    // HGR scanline reverse map
    for (int y = 0; y < 192; ++y) {
        const uint16_t abs = HgrDecoder::lineAddress(0x2000, y);
        const int got =
            VideoDirtyTracker::hgrScanlineFromPageOffset(static_cast<uint16_t>(abs - 0x2000u));
        expect(got == y, "hgr y map");
        if (got != y) {
            std::printf("  y=%d addr=%04X got=%d\n", y, abs, got);
            return 1;
        }
    }
    std::printf("PASS  hgr scanline reverse map\n");

    VideoDirtyTracker dirty;
    Apple2Bus bus;
    bus.setVideoDirtyTracker(&dirty);

    bus.write(TextDecoder::cellAddress(0x0400, 5, 3), 0xC1);
    auto bits = dirty.exchange();
    expect(bits.test(40) && bits.test(47), "text row5 dirty scanlines");
    expect(!bits.test(39) && !bits.test(48), "text row5 neighbors clean");
    expect(dirty.pending().empty(), "exchange clears pending");

    // Coalesce: many writes same line → one bit
    dirty.clear();
    for (int i = 0; i < 100; ++i) {
        bus.write(static_cast<uint16_t>(HgrDecoder::lineAddress(0x2000, 42) + (i % 40)), 0x7F);
    }
    bits = dirty.exchange();
    expect(bits.test(42), "hgr line42 dirty");
    expect(bits.popcount() == 1, "100 writes coalesce to 1 scanline");

    dirty.clear();
    bus.write(0xC050, 0); // GRAPHICS
    bits = dirty.exchange();
    expect(bits.popcount() == 192, "mode change marks all");

    dirty.setEnabled(false);
    bus.write(0x2000, 0xFF);
    expect(dirty.exchange().empty(), "disabled tracker ignores writes");
    dirty.setEnabled(true);
    dirty.clear();

    expect(VideoDirtyTracker::kMetadataBytes == sizeof(VideoDirtyTracker::Bitset), "metadata size");
    expect(VideoDirtyTracker::kMetadataBytes <= 32, "metadata tiny");

    // Page2 text
    dirty.clear();
    bus.write(TextDecoder::cellAddress(0x0800, 0, 0), 0xC1);
    bits = dirty.exchange();
    expect(bits.test(0), "page2 text dirty");

    if (g_fails) {
        std::printf("VideoDirtyTracker failures=%d\n", g_fails);
        return 1;
    }
    std::printf("All VideoDirtyTracker host tests passed\n");
    return 0;
}
