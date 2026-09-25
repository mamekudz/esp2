#include "esp_bracket/speaker.hpp"

namespace esp_bracket {

void Speaker::reset() {
    level_ = 0;
    head_ = 0;
    tail_ = 0;
    count_ = 0;
    dropped_ = 0;
}

void Speaker::toggle(uint32_t cycle) {
    level_ = static_cast<uint8_t>(level_ ^ 1u);
    if (count_ >= kRingCapacity) {
        // Drop oldest
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

size_t Speaker::pendingCount() const {
    return count_;
}

} // namespace esp_bracket
