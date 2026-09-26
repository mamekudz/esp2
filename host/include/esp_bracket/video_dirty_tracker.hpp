#pragma once

#include <cstddef>
#include <cstdint>

namespace esp_bracket {

/**
 * Dirty metadata for physical display optimization.
 *
 * Apple II VRAM writes remain fully emulated; this only records which
 * logical scanlines (0..191) may need a CO5300 update.
 *
 * Must NEVER skip/combine emulated RAM writes or alter 6502 timing.
 */
class VideoDirtyTracker {
  public:
    static constexpr int kScanlines = 192;
    static constexpr int kWordCount = (kScanlines + 31) / 32; // 6
    static constexpr int kTextRows = 24;

    struct Bitset {
        uint32_t words[kWordCount]{};

        void clear() {
            for (int i = 0; i < kWordCount; ++i) {
                words[i] = 0;
            }
        }

        void mark(int y) {
            if (y < 0 || y >= kScanlines) {
                return;
            }
            words[y >> 5] |= (uint32_t{1} << (y & 31));
        }

        void markAll() {
            for (int i = 0; i < kWordCount; ++i) {
                words[i] = 0xFFFFFFFFu;
            }
            // Clear unused high bits in last word (192 = 6*32 exactly).
        }

        bool test(int y) const {
            if (y < 0 || y >= kScanlines) {
                return false;
            }
            return (words[y >> 5] & (uint32_t{1} << (y & 31))) != 0;
        }

        int popcount() const {
            int n = 0;
            for (int i = 0; i < kWordCount; ++i) {
                uint32_t w = words[i];
                while (w) {
                    w &= (w - 1);
                    ++n;
                }
            }
            return n;
        }

        bool empty() const {
            for (int i = 0; i < kWordCount; ++i) {
                if (words[i] != 0) {
                    return false;
                }
            }
            return true;
        }
    };

    VideoDirtyTracker() { clear(); }

    void clear() { pending_.clear(); }

    void setEnabled(bool on) { enabled_ = on; }
    bool enabled() const { return enabled_; }

    /** After authentic RAM store — map address into scanline dirty bits. */
    void markRamWrite(uint16_t address);

    /** Soft-switch / mode change — entire viewport may reinterpret memory. */
    void markModeChange();

    void markAll() {
        if (enabled_) {
            pending_.markAll();
        }
    }

    void markScanline(int y) {
        if (enabled_) {
            pending_.mark(y);
        }
    }

    void markTextRow(int row) {
        if (!enabled_ || row < 0 || row >= kTextRows) {
            return;
        }
        const int y0 = row * 8;
        for (int i = 0; i < 8; ++i) {
            pending_.mark(y0 + i);
        }
    }

    /** Snapshot pending bits and clear them (display-task handoff). */
    Bitset exchange() {
        Bitset out = pending_;
        pending_.clear();
        return out;
    }

    const Bitset &pending() const { return pending_; }

    /** Map text/LoRes page address → text row 0..23, or -1. */
    static int textRowFromAddress(uint16_t address);

    /** Map HGR page-relative offset → scanline 0..191, or -1 (hole). */
    static int hgrScanlineFromPageOffset(uint16_t pageOffset);

    static constexpr size_t kMetadataBytes = sizeof(Bitset);

  private:
    Bitset pending_{};
    bool enabled_ = true;
};

} // namespace esp_bracket
