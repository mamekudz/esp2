#include "esp_bracket/speaker.hpp"

namespace esp_bracket {

void Speaker::reset() {
    level_ = 0;
    head_ = 0;
    tail_ = 0;
    count_ = 0;
    dropped_ = 0;
    edgeTotal_ = 0;
    lastEdgeCycle_ = 0;
    haveLastEdge_ = false;
    minDelta_ = 0xFFFFFFFFu;
    maxDelta_ = 0;
}

void Speaker::toggle(uint32_t cycle) {
    if (haveLastEdge_) {
        const uint32_t d = cycle - lastEdgeCycle_;
        if (d < minDelta_) {
            minDelta_ = d;
        }
        if (d > maxDelta_) {
            maxDelta_ = d;
        }
    }
    lastEdgeCycle_ = cycle;
    haveLastEdge_ = true;
    ++edgeTotal_;

    level_ = static_cast<uint8_t>(level_ ^ 1u);
    if (count_ >= kRingCapacity) {
        // Explicit overflow: drop oldest edge, count overrun.
        head_ = (head_ + 1) % kRingCapacity;
        --count_;
        ++dropped_;
    }
    ring_[tail_] = SpeakerEvent{cycle, level_};
    tail_ = (tail_ + 1) % kRingCapacity;
    ++count_;
}

size_t Speaker::consumeEvents(SpeakerEvent *out, size_t max) {
    if (!out || max == 0) {
        return 0;
    }
    size_t n = 0;
    while (n < max && count_ > 0) {
        out[n++] = ring_[head_];
        head_ = (head_ + 1) % kRingCapacity;
        --count_;
    }
    return n;
}

size_t Speaker::peekEvents(SpeakerEvent *out, size_t max) const {
    if (!out || max == 0) {
        return 0;
    }
    size_t n = 0;
    size_t idx = head_;
    size_t left = count_;
    while (n < max && left > 0) {
        out[n++] = ring_[idx];
        idx = (idx + 1) % kRingCapacity;
        --left;
    }
    return n;
}

size_t Speaker::pendingCount() const {
    return count_;
}

Speaker::Diag Speaker::diag() const {
    Diag d{};
    d.edgeCount = edgeTotal_;
    d.pending = static_cast<uint32_t>(count_);
    d.dropped = static_cast<uint32_t>(dropped_);
    d.minDeltaCycles = haveLastEdge_ ? minDelta_ : 0;
    d.maxDeltaCycles = maxDelta_;
    d.level = level_;
    return d;
}

void SpeakerPcmRenderer::reset(uint8_t initialLevel) {
    level_ = initialLevel & 1u;
    cursorCycle_ = 0;
    edgeIndex_ = 0;
    samplesOut_ = 0;
    underruns_ = 0;
}

void SpeakerPcmRenderer::setRates(uint32_t cpuHz, uint32_t sampleRateHz) {
    if (cpuHz > 0) {
        cpuHz_ = cpuHz;
    }
    if (sampleRateHz > 0) {
        sampleRate_ = sampleRateHz;
    }
}

SpeakerPcmRenderer::Diag SpeakerPcmRenderer::diag() const {
    Diag d{};
    d.pcmSamples = samplesOut_;
    d.sampleRate = sampleRate_;
    d.cpuHz = cpuHz_;
    d.underruns = underruns_;
    d.cursorCycle = cursorCycle_;
    return d;
}

uint32_t SpeakerPcmRenderer::highCyclesInRange(uint8_t levelAtStart, const SpeakerEvent *edges,
                                               size_t edgeCount, uint32_t rangeStart,
                                               uint32_t rangeEnd, size_t *ioEdgeIndex) {
    if (rangeEnd <= rangeStart) {
        return 0;
    }
    size_t ei = ioEdgeIndex ? *ioEdgeIndex : 0;
    while (ei < edgeCount && edges[ei].cycle <= rangeStart) {
        ++ei;
    }

    uint8_t level = levelAtStart & 1u;
    uint32_t t = rangeStart;
    uint32_t high = 0;

    while (t < rangeEnd) {
        uint32_t next = rangeEnd;
        if (ei < edgeCount && edges[ei].cycle < rangeEnd) {
            next = edges[ei].cycle;
        }
        if (level != 0) {
            high += next - t;
        }
        t = next;
        if (ei < edgeCount && edges[ei].cycle == t && t < rangeEnd) {
            level = edges[ei].level & 1u;
            ++ei;
        }
    }

    if (ioEdgeIndex) {
        *ioEdgeIndex = ei;
    }
    return high;
}

size_t SpeakerPcmRenderer::render(const SpeakerEvent *edges, size_t edgeCount,
                                  uint64_t machineCycleEnd, int16_t *out, size_t outCount) {
    if (!out || outCount == 0 || sampleRate_ == 0 || cpuHz_ == 0) {
        return 0;
    }

    size_t written = 0;
    size_t ei = edgeIndex_;

    for (size_t s = 0; s < outCount; ++s) {
        const uint64_t rangeStart = sampleStartCycle(samplesOut_);
        const uint64_t rangeEnd = sampleStartCycle(samplesOut_ + 1);

        if (rangeEnd > machineCycleEnd || rangeEnd <= rangeStart) {
            ++underruns_;
            out[written++] = level_ ? static_cast<int16_t>(16000) : static_cast<int16_t>(-16000);
            ++samplesOut_;
            cursorCycle_ = rangeStart;
            continue;
        }

        // Edges use uint32 stamps; for windows within first ~4000s @1MHz they fit.
        const uint32_t rs = static_cast<uint32_t>(rangeStart);
        const uint32_t re = static_cast<uint32_t>(rangeEnd);

        while (ei < edgeCount && edges[ei].cycle <= rs) {
            level_ = edges[ei].level & 1u;
            ++ei;
        }

        size_t eiWalk = ei;
        const uint32_t high = highCyclesInRange(level_, edges, edgeCount, rs, re, &eiWalk);

        while (ei < edgeCount && edges[ei].cycle < re) {
            level_ = edges[ei].level & 1u;
            ++ei;
        }
        ei = eiWalk;
        while (ei < edgeCount && edges[ei].cycle == re) {
            level_ = edges[ei].level & 1u;
            ++ei;
        }

        const uint32_t span = re - rs;
        int32_t sample = static_cast<int32_t>((static_cast<int64_t>(high) * 32000) / span) - 16000;
        if (sample > 32767) {
            sample = 32767;
        }
        if (sample < -32768) {
            sample = -32768;
        }
        out[written++] = static_cast<int16_t>(sample);
        ++samplesOut_;
        cursorCycle_ = rangeEnd;
    }

    edgeIndex_ = ei;
    return written;
}

void SpeakerEdgeDeltaEncoder::reset(uint32_t startCycle) {
    lastCycle_ = startCycle;
    started_ = false;
}

size_t SpeakerEdgeDeltaEncoder::encode(const SpeakerEvent *edges, size_t edgeCount,
                                       SpeakerEdgeDelta *out, size_t outCap) {
    if (!edges || !out || outCap == 0) {
        return 0;
    }
    size_t n = 0;
    for (size_t i = 0; i < edgeCount && n < outCap; ++i) {
        const uint32_t c = edges[i].cycle;
        out[n].deltaCycles = c - lastCycle_;
        out[n].levelAfter = edges[i].level & 1u;
        lastCycle_ = c;
        started_ = true;
        ++n;
    }
    return n;
}

size_t CapturingAudioBackend::writePcm(const int16_t *samples, size_t count) {
    if (!samples || count == 0) {
        return 0;
    }
    size_t n = 0;
    while (n < count && size_ < kCap) {
        buf_[size_++] = samples[n++];
    }
    return n;
}

} // namespace esp_bracket
