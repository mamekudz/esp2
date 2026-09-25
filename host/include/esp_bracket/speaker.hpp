#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_bracket/apple2_machine.hpp"

namespace esp_bracket {

/**
 * Soft-toggle speaker ($C030). Bounded ring of SpeakerEvent for host audio.
 */
class Speaker {
  public:
    static constexpr size_t kRingCapacity = 4096;

    Speaker() { reset(); }

    void reset();

    /** Toggle level on soft-switch access; records event at cycle. */
    void toggle(uint32_t cycle);

    uint8_t level() const { return level_; }

    /** Drain pending events into out[0..max). Returns count written. */
    size_t consumeEvents(SpeakerEvent *out, size_t max);

    size_t pendingCount() const;
    size_t droppedCount() const { return dropped_; }

  private:
    uint8_t level_ = 0;
    SpeakerEvent ring_[kRingCapacity]{};
    size_t head_ = 0;
    size_t tail_ = 0;
    size_t count_ = 0;
    size_t dropped_ = 0;
};

} // namespace esp_bracket
