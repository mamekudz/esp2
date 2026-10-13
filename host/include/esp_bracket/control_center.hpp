#pragma once

/**
 * Host-testable ESP][ Control Center: layout, touch gate, coordinate map.
 * Firmware UI is a frontend over existing display/disk/BLE/SD services.
 */

#include <cstdint>

namespace esp_bracket {

enum class SystemTouchMode : uint8_t {
    Menu = 0,
    FutureEmu = 1,
};

enum class CcGate : uint8_t {
    Closed = 0,
    WaitRelease = 1,
    Interactive = 2,
};

enum class CcEvent : uint8_t {
    None = 0,
    OpenMenu = 1,
    Activate = 2,
};

enum class CcHit : int16_t {
    None = -1,
    Display = 0,
    Disks = 1,
    Input = 2,
    Bluetooth = 3,
    Storage = 4,
    Settings = 5,
    Reset = 6,
    Resume = 7,
    Back = 8,
    JoyTest = 16,
    // Display options
    OrientClassic = 20,
    OrientLandscape = 21,
    MonitorWhite = 22,
    MonitorGreen = 23,
    MonitorAmber = 24,
    MonitorArtifact = 25,
    EffectClean = 26,
    EffectCrt = 27,
    MonoNormal = 28,  // RLCD: dark ink on paper
    MonoInvert = 29,  // RLCD: light ink on dark
    // Disks
    DiskBrowse = 30,
    DiskEject = 31,
    // RLCD classic fit (integer 1× default; full panel scale optional)
    ScaleInteger = 32,
    ScaleFull = 33,
    DiskFile0 = 40, // visible row slots 40..51 (index = listScroll + slot)
    DiskFileMax = 51,
    // BLE
    BleAdd = 60,
    BleList = 61,
    BleScanItem0 = 200, // 200..223
    BleKnown0 = 90,    // 90..97
    BleConnect = 100,
    BleDisconnect = 101,
    BleForget = 102,
    BleForgetYes = 103,
    BleForgetNo = 104,
    // Storage
    StorageShare = 110,
    StorageShareYes = 111,
    StorageShareNo = 112,
    StorageReturn = 113,
    // Settings
    LocaleEn = 120,
    LocaleDe = 121,
    // Reset
    ResetApple = 130,
    ColdStart = 131,
    RestartEsp = 132,
    ConfirmYes = 133,
    ConfirmNo = 134,
    ListPrev = 140,
    ListNext = 141,
};

struct CcRect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

inline bool ccRectHit(const CcRect &r, int px, int py) {
    return px >= r.x && py >= r.y && px < r.x + r.w && py < r.y + r.h;
}

/** Minimum finger target (px). */
inline constexpr int kCcMinHit = 48;
inline constexpr int kCcHeaderH = 40;
inline constexpr int kCcFooterH = 52;
inline constexpr int kCcPad = 8;
inline constexpr int kCcHomeTiles = 7; // Display..Reset (Resume is footer)

/** Always-present Bluetooth hub actions (frontend; backend may be disabled). */
inline constexpr int kCcBleHubActions = 2;

enum class CcUiSurface : uint8_t {
    FullPanel = 0,
    EnclosureVisible = 1,
};

enum class CcPresentMode : uint8_t {
    Classic = 0,
    Landscape = 1,
};

/** System-UI rectangle in physical panel pixels (not the Apple II blit). */
struct CcUiViewport {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

struct CcViewportOverride {
    bool enabled = false;
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

struct CcLayoutContext {
    int physicalW = 0;
    int physicalH = 0;
    CcPresentMode present = CcPresentMode::Classic;
    CcUiSurface surface = CcUiSurface::FullPanel;
    CcViewportOverride over{};
    CcUiViewport ui{};
};

inline CcRect ccAsRect(const CcUiViewport &v) { return {v.x, v.y, v.w, v.h}; }

inline bool ccTouchInUiViewport(const CcUiViewport &ui, int panelX, int panelY) {
    return ccRectHit(ccAsRect(ui), panelX, panelY);
}

inline bool ccToUiLocal(const CcUiViewport &ui, int panelX, int panelY, int *lx, int *ly) {
    if (!lx || !ly || !ccTouchInUiViewport(ui, panelX, panelY)) {
        return false;
    }
    *lx = panelX - ui.x;
    *ly = panelY - ui.y;
    return true;
}

inline CcUiViewport ccClipViewportToPanel(int x, int y, int w, int h, int panelW, int panelH) {
    if (panelW < 80 || panelH < 120) {
        return {0, 0, panelW, panelH};
    }
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x >= panelW || y >= panelH) {
        return {0, 0, panelW, panelH};
    }
    if (x + w > panelW) {
        w = panelW - x;
    }
    if (y + h > panelH) {
        h = panelH - y;
    }
    if (w < 80 || h < 120) {
        return {0, 0, panelW, panelH};
    }
    return {x, y, w, h};
}

/**
 * Waveshare 280×456 enclosure candidate (PROVISIONAL_PHYSICAL_VERIFY).
 * Top inset hides active-area R2.50 (~2.5/36.06×456 ≈ 32 px). Height keeps
 * header + 3-row home + footer inside the upper window; lower panel is
 * treated as drive/bezel-hidden (CLAUDE.md §7). Not CAD-final.
 */
inline CcUiViewport ccProvisionalEnclosureVisible(int panelW, int panelH) {
    if (panelW == 280 && panelH == 456) {
        return ccClipViewportToPanel(0, 32, 280, 280, panelW, panelH);
    }
    return {0, 0, panelW, panelH};
}

inline CcUiSurface ccDefaultUiSurface(int panelW, int panelH) {
    if (panelW == 280 && panelH == 456) {
        return CcUiSurface::EnclosureVisible;
    }
    return CcUiSurface::FullPanel;
}

inline CcUiViewport ccComputeSystemUiViewport(int panelW, int panelH, CcPresentMode present,
                                             CcUiSurface surface, const CcViewportOverride &over) {
    (void)present;
    CcUiViewport v;
    if (over.enabled) {
        v = ccClipViewportToPanel(over.x, over.y, over.w, over.h, panelW, panelH);
    } else if (surface == CcUiSurface::EnclosureVisible) {
        v = ccProvisionalEnclosureVisible(panelW, panelH);
    } else {
        v = {0, 0, panelW, panelH};
    }
    return v;
}

inline bool ccRowFullyInsideBody(const CcUiViewport &ui, const CcRect &localRow) {
    const int bodyBottom = ui.h - kCcFooterH;
    return localRow.y >= kCcHeaderH && localRow.y + localRow.h <= bodyBottom;
}

inline int ccHomeCols(int panelW) {
    return panelW >= 240 ? 2 : 1;
}

/** Short viewports (diymore 240×320, Waveshare enclosure window) use 3 columns. */
inline int ccHomeColsFor(int panelW, int panelH) {
    if (panelH < 400) {
        return panelW >= 200 ? 3 : 2;
    }
    if (panelW <= 240) {
        return 3;
    }
    return ccHomeCols(panelW);
}

inline int ccHomeRows(int cols) {
    return (kCcHomeTiles + cols - 1) / cols;
}

inline int ccBodyH(int panelH) {
    const int h = panelH - kCcHeaderH - kCcFooterH;
    return h > kCcMinHit ? h : kCcMinHit;
}

inline CcRect ccHeaderRect(int panelW) {
    return {0, 0, panelW, kCcHeaderH};
}

inline CcRect ccBackRect(int panelW) {
    const int w = panelW >= 200 ? 72 : 56;
    return {panelW - w, 0, w, kCcHeaderH};
}

inline CcRect ccFooterRect(int panelW, int panelH) {
    return {0, panelH - kCcFooterH, panelW, kCcFooterH};
}

inline CcRect ccSubBackRect(int panelW, int panelH) {
    return {0, panelH - kCcFooterH, panelW / 2, kCcFooterH};
}

inline CcRect ccSubResumeRect(int panelW, int panelH) {
    const int x = panelW / 2;
    return {x, panelH - kCcFooterH, panelW - x, kCcFooterH};
}

inline CcRect ccHomeTileRect(int panelW, int panelH, int index) {
    CcRect r{};
    if (index < 0 || index >= kCcHomeTiles) {
        return r;
    }
    const int cols = ccHomeColsFor(panelW, panelH);
    const int rows = ccHomeRows(cols);
    const int body = ccBodyH(panelH);
    int tileH = (body - kCcPad * (rows + 1)) / rows;
    if (tileH < kCcMinHit) {
        tileH = kCcMinHit;
    }
    const int tileW = (panelW - kCcPad * (cols + 1)) / cols;
    const int col = index % cols;
    const int row = index / cols;
    r.x = kCcPad + col * (tileW + kCcPad);
    r.y = kCcHeaderH + kCcPad + row * (tileH + kCcPad);
    r.w = tileW;
    r.h = tileH;
    return r;
}

inline int ccListRowH(int panelH) {
    const int avail = ccBodyH(panelH) - kCcPad;
    int h = avail / 4;
    if (h < kCcMinHit) {
        h = kCcMinHit;
    }
    if (h > 64) {
        h = 64;
    }
    return h;
}

inline CcRect ccListRowRect(int panelW, int panelH, int visualRow) {
    const int y0 = kCcHeaderH + kCcPad;
    const int rowH = ccListRowH(panelH);
    CcRect r;
    r.x = kCcPad;
    r.y = y0 + visualRow * rowH;
    r.w = panelW - 2 * kCcPad;
    r.h = rowH;
    return r;
}

inline int ccListVisibleRows(int panelH) {
    const int avail = ccBodyH(panelH) - kCcPad;
    const int rowH = ccListRowH(panelH);
    const int n = avail / rowH;
    return n > 0 ? n : 1;
}

/** Device rows when a prev/next pager occupies the last visible row. */
inline int ccListContentRows(int panelH, int itemCount) {
    const int vr = ccListVisibleRows(panelH);
    if (vr >= 2 && itemCount > vr - 1) {
        return vr - 1;
    }
    return vr;
}

inline CcRect ccListPagePrevRect(int panelW, int panelH) {
    const int vr = ccListVisibleRows(panelH);
    const CcRect row = ccListRowRect(panelW, panelH, vr > 0 ? vr - 1 : 0);
    return {row.x, row.y, row.w / 2, row.h};
}

inline CcRect ccListPageNextRect(int panelW, int panelH) {
    const int vr = ccListVisibleRows(panelH);
    const CcRect row = ccListRowRect(panelW, panelH, vr > 0 ? vr - 1 : 0);
    return {row.x + row.w / 2, row.y, row.w - row.w / 2, row.h};
}

/** Physical controller sample → panel pixels used by Control Center drawing. */
inline void mapTouchToPanel(uint16_t rawX, uint16_t rawY, int panelW, int panelH, bool rotateCw90,
                            uint16_t *outX, uint16_t *outY) {
    if (!outX || !outY) {
        return;
    }
    if (!rotateCw90) {
        *outX = rawX;
        *outY = rawY;
        if (panelW > 0 && *outX >= static_cast<uint16_t>(panelW)) {
            *outX = static_cast<uint16_t>(panelW - 1);
        }
        if (panelH > 0 && *outY >= static_cast<uint16_t>(panelH)) {
            *outY = static_cast<uint16_t>(panelH - 1);
        }
        return;
    }
    // Raw native portrait → logical landscape matching a CW-rotated UI.
    *outX = rawY;
    *outY = static_cast<uint16_t>(panelW > 0 ? panelW - 1 - rawX : 0);
}

struct CcPointer {
    CcGate gate = CcGate::Closed;
    int pressHit = static_cast<int>(CcHit::None);
    SystemTouchMode mode = SystemTouchMode::Menu;

    CcEvent onPressed(bool wakeConsumed) {
        if (wakeConsumed) {
            return CcEvent::None;
        }
        if (mode != SystemTouchMode::Menu) {
            return CcEvent::None;
        }
        if (gate == CcGate::Closed) {
            gate = CcGate::WaitRelease;
            pressHit = static_cast<int>(CcHit::None);
            return CcEvent::OpenMenu;
        }
        if (gate == CcGate::WaitRelease) {
            return CcEvent::None;
        }
        return CcEvent::None;
    }

    void onPressedHit(int hit) {
        if (gate == CcGate::Interactive) {
            pressHit = hit;
        }
    }

    CcEvent onReleased(int hit) {
        if (gate == CcGate::WaitRelease) {
            gate = CcGate::Interactive;
            pressHit = static_cast<int>(CcHit::None);
            return CcEvent::None;
        }
        if (gate == CcGate::Interactive) {
            const int start = pressHit;
            pressHit = static_cast<int>(CcHit::None);
            if (start != static_cast<int>(CcHit::None) && start == hit) {
                return CcEvent::Activate;
            }
        }
        return CcEvent::None;
    }

    void close() {
        gate = CcGate::Closed;
        pressHit = static_cast<int>(CcHit::None);
    }
};

inline CcHit ccHomeHit(int panelW, int panelH, int x, int y) {
    if (ccRectHit(ccFooterRect(panelW, panelH), x, y)) {
        return CcHit::Resume;
    }
    for (int i = 0; i < kCcHomeTiles; ++i) {
        if (ccRectHit(ccHomeTileRect(panelW, panelH, i), x, y)) {
            return static_cast<CcHit>(i);
        }
    }
    return CcHit::None;
}

} // namespace esp_bracket
