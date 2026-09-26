#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_bracket/apple2_machine.hpp"

namespace esp_bracket {

/**
 * Soft-toggle speaker ($C030–$C03F).
 *
 * Cycle-accurate 1-bit model: every soft-switch access records an edge
 * {cycle, level}. Do not collapse to beeps/notes/frequencies here.
 * Timing is preserved until SpeakerPcmRenderer / BlueShift packing.
 */
class Speaker {
  public:
    static constexpr size_t kRingCapacity = 4096;

    Speaker() { reset(); }

    void reset();

    /** Toggle 1-bit level; record edge at exact emulated cycle. */
    void toggle(uint32_t cycle);

    uint8_t level() const { return level_; }

    size_t consumeEvents(SpeakerEvent *out, size_t max);
    size_t peekEvents(SpeakerEvent *out, size_t max) const;

    size_t pendingCount() const;
    size_t droppedCount() const { return dropped_; }
    size_t edgeCountTotal() const { return edgeTotal_; }

    /** Language-neutral diagnostics snapshot. */
    struct Diag {
        uint64_t edgeCount = 0;
        uint32_t pending = 0;
        uint32_t dropped = 0; // bufferOverruns (oldest-drop)
        uint32_t minDeltaCycles = 0xFFFFFFFFu;
        uint32_t maxDeltaCycles = 0;
        uint8_t level = 0;
    };
    Diag diag() const;

  private:
    uint8_t level_ = 0;
    SpeakerEvent ring_[kRingCapacity]{};
    size_t head_ = 0;
    size_t tail_ = 0;
    size_t count_ = 0;
    size_t dropped_ = 0;
    size_t edgeTotal_ = 0;
    uint32_t lastEdgeCycle_ = 0;
    bool haveLastEdge_ = false;
    uint32_t minDelta_ = 0xFFFFFFFFu;
    uint32_t maxDelta_ = 0;
};

/**
 * Integrate 1-bit speaker state over each PCM sample interval.
 * Short pulses contribute fractional amplitude — not dropped.
 *
 * Sample positions use rational mapping:
 *   cycle = (sampleIndex * cpuHz) / sampleRate
 * so long runs do not accumulate stepwise rounding drift.
 */
class SpeakerPcmRenderer {
  public:
    static constexpr uint32_t kDefaultCpuHz = 1020484; // Apple II NTSC approx
    static constexpr uint32_t kDefaultSampleRate = 44100;

    SpeakerPcmRenderer() = default;

    void reset(uint8_t initialLevel = 0);
    void setRates(uint32_t cpuHz, uint32_t sampleRateHz);

    uint8_t level() const { return level_; }
    uint64_t renderedSamples() const { return samplesOut_; }
    uint32_t underruns() const { return underruns_; }
    uint32_t sampleRate() const { return sampleRate_; }
    uint32_t cpuHz() const { return cpuHz_; }
    uint64_t cursorCycle64() const { return cursorCycle_; }

    struct Diag {
        uint64_t pcmSamples = 0;
        uint32_t sampleRate = 0;
        uint32_t cpuHz = 0;
        uint32_t underruns = 0;
        uint64_t cursorCycle = 0;
    };
    Diag diag() const;

    /**
     * Render PCM from chronologically ordered edges.
     * machineCycleEnd is exclusive end of available emulated time (uint64 domain
     * for long streams; edges remain uint32 stamps within the active window).
     */
    size_t render(const SpeakerEvent *edges, size_t edgeCount, uint64_t machineCycleEnd,
                  int16_t *out, size_t outCount);

    static uint32_t highCyclesInRange(uint8_t startLevel, const SpeakerEvent *edges,
                                      size_t edgeCount, uint32_t rangeStart, uint32_t rangeEnd,
                                      size_t *ioEdgeIndex);

    /** Absolute cycle at the start of sampleIndex (rational, no stepwise drift). */
    uint64_t sampleStartCycle(uint64_t sampleIndex) const {
        return (sampleIndex * static_cast<uint64_t>(cpuHz_)) / sampleRate_;
    }

  private:
    uint32_t cpuHz_ = kDefaultCpuHz;
    uint32_t sampleRate_ = kDefaultSampleRate;
    uint8_t level_ = 0;
    uint64_t cursorCycle_ = 0;
    size_t edgeIndex_ = 0;
    uint64_t samplesOut_ = 0;
    uint32_t underruns_ = 0;
};

/**
 * Optional BlueShift™ transport prep (encode only — no BLE in this repo).
 * Prefer cycle-delta edges over PCM on the wire.
 */
struct SpeakerEdgeDelta {
    uint32_t deltaCycles;
    uint8_t levelAfter;
};

class SpeakerEdgeDeltaEncoder {
  public:
    void reset(uint32_t startCycle = 0);
    size_t encode(const SpeakerEvent *edges, size_t edgeCount, SpeakerEdgeDelta *out,
                  size_t outCap);
    uint32_t lastAbsoluteCycle() const { return lastCycle_; }

  private:
    uint32_t lastCycle_ = 0;
    bool started_ = false;
};

/**
 * Host-only PCM sink boundary. No Apple II / browser assumptions in Speaker.
 * Physical ESP32 / BlueShift reconstruct from the same edge stream.
 */
class HostAudioBackend {
  public:
    virtual ~HostAudioBackend() = default;
    /** Accept interleaved mono int16 frames. Returns frames consumed. */
    virtual size_t writePcm(const int16_t *samples, size_t count) = 0;
};

/** Captures PCM for host tests (bounded). */
class CapturingAudioBackend : public HostAudioBackend {
  public:
    static constexpr size_t kCap = 8192;
    size_t writePcm(const int16_t *samples, size_t count) override;
    size_t size() const { return size_; }
    const int16_t *data() const { return buf_; }
    void clear() { size_ = 0; }

  private:
    int16_t buf_[kCap]{};
    size_t size_ = 0;
};

} // namespace esp_bracket
