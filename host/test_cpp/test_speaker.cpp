#include "esp_bracket/apple2_bus.hpp"
#include "esp_bracket/speaker.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace esp_bracket;

static int g_failures = 0;
static int g_passes = 0;

static void expect(bool ok, const char *name) {
    if (!ok) {
        std::fprintf(stderr, "FAIL  %s\n", name);
        ++g_failures;
    } else {
        std::printf("PASS  %s\n", name);
        ++g_passes;
    }
}

static int16_t expectedDuty(uint32_t high, uint32_t span) {
    return static_cast<int16_t>(static_cast<int32_t>((static_cast<int64_t>(high) * 32000) / span) -
                                16000);
}

static void testOneToggle() {
    Speaker sp;
    sp.toggle(42);
    expect(sp.level() == 1, "one toggle level");
    SpeakerEvent ev[1];
    expect(sp.consumeEvents(ev, 1) == 1 && ev[0].cycle == 42 && ev[0].level == 1, "one edge");
}

static void testSquareWave() {
    // 50% duty: high 50 of every 100 cycles, over 4 samples @ 100 cyc/sample
    std::vector<SpeakerEvent> edges;
    uint32_t t = 0;
    uint8_t lvl = 0;
    for (int i = 0; i < 8; ++i) {
        lvl ^= 1;
        edges.push_back({t, lvl});
        t += 50;
    }
    SpeakerPcmRenderer r;
    r.reset(0);
    r.setRates(100000, 1000);
    int16_t out[4]{};
    r.render(edges.data(), edges.size(), 400, out, 4);
    // Each sample window 100 cycles with 50 high → sample 0
    for (int i = 0; i < 4; ++i) {
        expect(out[i] == 0, "square 50pct");
    }
}

static void testPulseWidths() {
    SpeakerPcmRenderer r;
    auto runPulse = [&](uint32_t width, const char *name) {
        r.reset(0);
        r.setRates(100000, 1000); // 100 cyc/sample
        SpeakerEvent e[2] = {{10, 1}, {static_cast<uint32_t>(10 + width), 0}};
        int16_t out[1]{};
        r.render(e, 2, 100, out, 1);
        expect(out[0] == expectedDuty(width, 100), name);
    };
    runPulse(5, "short pulse");
    runPulse(25, "medium pulse");
    runPulse(80, "long pulse");
}

static void testSubSamplePulse() {
    // Pulse entirely between sample boundaries: sample0=[0,100), pulse at [40,45)
    SpeakerPcmRenderer r;
    r.reset(0);
    r.setRates(100000, 1000);
    SpeakerEvent e[2] = {{40, 1}, {45, 0}};
    int16_t out[1]{};
    r.render(e, 2, 100, out, 1);
    expect(out[0] == expectedDuty(5, 100), "sub-sample pulse amplitude");
    expect(out[0] != -16000, "sub-sample not lost");
}

static void testVaryingPwmSequence() {
    // Three samples, pulses of 10, 40, 70 cycles starting at +10 each window
    std::vector<SpeakerEvent> edges;
    for (int s = 0; s < 3; ++s) {
        const uint32_t base = static_cast<uint32_t>(s * 100);
        const uint32_t w = static_cast<uint32_t>(10 + s * 30);
        edges.push_back({base + 10, 1});
        edges.push_back({base + 10 + w, 0});
    }
    SpeakerPcmRenderer r;
    r.reset(0);
    r.setRates(100000, 1000);
    int16_t out[3]{};
    r.render(edges.data(), edges.size(), 300, out, 3);
    expect(out[0] == expectedDuty(10, 100), "pwm amp0");
    expect(out[1] == expectedDuty(40, 100), "pwm amp1");
    expect(out[2] == expectedDuty(70, 100), "pwm amp2");
    expect(out[0] < out[1] && out[1] < out[2], "pwm increasing");
}

static void testSampleRates44100And48000() {
    constexpr uint32_t cpu = SpeakerPcmRenderer::kDefaultCpuHz;
    // Continuous high from cycle 0
    SpeakerEvent on[1] = {{0, 1}};

    for (uint32_t rate : {44100u, 48000u}) {
        SpeakerPcmRenderer r;
        r.reset(0);
        r.setRates(cpu, rate);
        const uint64_t needCycles = r.sampleStartCycle(100);
        int16_t out[100]{};
        r.render(on, 1, needCycles + 1, out, 100);
        bool allHigh = true;
        for (int i = 0; i < 100; ++i) {
            if (out[i] != 16000) {
                allHigh = false;
                break;
            }
        }
        expect(allHigh, rate == 44100 ? "44.1k full high" : "48k full high");
        // Emulation timing independent: edge still at cycle 0
        expect(on[0].cycle == 0, rate == 44100 ? "44.1k edge time" : "48k edge time");
    }
}

static void testLongDurationNoDrift() {
    // 1 second of 50% square at Apple II rate / 44100 — mean near 0
    constexpr uint32_t cpu = 1020484;
    constexpr uint32_t rate = 44100;
    constexpr uint32_t halfPeriod = 256; // cycles high/low
    std::vector<SpeakerEvent> edges;
    edges.reserve(8000);
    uint8_t lvl = 0;
    for (uint32_t c = 0; c < cpu; c += halfPeriod) {
        lvl ^= 1;
        edges.push_back({c, lvl});
    }
    SpeakerPcmRenderer r;
    r.reset(0);
    r.setRates(cpu, rate);
    const size_t nSamp = rate; // 1 second
    std::vector<int16_t> out(nSamp);
    const uint64_t end = r.sampleStartCycle(nSamp);
    r.render(edges.data(), edges.size(), end, out.data(), nSamp);

    // Check rational cursor: sampleStartCycle(n) == (n*cpu)/rate exactly
    expect(r.sampleStartCycle(nSamp) == (static_cast<uint64_t>(nSamp) * cpu) / rate,
           "rational sample mapping");
    expect(r.cursorCycle64() == end, "cursor matches end");

    int64_t sum = 0;
    for (size_t i = 0; i < nSamp; ++i) {
        sum += out[i];
    }
    const double mean = static_cast<double>(sum) / static_cast<double>(nSamp);
    expect(std::fabs(mean) < 500.0, "long-run mean near 0");
}

static void testBufferOverflow() {
    Speaker sp;
    for (size_t i = 0; i < Speaker::kRingCapacity + 10; ++i) {
        sp.toggle(static_cast<uint32_t>(i * 3));
    }
    expect(sp.pendingCount() == Speaker::kRingCapacity, "ring full");
    expect(sp.droppedCount() == 10, "oldest dropped");
    expect(sp.diag().dropped == 10, "diag bufferOverruns");
}

static void testResetBehavior() {
    Speaker sp;
    sp.toggle(10);
    sp.toggle(20);
    sp.reset();
    expect(sp.level() == 0, "reset level");
    expect(sp.pendingCount() == 0, "reset pending");
    expect(sp.droppedCount() == 0, "reset dropped");
    SpeakerPcmRenderer r;
    r.setRates(1000, 100);
    int16_t tmp[4]{};
    SpeakerEvent e[1] = {{0, 1}};
    r.render(e, 1, 100, tmp, 4);
    r.reset(0);
    expect(r.renderedSamples() == 0 && r.level() == 0, "pcm reset");
}

static void testC030MirrorsViaBus() {
    Apple2Bus bus;
    for (uint16_t a = 0xC030; a <= 0xC03F; ++a) {
        bus.speaker().reset();
        bus.setAccessCycle(1000 + a);
        bus.read(a);
        SpeakerEvent ev[2];
        expect(bus.speaker().consumeEvents(ev, 2) == 1, "mirror one edge");
        expect(ev[0].cycle == static_cast<uint32_t>(1000 + a), "mirror cycle");
    }
}

static void testHostBackendSeparation() {
    SpeakerPcmRenderer r;
    r.reset(0);
    r.setRates(100000, 1000);
    SpeakerEvent e[1] = {{0, 1}};
    int16_t pcm[2]{};
    r.render(e, 1, 200, pcm, 2);
    CapturingAudioBackend backend;
    expect(backend.writePcm(pcm, 2) == 2, "host backend write");
    expect(backend.data()[0] == 16000, "backend pcm");
}

static void testDiagFields() {
    Speaker sp;
    sp.toggle(100);
    sp.toggle(130);
    sp.toggle(200);
    const auto d = sp.diag();
    expect(d.edgeCount == 3, "diag edgeCount");
    expect(d.minDeltaCycles == 30, "diag minDelta");
    expect(d.maxDeltaCycles == 70, "diag maxDelta");
    SpeakerPcmRenderer r;
    r.setRates(44100, 44100);
    expect(r.diag().sampleRate == 44100, "diag sampleRate");
}

static void benchmarkRenderer() {
    constexpr uint32_t cpu = 1020484;
    constexpr uint32_t rate = 44100;
    std::vector<SpeakerEvent> edges;
    uint8_t lvl = 0;
    for (uint32_t c = 0; c < cpu / 10; c += 40) {
        lvl ^= 1;
        edges.push_back({c, lvl});
    }
    SpeakerPcmRenderer r;
    r.reset(0);
    r.setRates(cpu, rate);
    const size_t nSamp = rate / 10; // 100ms
    std::vector<int16_t> out(nSamp);
    const auto t0 = std::chrono::steady_clock::now();
    r.render(edges.data(), edges.size(), r.sampleStartCycle(nSamp), out.data(), nSamp);
    const auto t1 = std::chrono::steady_clock::now();
    const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    std::printf("HOST MEASUREMENT  speaker_pcm_100ms_ms=%.3f edges=%zu samples=%zu\n", ms,
                edges.size(), nSamp);
    expect(ms < 500.0, "benchmark completes");
}

int main() {
    testOneToggle();
    testSquareWave();
    testPulseWidths();
    testSubSamplePulse();
    testVaryingPwmSequence();
    testSampleRates44100And48000();
    testLongDurationNoDrift();
    testBufferOverflow();
    testResetBehavior();
    testC030MirrorsViaBus();
    testHostBackendSeparation();
    testDiagFields();
    benchmarkRenderer();

    std::printf("\nSpeaker fidelity tests: %d passed, %d failed\n", g_passes, g_failures);
    return g_failures == 0 ? 0 : 1;
}
