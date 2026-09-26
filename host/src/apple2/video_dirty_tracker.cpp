#include "esp_bracket/video_dirty_tracker.hpp"

namespace esp_bracket {

int VideoDirtyTracker::textRowFromAddress(uint16_t address) {
    uint16_t pageBase = 0;
    if (address >= 0x0400u && address <= 0x07FFu) {
        pageBase = 0x0400u;
    } else if (address >= 0x0800u && address <= 0x0BFFu) {
        pageBase = 0x0800u;
    } else {
        return -1;
    }
    const uint16_t off = static_cast<uint16_t>(address - pageBase);
    const int low = static_cast<int>(off & 0x7Fu);
    if (low >= 120) {
        return -1; // screen hole
    }
    const int third = low / 40; // 0..2
    const int rowLow = static_cast<int>((off >> 7) & 7u);
    return rowLow + third * 8;
}

int VideoDirtyTracker::hgrScanlineFromPageOffset(uint16_t pageOffset) {
    // Inverse of HgrDecoder::lineAddress layout (within 8K page).
    const int low = static_cast<int>(pageOffset & 0x7Fu);
    if (low >= 120) {
        return -1; // screen hole
    }
    const int y2 = low / 40; // 0..2
    const int y1 = static_cast<int>((pageOffset >> 7) & 7u);
    const int y0 = static_cast<int>((pageOffset >> 10) & 7u);
    const int y = y0 | (y1 << 3) | (y2 << 6);
    if (y < 0 || y >= kScanlines) {
        return -1;
    }
    return y;
}

void VideoDirtyTracker::markRamWrite(uint16_t address) {
    if (!enabled_) {
        return;
    }
    if (address >= 0x0400u && address <= 0x0BFFu) {
        const int row = textRowFromAddress(address);
        if (row >= 0) {
            markTextRow(row);
        }
        return;
    }
    if (address >= 0x2000u && address <= 0x3FFFu) {
        const int y = hgrScanlineFromPageOffset(static_cast<uint16_t>(address - 0x2000u));
        if (y >= 0) {
            pending_.mark(y);
        }
        return;
    }
    if (address >= 0x4000u && address <= 0x5FFFu) {
        const int y = hgrScanlineFromPageOffset(static_cast<uint16_t>(address - 0x4000u));
        if (y >= 0) {
            pending_.mark(y);
        }
    }
}

void VideoDirtyTracker::markModeChange() {
    markAll();
}

} // namespace esp_bracket
