/**
 * Host performance benchmarks (HOST MEASUREMENT lines).
 * Emulated cycles are authoritative; wall-clock is host-only evidence.
 */
#include "esp_bracket/apple2_machine_host.hpp"
#include "esp_bracket/artifact_renderer.hpp"
#include "esp_bracket/disk_ii_encoding.hpp"
#include "esp_bracket/disk_ii_media.hpp"
#include "esp_bracket/disk_ii_track.hpp"
#include "esp_bracket/hgr_decoder.hpp"
#include "esp_bracket/media_types.hpp"
#include "esp_bracket/text_decoder.hpp"
#include "esp_bracket/types.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

using namespace esp_bracket;

static double msSince(std::chrono::steady_clock::time_point t0) {
    using namespace std::chrono;
    return duration<double, std::milli>(steady_clock::now() - t0).count();
}

static void benchCpu() {
    auto m = std::make_unique<HostAppleIIMachine>();
    m->loadSyntheticRom();
    m->powerOn();
    const uint32_t budget = 2000000;
    const auto t0 = std::chrono::steady_clock::now();
    m->runCycles(budget);
    const double ms = msSince(t0);
    const double cps = ms > 0 ? (static_cast<double>(budget) * 1000.0 / ms) : 0;
    std::printf("HOST MEASUREMENT  cpu_emu_cycles_per_s=%.0f budget=%u ms=%.3f\n", cps, budget, ms);
}

static void benchMachine() {
    auto m = std::make_unique<HostAppleIIMachine>();
    m->loadSyntheticRom();
    m->mountDisk(DriveId::Drive1, "Esp2BootTest");
    m->powerOn();
    CpuRegisters r = m->cpu().registers();
    r.pc = 0xC600;
    m->cpu().setRegisters(r);
    const uint32_t budget = 2000000;
    const auto t0 = std::chrono::steady_clock::now();
    m->runCycles(budget);
    const double ms = msSince(t0);
    std::printf("HOST MEASUREMENT  machine_2e6_cycles_ms=%.3f\n", ms);
}

static void benchVideo() {
    auto ram = std::make_unique<uint8_t[]>(0xC000);
    std::memset(ram.get(), 0, 0xC000);
    TextDecoder::writeTextScreen(ram.get(), 0x0400, "ESP][ PERF", 0, 0);
    uint8_t chars[40 * 24];
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 200; ++i) {
        TextDecoder::decodeScreen(ram.get(), 0x0400, chars);
    }
    std::printf("HOST MEASUREMENT  text_decode_200_ms=%.3f\n", msSince(t0));

    HgrDecoder::writePattern(ram.get(), 0x2000, "vline");
    auto bits = std::make_unique<uint8_t[]>(280 * 192);
    auto high = std::make_unique<uint8_t[]>(40 * 192);
    t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 50; ++i) {
        HgrDecoder::decode(ram.get(), 0x2000, bits.get(), high.get());
    }
    std::printf("HOST MEASUREMENT  hgr_decode_50_ms=%.3f\n", msSince(t0));

    std::vector<uint8_t> rgb(280 * 192 * 3);
    t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 50; ++i) {
        ArtifactRenderer::render(bits.get(), high.get(), VideoColorMode::CompositeColor, rgb.data(),
                                 rgb.size());
    }
    std::printf("HOST MEASUREMENT  artifact_50_ms=%.3f\n", msSince(t0));
}

static void benchDisk() {
    auto raw = std::make_unique<uint8_t[]>(kDos33ImageBytes);
    std::memset(raw.get(), 0, kDos33ImageBytes);
    auto t0 = std::chrono::steady_clock::now();
    auto img = std::make_unique<Dos33NibbleImage>();
    img->load(raw.get(), kDos33ImageBytes, false);
    size_t len = 0;
    for (int t = 0; t < 35; ++t) {
        (void)img->trackNibbles(t, &len);
    }
    std::printf("HOST MEASUREMENT  dsk_nibble_35tracks_ms=%.3f\n", msSince(t0));

    uint8_t sec[256];
    uint8_t nib[DiskIIEncoding::kDataNibbles];
    for (int i = 0; i < 256; ++i) {
        sec[i] = static_cast<uint8_t>(i);
    }
    t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 1000; ++i) {
        DiskIIEncoding::encodeSector(sec, nib);
        DiskIIEncoding::decodeSector(nib, sec);
    }
    std::printf("HOST MEASUREMENT  encode_decode_1000_ms=%.3f\n", msSince(t0));
}

int main() {
    benchCpu();
    benchMachine();
    benchVideo();
    benchDisk();
    std::printf("Perf benchmarks OK\n");
    return 0;
}
