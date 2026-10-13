#pragma once

/**
 * Host-testable BLE device / settings list model (no NimBLE dependency).
 */

#include <cstddef>
#include <cstdint>

namespace esp_bracket {

enum class BleLinkState : uint8_t {
    Discovered = 0,
    Connecting,
    Pairing,
    Connected,
    KnownOffline,
    Error,
};

enum class BleHidKind : uint8_t {
    Unknown = 0,
    Keyboard = 1,
    Mouse = 2,
    Gamepad = 3,
    Composite = 4,
};

struct ListViewport {
    size_t rowCount = 0;
    size_t selected = 0;
    size_t visibleRows = 0;
    size_t scrollTop = 0;
};

/** Keep selection visible; clamp scrollTop. */
inline ListViewport computeListViewport(size_t rowCount, size_t selected, size_t visibleRows) {
    ListViewport v{};
    v.rowCount = rowCount;
    v.visibleRows = visibleRows;
    if (visibleRows == 0) {
        v.selected = selected < rowCount ? selected : (rowCount > 0 ? rowCount - 1 : 0);
        return v;
    }
    v.selected = selected;
    if (v.selected >= rowCount && rowCount > 0) {
        v.selected = rowCount - 1;
    }
    if (rowCount <= visibleRows) {
        v.scrollTop = 0;
        return v;
    }
    if (v.selected < v.scrollTop) {
        v.scrollTop = v.selected;
    } else if (v.selected >= v.scrollTop + visibleRows) {
        v.scrollTop = v.selected + 1 - visibleRows;
    }
    if (v.scrollTop + visibleRows > rowCount) {
        v.scrollTop = rowCount - visibleRows;
    }
    return v;
}

inline BleHidKind classifyHidKinds(bool keyboard, bool mouse, bool gamepad, bool joystick) {
    const bool pointerLike = mouse || gamepad || joystick;
    if (keyboard && pointerLike) {
        return BleHidKind::Composite;
    }
    if (keyboard) {
        return BleHidKind::Keyboard;
    }
    if (gamepad || joystick) {
        return BleHidKind::Gamepad;
    }
    if (mouse) {
        return BleHidKind::Mouse;
    }
    return BleHidKind::Unknown;
}

/** Relative HID mouse delta → accumulated virtual axis (same math as firmware). */
inline int16_t clampVirtualAxis(int32_t v) {
    if (v > 32767) {
        return 32767;
    }
    if (v < -32768) {
        return -32768;
    }
    return static_cast<int16_t>(v);
}

inline int16_t accumulateRelativeAxis(int16_t current, int8_t delta, uint8_t sensitivity) {
    if (sensitivity < 1) {
        sensitivity = 1;
    }
    if (sensitivity > 32) {
        sensitivity = 32;
    }
    const int32_t next =
        static_cast<int32_t>(current) + static_cast<int32_t>(delta) * static_cast<int32_t>(sensitivity) * 256;
    return clampVirtualAxis(next);
}

} // namespace esp_bracket
