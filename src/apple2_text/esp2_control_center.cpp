#include "esp2_control_center.hpp"

#include "esp2_display_hal.h"
#include "esp_bracket/control_center.hpp"
#include "i18x_fw.hpp"

#include <Arduino.h>

#if ESP2_BLE_HID
#include "esp2_ble_hid.hpp"
#endif

#include <cstdio>
#include <cstring>

using esp_bracket::CcEvent;
using esp_bracket::CcHit;
using esp_bracket::CcPointer;
using esp_bracket::CcRect;
using esp_bracket::ccBackRect;
using esp_bracket::ccBodyH;
using esp_bracket::ccFooterRect;
using esp_bracket::ccSubBackRect;
using esp_bracket::ccSubResumeRect;
using esp_bracket::ccHomeHit;
using esp_bracket::ccHomeTileRect;
using esp_bracket::ccListContentRows;
using esp_bracket::ccListPageNextRect;
using esp_bracket::ccListPagePrevRect;
using esp_bracket::ccListRowRect;
using esp_bracket::ccListVisibleRows;
using esp_bracket::ccRectHit;
using esp_bracket::ccToUiLocal;
using esp_bracket::ccTouchInUiViewport;
using esp_bracket::CcLayoutContext;
using esp_bracket::CcUiViewport;
using esp_bracket::kCcFooterH;
using esp_bracket::kCcHeaderH;
using esp_bracket::kCcHomeTiles;
using esp_bracket::kCcMinHit;
using esp_bracket::kCcPad;

namespace esp2_cc_svc {
namespace {
Services g_svc{};
}

void setServices(const Services &s) { g_svc = s; }
const Services &services() { return g_svc; }
} // namespace esp2_cc_svc

namespace esp2_cc {
namespace {

enum class Screen : uint8_t {
    Home = 0,
    Display,
    Disks,
    DiskBrowse,
    Input,
    JoyTest,
    BluetoothHub,
    BleScan,
    BleKnown,
    BleDetail,
    BleForget,
    Storage,
    StorageShareConfirm,
    Settings,
    Reset,
    Confirm,
};

enum class ConfirmKind : uint8_t { None = 0, Cold, RestartEsp };

static bool g_open = false;
static Screen g_screen = Screen::Home;
static CcPointer g_ptr{};
static ConfirmKind g_confirm = ConfirmKind::None;
static const char *g_error = nullptr;
static constexpr int kMaxDiskFiles = 128;
static char g_diskFiles[kMaxDiskFiles][32]{};
static int g_diskFileCount = 0;
static char g_bleAddr[20]{};
static int g_pressedHit = -1;
static int g_downX = -1;
static int g_downY = -1;
static uint32_t g_blockOpenUntil = 0;
static uint32_t g_pairUntil = 0;
static bool g_pairPending = false;

static constexpr uint16_t kBg = 0x0000;
static constexpr uint16_t kFg = 0xFFFF;
static constexpr uint16_t kAccent = 0x07FF;
static constexpr uint16_t kSel = 0x39E7;
static constexpr uint16_t kDim = 0xBDF7;
static constexpr uint16_t kCard = 0x2104;
static constexpr uint16_t kPress = 0x4A69;
static constexpr uint16_t kHeader = 0x10A2;
static constexpr uint16_t kBackFill = 0x3186;
static constexpr uint16_t kResumeFill = 0x0011;
static bool g_uiDirty = true;
static CcLayoutContext g_layout{};
static int g_listScroll = 0;
static constexpr int kMaxFocus = 24;
static int g_focusHits[kMaxFocus]{};
static int g_focusCount = 0;
static int g_focusIdx = 0;
static bool isFocused(CcHit h);
static CcHit focusedHit();
static void rebuildFocusList();

#if ESP2_BLE_HID
const char *bleKindKey(esp2_ble::ScanDeviceKind k) {
    switch (k) {
    case esp2_ble::ScanDeviceKind::Keyboard:
        return "ble.kind.keyboard";
    case esp2_ble::ScanDeviceKind::Gamepad:
        return "ble.kind.gamepad";
    case esp2_ble::ScanDeviceKind::Composite:
        return "ble.kind.composite";
    default:
        return "ble.kind.hid";
    }
}

void formatBleLine(char *out, size_t cap, const char *name, const char *addr,
                   esp2_ble::ScanDeviceKind kind) {
    const bool useful = name && name[0] && std::strcmp(name, "(unnamed)") != 0;
    if (useful) {
        std::snprintf(out, cap, "%s", name);
        return;
    }
    const char *tail = addr && addr[0] ? addr : "";
    const size_t alen = std::strlen(tail);
    if (alen >= 5) {
        tail += alen - 5;
    }
    std::snprintf(out, cap, "%s %s", esp2_i18x::t(bleKindKey(kind)), tail);
}

esp2_ble::ScanDeviceKind kindFromKnownType(esp2_ble_config::DeviceType type) {
    if (type == esp2_ble_config::DeviceType::Keyboard) {
        return esp2_ble::ScanDeviceKind::Keyboard;
    }
    if (type == esp2_ble_config::DeviceType::Gamepad) {
        return esp2_ble::ScanDeviceKind::Gamepad;
    }
    return esp2_ble::ScanDeviceKind::OtherHid;
}
#endif

static CcUiViewport activeUi(int panelW, int panelH) {
    if (g_layout.ui.w > 0 && g_layout.ui.h > 0) {
        return g_layout.ui;
    }
    return {0, 0, panelW, panelH};
}

static bool g_panelCleared = false;
static Screen g_drawnScreen = Screen::Home;
static bool g_clearUi = true;

static void markUi() { g_uiDirty = true; }

static void markUiFull() {
    g_uiDirty = true;
    g_clearUi = true;
}

static void setErr(const char *key) {
    g_error = key;
    markUi();
}

static void drawLabelSz(Arduino_GFX *gfx, int x, int y, uint16_t color, const char *s, uint8_t sz,
                        uint16_t bg = kBg) {
    // Do not remap 0x0000 → white: mono inverted focus uses black ink on paper.
    gfx->setTextColor(color, bg);
    gfx->setTextSize(sz);
    gfx->setCursor(x, y);
    gfx->print(s ? s : "");
}

static void drawLabel(Arduino_GFX *gfx, int x, int y, uint16_t color, const char *s) {
    drawLabelSz(gfx, x, y, color, s, 1);
}

static void fillRoundish(Arduino_GFX *gfx, const CcRect &r, uint16_t fill) {
    gfx->fillRect(r.x, r.y, r.w, r.h, fill);
    gfx->drawRect(r.x, r.y, r.w, r.h, kFg);
}

static void blitBits16(Arduino_GFX *gfx, int x0, int y0, int px, uint16_t ink, const uint16_t *rows,
                       int nrows) {
    for (int y = 0; y < nrows; ++y) {
        const uint16_t bits = rows[y];
        for (int x = 0; x < 16; ++x) {
            if ((bits & static_cast<uint16_t>(0x8000u >> x)) != 0) {
                gfx->fillRect(x0 + x * px, y0 + y * px, px, px, ink);
            }
        }
    }
}

static void drawHomeIcon(Arduino_GFX *gfx, const CcRect &tile, int index, uint16_t ink,
                         uint16_t hole) {
    const int cx = tile.x + tile.w / 2;
    const int cy = tile.y + tile.h / 2;
    int s = tile.w < tile.h ? tile.w : tile.h;
    s -= 10;
    if (s < 40) {
        s = 40;
    }
    const int r = s / 2 - 2;
    switch (index) {
    case 0: { // monitor
        const int w = r * 2;
        const int h = r + r / 3;
        gfx->fillRect(cx - w / 2, cy - h / 2, w, h, ink);
        gfx->fillRect(cx - w / 2 + 6, cy - h / 2 + 6, w - 12, h - 16, hole);
        gfx->fillRect(cx - 6, cy + h / 2 - 4, 12, 8, ink);
        gfx->fillRect(cx - w / 3, cy + h / 2 + 4, w * 2 / 3, 6, ink);
        break;
    }
    case 1: { // disk
        gfx->fillCircle(cx, cy, r, ink);
        gfx->fillCircle(cx, cy, r / 3, hole);
        gfx->fillRect(cx - 5, cy - r, 10, r / 2, ink);
        break;
    }
    case 2: { // input / pad
        gfx->fillRect(cx - r, cy - r / 2, r * 2, r + 4, ink);
        gfx->fillCircle(cx - r / 2, cy, r / 3, hole);
        gfx->fillCircle(cx + r / 3, cy - r / 6, r / 5, hole);
        gfx->fillCircle(cx + r / 2 + 4, cy + r / 6, r / 5, hole);
        break;
    }
    case 3: { // Bluetooth rune as bitmap (fillRect — drawLine is a no-op on this panel)
        static const uint16_t kBtLogo[] = {
            0x0180, 0x01C0, 0x01A0, 0x0190, 0x0188, 0x218C, 0x3198, 0x1BF0, 0x0FE0, 0x07C0,
            0x0FE0, 0x1BF0, 0x3198, 0x218C, 0x0188, 0x0190, 0x01A0, 0x01C0, 0x0180,
        };
        const int px = s >= 56 ? 2 : 2;
        const int bw = 16 * px;
        const int bh = 19 * px;
        blitBits16(gfx, cx - bw / 2, cy - bh / 2, px, ink, kBtLogo, 19);
        (void)hole;
        break;
    }
    case 4: { // storage
        gfx->fillRect(cx - r, cy - r, r * 2, r * 2, ink);
        gfx->fillTriangle(cx - r, cy - r, cx - r + r / 2, cy - r, cx - r, cy - r + r / 2, hole);
        gfx->fillRect(cx - r / 2, cy - r / 4, r, 6, hole);
        gfx->fillRect(cx - r / 2, cy + 4, r, 6, hole);
        gfx->fillRect(cx - r / 2, cy + r / 3 + 4, r, 6, hole);
        break;
    }
    case 5: { // settings
        gfx->fillCircle(cx, cy, r, ink);
        gfx->fillCircle(cx, cy, r / 3, hole);
        gfx->fillRect(cx - 6, cy - r, 12, r / 2, ink);
        gfx->fillRect(cx - 6, cy + r / 2, 12, r / 2, ink);
        gfx->fillRect(cx - r, cy - 6, r / 2, 12, ink);
        gfx->fillRect(cx + r / 2, cy - 6, r / 2, 12, ink);
        break;
    }
    case 6: { // reset
        gfx->fillCircle(cx, cy, r, ink);
        gfx->fillCircle(cx, cy, r - 10, hole);
        gfx->fillTriangle(cx + r / 3, cy - r, cx + r, cy - r / 3, cx + r / 4, cy - r / 4, ink);
        gfx->fillRect(cx - 6, cy - r / 4, 12, r / 2, ink);
        break;
    }
    default:
        break;
    }
}

static void drawHeader(Arduino_GFX *gfx, const CcUiViewport &ui, const char *title, bool back) {
    gfx->fillRect(ui.x, ui.y, ui.w, kCcHeaderH, kHeader);
    gfx->drawFastHLine(ui.x, ui.y + kCcHeaderH - 1, ui.w, kFg);
    drawLabelSz(gfx, ui.x + 8, ui.y + 12, kFg, title, 2, kHeader);
    if (back) {
        const CcRect b = ccBackRect(ui.w);
        gfx->fillRect(ui.x + b.x, ui.y + b.y + 6, b.w - 4, b.h - 12, kBackFill);
        gfx->drawRect(ui.x + b.x, ui.y + b.y + 6, b.w - 4, b.h - 12, kFg);
        drawLabelSz(gfx, ui.x + b.x + 6, ui.y + b.y + 14, kFg, esp2_i18x::t("cc.back"), 1,
                    kBackFill);
    }
}

static void drawFooterResume(Arduino_GFX *gfx, const CcUiViewport &ui, bool pressed) {
    const CcRect f = ccFooterRect(ui.w, ui.h);
#if defined(ESP2_PLATFORM_WAVESHARE_RLCD42)
    const uint16_t fill = pressed ? kFg : 0x0000;
    const uint16_t ink = pressed ? 0x0000 : kFg;
#else
    const uint16_t fill = pressed ? kPress : kResumeFill;
    const uint16_t ink = kFg;
#endif
    gfx->fillRect(ui.x + f.x, ui.y + f.y, f.w, f.h, fill);
    gfx->drawRect(ui.x + f.x, ui.y + f.y, f.w, f.h, ink);
    drawLabelSz(gfx, ui.x + f.x + 12, ui.y + f.y + 16, ink, esp2_i18x::t("cc.resume_short"), 2, fill);
}

static void drawFooterBackResume(Arduino_GFX *gfx, const CcUiViewport &ui) {
    const CcRect b = ccSubBackRect(ui.w, ui.h);
    const CcRect r = ccSubResumeRect(ui.w, ui.h);
    const bool focBack = isFocused(CcHit::Back);
    const bool focRes = isFocused(CcHit::Resume);
#if defined(ESP2_PLATFORM_WAVESHARE_RLCD42)
    const uint16_t backFill = focBack ? kFg : 0x0000;
    const uint16_t resFill = focRes ? kFg : 0x0000;
    const uint16_t backInk = focBack ? 0x0000 : kFg;
    const uint16_t resInk = focRes ? 0x0000 : kFg;
#else
    const uint16_t backFill = focBack ? kPress : kBackFill;
    const uint16_t resFill = focRes ? kPress : kResumeFill;
    const uint16_t backInk = kFg;
    const uint16_t resInk = kFg;
#endif
    gfx->fillRect(ui.x + b.x, ui.y + b.y, b.w, b.h, backFill);
    gfx->drawRect(ui.x + b.x, ui.y + b.y, b.w, b.h, backInk);
    gfx->fillRect(ui.x + r.x, ui.y + r.y, r.w, r.h, resFill);
    gfx->drawRect(ui.x + r.x, ui.y + r.y, r.w, r.h, resInk);
    drawLabelSz(gfx, ui.x + b.x + 12, ui.y + b.y + 16, backInk, esp2_i18x::t("cc.back"), 2, backFill);
    drawLabelSz(gfx, ui.x + r.x + 12, ui.y + r.y + 16, resInk, esp2_i18x::t("cc.resume_short"), 2,
                resFill);
}

static void drawError(Arduino_GFX *gfx, const CcUiViewport &ui) {
    if (!g_error) {
        return;
    }
    gfx->setTextColor(0xF800, kBg);
    gfx->setCursor(ui.x + 8, ui.y + ui.h - kCcFooterH - 14);
    gfx->print(esp2_i18x::t(g_error));
}

static const char *homeKey(int i) {
    switch (i) {
    case 0:
        return "cc.display";
    case 1:
        return "cc.disks";
    case 2:
        return "cc.input";
    case 3:
        return "cc.bluetooth";
    case 4:
        return "cc.storage";
    case 5:
        return "cc.settings";
    case 6:
        return "cc.reset";
    default:
        return "";
    }
}

static bool bleOn() { return esp2_cc_svc::services().bleAvailable; }

static void pauseOpen() {
    if (esp2_cc_svc::services().pauseEmu) {
        esp2_cc_svc::services().pauseEmu();
    }
}

static void dismissChrome() {
#if ESP2_BLE_HID
    esp2_ble::cancelScanAsync();
#endif
    g_ptr.close();
    g_open = false;
    g_screen = Screen::Home;
    g_error = nullptr;
    g_confirm = ConfirmKind::None;
    g_pressedHit = -1;
    g_listScroll = 0;
#if ESP2_BLE_HID
    esp2_ble::setHidMonitor(false);
#endif
}

static void resumeClose() {
    dismissChrome();
    g_blockOpenUntil = millis() + 300;
    markUi();
    if (esp2_cc_svc::services().resumeEmu) {
        esp2_cc_svc::services().resumeEmu();
    }
}

static void goHome() {
    g_screen = Screen::Home;
    g_error = nullptr;
    g_listScroll = 0;
    g_focusIdx = 0;
    rebuildFocusList();
    markUiFull();
}

static int hitToInt(CcHit h) { return static_cast<int>(h); }

static CcHit hitHome(int panelW, int panelH, int x, int y) {
    return ccHomeHit(panelW, panelH, x, y);
}

static CcHit hitBackOrFooter(int panelW, int panelH, int x, int y, bool footerIsResume) {
    if (ccRectHit(ccBackRect(panelW), x, y) || ccRectHit(ccSubBackRect(panelW, panelH), x, y)) {
        return CcHit::Back;
    }
    if (footerIsResume && ccRectHit(ccSubResumeRect(panelW, panelH), x, y)) {
        return CcHit::Resume;
    }
    return CcHit::None;
}

static CcRect hubActionRect(int panelW, int panelH, int which) {
    const int y0 = kCcHeaderH + kCcPad;
    const int avail = ccBodyH(panelH) - kCcPad;
    int hh = avail / 2;
    if (hh < kCcMinHit) {
        hh = kCcMinHit;
    }
    CcRect r;
    r.x = kCcPad;
    r.y = y0 + which * hh;
    r.w = panelW - 2 * kCcPad;
    r.h = hh - kCcPad;
    if (r.h < kCcMinHit) {
        r.h = kCcMinHit;
    }
    return r;
}

/** Display-menu entries: full color boards vs mono RLCD. */
static int displayOptionCount() {
#if defined(ESP2_PLATFORM_WAVESHARE_RLCD42)
    return 8; // Classic, Landscape, 1:1, Fit, Clean, CRT, Normal, Invert
#else
    return 8;
#endif
}

static CcHit displayOptionHit(int idx) {
#if defined(ESP2_PLATFORM_WAVESHARE_RLCD42)
    static const CcHit kHits[] = {CcHit::OrientClassic, CcHit::OrientLandscape, CcHit::ScaleInteger,
                                  CcHit::ScaleFull,     CcHit::EffectClean,     CcHit::EffectCrt,
                                  CcHit::MonoNormal,    CcHit::MonoInvert};
    if (idx < 0 || idx >= 8) {
        return CcHit::None;
    }
    return kHits[idx];
#else
    static const CcHit kHits[] = {
        CcHit::OrientClassic, CcHit::OrientLandscape, CcHit::MonitorWhite, CcHit::MonitorGreen,
        CcHit::MonitorAmber,  CcHit::MonitorArtifact, CcHit::EffectClean,  CcHit::EffectCrt};
    if (idx < 0 || idx >= 8) {
        return CcHit::None;
    }
    return kHits[idx];
#endif
}

static const char *displayOptionLabel(int idx) {
#if defined(ESP2_PLATFORM_WAVESHARE_RLCD42)
    switch (idx) {
    case 0:
        return esp2_i18x::t("cc.orient.classic");
    case 1:
        return esp2_i18x::t("cc.orient.landscape");
    case 2:
        return esp2_i18x::t("cc.scale.integer");
    case 3:
        return esp2_i18x::t("cc.scale.full");
    case 4:
        return esp2_i18x::t("cc.effect.clean");
    case 5:
        return esp2_i18x::t("cc.effect.crt");
    case 6:
        return esp2_i18x::t("cc.invert.normal");
    case 7:
        return esp2_i18x::t("cc.invert.invert");
    default:
        return "";
    }
#else
    switch (idx) {
    case 0:
        return esp2_i18x::t("cc.orient.classic");
    case 1:
        return esp2_i18x::t("cc.orient.landscape");
    case 2:
        return esp2_i18x::t("cc.monitor.white");
    case 3:
        return esp2_i18x::t("cc.monitor.green");
    case 4:
        return esp2_i18x::t("cc.monitor.amber");
    case 5:
        return esp2_i18x::t("cc.monitor.artifact");
    case 6:
        return esp2_i18x::t("cc.effect.clean");
    case 7:
        return esp2_i18x::t("cc.effect.crt");
    default:
        return "";
    }
#endif
}

static bool displayOptionOn(int idx, const esp2_cc_svc::PresentSnap &p) {
#if defined(ESP2_PLATFORM_WAVESHARE_RLCD42)
    switch (idx) {
    case 0:
        return p.orient == esp2_cc_svc::Orient::Classic;
    case 1:
        return p.orient == esp2_cc_svc::Orient::Landscape;
    case 2:
        return !esp2DisplayClassicFitFull();
    case 3:
        return esp2DisplayClassicFitFull();
    case 4:
        return p.effect == esp2_cc_svc::Effect::Clean;
    case 5:
        return p.effect == esp2_cc_svc::Effect::Crt;
    case 6:
        return !esp2DisplayInvert();
    case 7:
        return esp2DisplayInvert();
    default:
        return false;
    }
#else
    switch (idx) {
    case 0:
        return p.orient == esp2_cc_svc::Orient::Classic;
    case 1:
        return p.orient == esp2_cc_svc::Orient::Landscape;
    case 2:
        return p.monitor == esp2_cc_svc::Monitor::White;
    case 3:
        return p.monitor == esp2_cc_svc::Monitor::Green;
    case 4:
        return p.monitor == esp2_cc_svc::Monitor::Amber;
    case 5:
        return p.monitor == esp2_cc_svc::Monitor::Artifact;
    case 6:
        return p.effect == esp2_cc_svc::Effect::Clean;
    case 7:
        return p.effect == esp2_cc_svc::Effect::Crt;
    default:
        return false;
    }
#endif
}

static int currentListCount() {
    switch (g_screen) {
    case Screen::Display:
        return displayOptionCount();
    case Screen::DiskBrowse:
        return g_diskFileCount;
#if ESP2_BLE_HID
    case Screen::BleScan:
        return static_cast<int>(esp2_ble::discoveredCount());
    case Screen::BleKnown:
        return static_cast<int>(esp2_ble::config().knownCount);
#endif
    default:
        return 0;
    }
}

static void pushFocus(CcHit h) {
    if (g_focusCount >= kMaxFocus) {
        return;
    }
    g_focusHits[g_focusCount++] = hitToInt(h);
}

static CcHit focusedHit() {
    if (g_focusCount <= 0 || g_focusIdx < 0 || g_focusIdx >= g_focusCount) {
        return CcHit::None;
    }
    return static_cast<CcHit>(g_focusHits[g_focusIdx]);
}

static bool isFocused(CcHit h) { return focusedHit() == h; }

static void rebuildFocusList() {
    g_focusCount = 0;
    const int uh = g_layout.ui.h > 0 ? g_layout.ui.h : 280;
    switch (g_screen) {
    case Screen::Home:
        for (int i = 0; i < kCcHomeTiles; ++i) {
            pushFocus(static_cast<CcHit>(i));
        }
        pushFocus(CcHit::Resume);
        break;
    case Screen::Display: {
        const int n = displayOptionCount();
        const int slots = ccListContentRows(uh, n);
        for (int i = 0; i < slots && g_listScroll + i < n; ++i) {
            pushFocus(displayOptionHit(g_listScroll + i));
        }
        if (g_listScroll > 0) {
            pushFocus(CcHit::ListPrev);
        }
        if (g_listScroll + slots < n) {
            pushFocus(CcHit::ListNext);
        }
        pushFocus(CcHit::Back);
        pushFocus(CcHit::Resume);
        break;
    }
    case Screen::Disks:
        pushFocus(CcHit::DiskBrowse);
        pushFocus(CcHit::DiskEject);
        pushFocus(CcHit::Back);
        pushFocus(CcHit::Resume);
        break;
    case Screen::DiskBrowse: {
        const int n = g_diskFileCount;
        const int slots = ccListContentRows(uh, n);
        for (int i = 0; i < slots && g_listScroll + i < n; ++i) {
            pushFocus(static_cast<CcHit>(hitToInt(CcHit::DiskFile0) + g_listScroll + i));
        }
        if (g_listScroll > 0) {
            pushFocus(CcHit::ListPrev);
        }
        if (g_listScroll + slots < n) {
            pushFocus(CcHit::ListNext);
        }
        pushFocus(CcHit::Back);
        pushFocus(CcHit::Resume);
        break;
    }
    case Screen::Input:
        pushFocus(CcHit::JoyTest);
        pushFocus(CcHit::Back);
        pushFocus(CcHit::Resume);
        break;
    case Screen::JoyTest:
        pushFocus(CcHit::Back);
        pushFocus(CcHit::Resume);
        break;
    case Screen::BluetoothHub:
        pushFocus(CcHit::BleAdd);
        pushFocus(CcHit::BleList);
        pushFocus(CcHit::Back);
        pushFocus(CcHit::Resume);
        break;
    case Screen::Storage:
        pushFocus(CcHit::StorageShare);
        pushFocus(CcHit::StorageReturn);
        pushFocus(CcHit::Back);
        pushFocus(CcHit::Resume);
        break;
    case Screen::Settings:
        pushFocus(CcHit::LocaleEn);
        pushFocus(CcHit::LocaleDe);
        pushFocus(CcHit::Back);
        pushFocus(CcHit::Resume);
        break;
    case Screen::Reset:
        pushFocus(CcHit::ResetApple);
        pushFocus(CcHit::ColdStart);
        pushFocus(CcHit::RestartEsp);
        pushFocus(CcHit::Back);
        pushFocus(CcHit::Resume);
        break;
    case Screen::Confirm:
    case Screen::StorageShareConfirm:
        pushFocus(g_screen == Screen::StorageShareConfirm ? CcHit::StorageShareYes : CcHit::ConfirmYes);
        pushFocus(g_screen == Screen::StorageShareConfirm ? CcHit::StorageShareNo : CcHit::ConfirmNo);
        break;
    case Screen::BleForget:
        pushFocus(CcHit::BleForgetYes);
        pushFocus(CcHit::BleForgetNo);
        break;
    case Screen::BleDetail:
        pushFocus(CcHit::BleConnect);
        pushFocus(CcHit::BleDisconnect);
        pushFocus(CcHit::BleForget);
        pushFocus(CcHit::Back);
        pushFocus(CcHit::Resume);
        break;
#if ESP2_BLE_HID
    case Screen::BleScan: {
        const int n = static_cast<int>(esp2_ble::discoveredCount());
        const int slots = ccListContentRows(uh, n);
        for (int i = 0; i < slots && g_listScroll + i < n; ++i) {
            pushFocus(static_cast<CcHit>(hitToInt(CcHit::BleScanItem0) + g_listScroll + i));
        }
        if (g_listScroll > 0) {
            pushFocus(CcHit::ListPrev);
        }
        if (g_listScroll + slots < n) {
            pushFocus(CcHit::ListNext);
        }
        pushFocus(CcHit::Back);
        pushFocus(CcHit::Resume);
        break;
    }
    case Screen::BleKnown: {
        const int n = static_cast<int>(esp2_ble::config().knownCount);
        const int slots = ccListContentRows(uh, n);
        for (int i = 0; i < slots && g_listScroll + i < n; ++i) {
            pushFocus(static_cast<CcHit>(hitToInt(CcHit::BleKnown0) + g_listScroll + i));
        }
        if (g_listScroll > 0) {
            pushFocus(CcHit::ListPrev);
        }
        if (g_listScroll + slots < n) {
            pushFocus(CcHit::ListNext);
        }
        pushFocus(CcHit::Back);
        pushFocus(CcHit::Resume);
        break;
    }
#endif
    default:
        pushFocus(CcHit::Back);
        pushFocus(CcHit::Resume);
        break;
    }
    if (g_focusCount <= 0) {
        g_focusIdx = 0;
    } else if (g_focusIdx >= g_focusCount) {
        g_focusIdx = g_focusCount - 1;
    }
}

static void drawPager(Arduino_GFX *gfx, const CcUiViewport &ui, int itemCount) {
    const int slots = ccListContentRows(ui.h, itemCount);
    if (itemCount <= slots) {
        return;
    }
    const CcRect p = ccListPagePrevRect(ui.w, ui.h);
    const CcRect n = ccListPageNextRect(ui.w, ui.h);
    gfx->fillRect(ui.x + p.x, ui.y + p.y, p.w, p.h, kBackFill);
    gfx->drawRect(ui.x + p.x, ui.y + p.y, p.w, p.h, kFg);
    gfx->fillRect(ui.x + n.x, ui.y + n.y, n.w, n.h, kResumeFill);
    gfx->drawRect(ui.x + n.x, ui.y + n.y, n.w, n.h, kFg);
    drawLabelSz(gfx, ui.x + p.x + 10, ui.y + p.y + p.h / 2 - 8, kFg, esp2_i18x::t("cc.page_prev"), 2,
                kBackFill);
    drawLabelSz(gfx, ui.x + n.x + 10, ui.y + n.y + n.h / 2 - 8, kFg, esp2_i18x::t("cc.page_next"), 2,
                kResumeFill);
}

static CcHit hitPager(int panelW, int panelH, int x, int y, int itemCount) {
    if (itemCount <= ccListContentRows(panelH, itemCount)) {
        return CcHit::None;
    }
    if (ccRectHit(ccListPagePrevRect(panelW, panelH), x, y)) {
        return CcHit::ListPrev;
    }
    if (ccRectHit(ccListPageNextRect(panelW, panelH), x, y)) {
        return CcHit::ListNext;
    }
    return CcHit::None;
}

static CcHit hitListSlots(int panelW, int panelH, int x, int y, int count, int baseId, int scroll) {
    const int slots = ccListContentRows(panelH, count);
    for (int i = 0; i < slots; ++i) {
        const int idx = scroll + i;
        if (idx >= count) {
            break;
        }
        if (ccRectHit(ccListRowRect(panelW, panelH, i), x, y)) {
            return static_cast<CcHit>(baseId + i);
        }
    }
    return CcHit::None;
}

static CcHit hitList(int panelW, int panelH, int x, int y, int count, int baseId, int scroll) {
    const int slots = ccListContentRows(panelH, count);
    for (int i = 0; i < slots; ++i) {
        const int idx = scroll + i;
        if (idx >= count) {
            break;
        }
        if (ccRectHit(ccListRowRect(panelW, panelH, i), x, y)) {
            return static_cast<CcHit>(baseId + idx);
        }
    }
    return CcHit::None;
}

static CcHit hitTest(int panelW, int panelH, int x, int y) {
    switch (g_screen) {
    case Screen::Home:
        return hitHome(panelW, panelH, x, y);
    case Screen::Display: {
        CcHit h = hitBackOrFooter(panelW, panelH, x, y, true);
        if (h != CcHit::None) {
            return h;
        }
        const int n = displayOptionCount();
        CcHit pg = hitPager(panelW, panelH, x, y, n);
        if (pg != CcHit::None) {
            return pg;
        }
        const int slots = ccListContentRows(panelH, n);
        for (int i = 0; i < slots; ++i) {
            const int idx = g_listScroll + i;
            if (idx < n && ccRectHit(ccListRowRect(panelW, panelH, i), x, y)) {
                return displayOptionHit(idx);
            }
        }
        return CcHit::None;
    }
    case Screen::Disks: {
        CcHit h = hitBackOrFooter(panelW, panelH, x, y, true);
        if (h != CcHit::None) {
            return h;
        }
        if (ccRectHit(ccListRowRect(panelW, panelH, 2), x, y)) {
            return CcHit::DiskBrowse;
        }
        if (ccRectHit(ccListRowRect(panelW, panelH, 3), x, y)) {
            return CcHit::DiskEject;
        }
        return CcHit::None;
    }
    case Screen::DiskBrowse: {
        CcHit h = hitBackOrFooter(panelW, panelH, x, y, true);
        if (h != CcHit::None) {
            return h;
        }
        return hitListSlots(panelW, panelH, x, y, g_diskFileCount, hitToInt(CcHit::DiskFile0),
                       g_listScroll);
    }
    case Screen::Input: {
        CcHit h = hitBackOrFooter(panelW, panelH, x, y, true);
        if (h != CcHit::None) {
            return h;
        }
        if (ccRectHit(ccListRowRect(panelW, panelH, 0), x, y)) {
            return CcHit::JoyTest;
        }
        return CcHit::None;
    }
    case Screen::JoyTest:
        return hitBackOrFooter(panelW, panelH, x, y, true);
    case Screen::Settings:
    case Screen::Storage:
        return hitBackOrFooter(panelW, panelH, x, y, true) != CcHit::None
                   ? hitBackOrFooter(panelW, panelH, x, y, true)
                   : (g_screen == Screen::Storage
                          ? (ccRectHit(ccListRowRect(panelW, panelH, 3), x, y) ? CcHit::StorageShare
                             : ccRectHit(ccListRowRect(panelW, panelH, 4), x, y)
                                 ? CcHit::StorageReturn
                                 : CcHit::None)
                          : (g_screen == Screen::Settings
                                 ? (ccRectHit(ccListRowRect(panelW, panelH, 0), x, y)
                                        ? CcHit::LocaleEn
                                        : (ccRectHit(ccListRowRect(panelW, panelH, 1), x, y)
                                               ? CcHit::LocaleDe
                                               : CcHit::None))
                                 : CcHit::None));
    case Screen::BluetoothHub: {
        CcHit h = hitBackOrFooter(panelW, panelH, x, y, true);
        if (h != CcHit::None) {
            return h;
        }
        if (ccRectHit(hubActionRect(panelW, panelH, 0), x, y)) {
            return CcHit::BleAdd;
        }
        if (ccRectHit(hubActionRect(panelW, panelH, 1), x, y)) {
            return CcHit::BleList;
        }
        return CcHit::None;
    }
#if ESP2_BLE_HID
    case Screen::BleScan: {
        CcHit h = hitBackOrFooter(panelW, panelH, x, y, true);
        if (h != CcHit::None) {
            return h;
        }
        const int n = static_cast<int>(esp2_ble::discoveredCount());
        CcHit pg = hitPager(panelW, panelH, x, y, n);
        if (pg != CcHit::None) {
            return pg;
        }
        const int slots = ccListContentRows(panelH, n);
        for (int i = 0; i < slots; ++i) {
            const int idx = g_listScroll + i;
            if (idx >= n) {
                break;
            }
            if (ccRectHit(ccListRowRect(panelW, panelH, i), x, y)) {
                return static_cast<CcHit>(hitToInt(CcHit::BleScanItem0) + idx);
            }
        }
        return CcHit::None;
    }
    case Screen::BleKnown: {
        CcHit h = hitBackOrFooter(panelW, panelH, x, y, true);
        if (h != CcHit::None) {
            return h;
        }
        const int n = static_cast<int>(esp2_ble::config().knownCount);
        CcHit pg = hitPager(panelW, panelH, x, y, n);
        if (pg != CcHit::None) {
            return pg;
        }
        return hitList(panelW, panelH, x, y, n, hitToInt(CcHit::BleKnown0), g_listScroll);
    }
    case Screen::BleDetail: {
        CcHit h = hitBackOrFooter(panelW, panelH, x, y, true);
        if (h != CcHit::None) {
            return h;
        }
        if (ccRectHit(ccListRowRect(panelW, panelH, 1), x, y)) {
            return CcHit::BleConnect;
        }
        if (ccRectHit(ccListRowRect(panelW, panelH, 2), x, y)) {
            return CcHit::BleDisconnect;
        }
        if (ccRectHit(ccListRowRect(panelW, panelH, 3), x, y)) {
            return CcHit::BleForget;
        }
        return CcHit::None;
    }
    case Screen::BleForget: {
        if (ccRectHit(ccListRowRect(panelW, panelH, 0), x, y)) {
            return CcHit::BleForgetYes;
        }
        if (ccRectHit(ccListRowRect(panelW, panelH, 1), x, y)) {
            return CcHit::BleForgetNo;
        }
        return hitBackOrFooter(panelW, panelH, x, y, true);
    }
#endif
    case Screen::Reset: {
        CcHit h = hitBackOrFooter(panelW, panelH, x, y, true);
        if (h != CcHit::None) {
            return h;
        }
        if (ccRectHit(ccListRowRect(panelW, panelH, 0), x, y)) {
            return CcHit::ResetApple;
        }
        if (ccRectHit(ccListRowRect(panelW, panelH, 1), x, y)) {
            return CcHit::ColdStart;
        }
        if (ccRectHit(ccListRowRect(panelW, panelH, 2), x, y)) {
            return CcHit::RestartEsp;
        }
        return CcHit::None;
    }
    case Screen::StorageShareConfirm:
    case Screen::Confirm: {
        if (ccRectHit(ccListRowRect(panelW, panelH, 0), x, y)) {
            return g_screen == Screen::StorageShareConfirm ? CcHit::StorageShareYes : CcHit::ConfirmYes;
        }
        if (ccRectHit(ccListRowRect(panelW, panelH, 1), x, y)) {
            return g_screen == Screen::StorageShareConfirm ? CcHit::StorageShareNo : CcHit::ConfirmNo;
        }
        return CcHit::None;
    }
    default:
        return CcHit::None;
    }
}

static void consumeUntilRelease() {
    g_ptr.gate = esp_bracket::CcGate::WaitRelease;
    g_ptr.pressHit = static_cast<int>(CcHit::None);
    g_pressedHit = -1;
}

static void activate(CcHit hit) {
    markUiFull();
    if (hit != CcHit::ListPrev && hit != CcHit::ListNext) {
        g_listScroll = 0;
    }
    struct NavConsume {
        Screen before;
        ~NavConsume() {
            if (g_open && g_screen != before) {
                consumeUntilRelease();
            }
        }
    } nav{g_screen};
    const auto &svc = esp2_cc_svc::services();
    switch (hit) {
    case CcHit::Resume:
    case CcHit::Back:
        if (g_screen == Screen::Home || hit == CcHit::Resume) {
            resumeClose();
            return;
        }
        if (g_screen == Screen::JoyTest) {
#if ESP2_BLE_HID
            esp2_ble::setHidMonitor(false);
#endif
            g_screen = Screen::Input;
            return;
        }
        if (g_screen == Screen::DiskBrowse) {
            g_screen = Screen::Disks;
            return;
        }
        if (g_screen == Screen::BleScan || g_screen == Screen::BleKnown) {
#if ESP2_BLE_HID
            if (g_screen == Screen::BleScan) {
                esp2_ble::cancelScanAsync();
            }
#endif
            g_screen = Screen::BluetoothHub;
            return;
        }
        if (g_screen == Screen::BleDetail) {
            g_screen = Screen::BleKnown;
            return;
        }
        if (g_screen == Screen::BleForget) {
            g_screen = Screen::BleDetail;
            return;
        }
        goHome();
        return;
    case CcHit::ListPrev: {
        const int n = currentListCount();
        const int uh = g_layout.ui.h > 0 ? g_layout.ui.h : 280;
        const int slots = ccListContentRows(uh, n);
        g_listScroll -= slots > 0 ? slots : 1;
        if (g_listScroll < 0) {
            g_listScroll = 0;
        }
        return;
    }
    case CcHit::ListNext: {
        const int n = currentListCount();
        const int uh = g_layout.ui.h > 0 ? g_layout.ui.h : 280;
        const int slots = ccListContentRows(uh, n);
        int maxS = n - slots;
        if (maxS < 0) {
            maxS = 0;
        }
        g_listScroll += slots > 0 ? slots : 1;
        if (g_listScroll > maxS) {
            g_listScroll = maxS;
        }
        return;
    }
    case CcHit::Display:
        g_screen = Screen::Display;
        return;
    case CcHit::Disks:
        g_screen = Screen::Disks;
        return;
    case CcHit::Input:
        g_screen = Screen::Input;
        return;
    case CcHit::JoyTest:
        g_screen = Screen::JoyTest;
#if ESP2_BLE_HID
        esp2_ble::setHidMonitor(true);
#endif
        return;
    case CcHit::Bluetooth:
        g_screen = Screen::BluetoothHub;
        return;
    case CcHit::Storage:
        g_screen = Screen::Storage;
        return;
    case CcHit::Settings:
        g_screen = Screen::Settings;
        return;
    case CcHit::Reset:
        g_screen = Screen::Reset;
        return;
    case CcHit::OrientClassic:
        if (svc.setOrient) {
            svc.setOrient(esp2_cc_svc::Orient::Classic);
        }
        return;
    case CcHit::OrientLandscape:
        if (svc.setOrient) {
            svc.setOrient(esp2_cc_svc::Orient::Landscape);
        }
        return;
    case CcHit::MonitorWhite:
        if (svc.setMonitor) {
            svc.setMonitor(esp2_cc_svc::Monitor::White);
        }
        return;
    case CcHit::MonitorGreen:
        if (svc.setMonitor) {
            svc.setMonitor(esp2_cc_svc::Monitor::Green);
        }
        return;
    case CcHit::MonitorAmber:
        if (svc.setMonitor) {
            svc.setMonitor(esp2_cc_svc::Monitor::Amber);
        }
        return;
    case CcHit::MonitorArtifact:
        if (svc.setMonitor) {
            svc.setMonitor(esp2_cc_svc::Monitor::Artifact);
        }
        return;
    case CcHit::EffectClean:
        if (svc.setEffect) {
            svc.setEffect(esp2_cc_svc::Effect::Clean);
        }
        return;
    case CcHit::EffectCrt:
        if (svc.setEffect) {
            svc.setEffect(esp2_cc_svc::Effect::Crt);
        }
        return;
    case CcHit::MonoNormal:
        esp2DisplaySetInvert(false);
        markUiFull();
        return;
    case CcHit::MonoInvert:
        esp2DisplaySetInvert(true);
        markUiFull();
        return;
    case CcHit::ScaleInteger:
        esp2DisplaySetClassicFitFull(false);
        markUiFull();
        return;
    case CcHit::ScaleFull:
        esp2DisplaySetClassicFitFull(true);
        markUiFull();
        return;
    case CcHit::DiskBrowse: {
        auto d = svc.disks ? svc.disks() : esp2_cc_svc::DiskSnap{};
        if (!d.sdUsable) {
            setErr("cc.err.no_sd");
            return;
        }
        g_diskFileCount = svc.listDiskFiles ? svc.listDiskFiles(g_diskFiles, kMaxDiskFiles) : 0;
        g_screen = Screen::DiskBrowse;
        return;
    }
    case CcHit::DiskEject:
        if (svc.ejectDrive1) {
            svc.ejectDrive1();
        }
        return;
    case CcHit::LocaleEn:
        esp2_i18x::setLocale(esp2_i18x::Locale::EnUs);
        return;
    case CcHit::LocaleDe:
        esp2_i18x::setLocale(esp2_i18x::Locale::DeDe);
        return;
    case CcHit::StorageShare: {
        auto st = svc.storage ? svc.storage() : esp2_cc_svc::StorageSnap{};
        if (!st.mscSupported) {
            setErr("cc.err.msc_unsupported");
            return;
        }
        if (st.owner == esp2_cc_svc::SdOwnerUi::Computer) {
            setErr("cc.err.sd_computer");
            return;
        }
        g_screen = Screen::StorageShareConfirm;
        return;
    }
    case CcHit::StorageShareYes:
        if (svc.shareSd && svc.shareSd()) {
            dismissChrome();
            return;
        }
        setErr("cc.err.msc_unsupported");
        g_screen = Screen::Storage;
        return;
    case CcHit::StorageShareNo:
        g_screen = Screen::Storage;
        return;
    case CcHit::StorageReturn:
        if (svc.returnSd && !svc.returnSd()) {
            setErr("cc.err.sd_computer");
        }
        return;
    case CcHit::ResetApple:
        if (svc.resetApple) {
            svc.resetApple();
        }
        return;
    case CcHit::ColdStart:
        g_confirm = ConfirmKind::Cold;
        g_screen = Screen::Confirm;
        return;
    case CcHit::RestartEsp:
        g_confirm = ConfirmKind::RestartEsp;
        g_screen = Screen::Confirm;
        return;
    case CcHit::ConfirmYes:
        if (g_confirm == ConfirmKind::Cold && svc.coldStart) {
            svc.coldStart();
        } else if (g_confirm == ConfirmKind::RestartEsp && svc.restartEsp) {
            svc.restartEsp();
        }
        g_confirm = ConfirmKind::None;
        resumeClose();
        return;
    case CcHit::ConfirmNo:
        g_confirm = ConfirmKind::None;
        g_screen = Screen::Reset;
        return;
#if ESP2_BLE_HID
    case CcHit::BleAdd:
        g_pairUntil = 0;
        g_pairPending = false;
        g_screen = Screen::BleScan;
        if (esp2_ble::ready() || esp2_ble::init()) {
            if (!esp2_ble::beginScanAsync(12)) {
                setErr("cc.err.ble_pair");
            }
        } else {
            setErr("cc.err.ble_off");
            g_screen = Screen::BluetoothHub;
        }
        return;
    case CcHit::BleList:
        g_screen = Screen::BleKnown;
        (void)esp2_ble::requestAutoReconnect();
        return;
    case CcHit::BleConnect:
        if (!esp2_ble::requestConnect(g_bleAddr)) {
            setErr("cc.err.ble_pair");
        }
        g_screen = Screen::BleKnown;
        return;
    case CcHit::BleDisconnect:
        (void)esp2_ble::requestDisconnect(g_bleAddr);
        g_screen = Screen::BleKnown;
        return;
    case CcHit::BleForget:
        g_screen = Screen::BleForget;
        return;
    case CcHit::BleForgetYes:
        (void)esp2_ble::forgetAddress(g_bleAddr);
        g_screen = Screen::BleKnown;
        return;
    case CcHit::BleForgetNo:
        g_screen = Screen::BleDetail;
        return;
#endif
    default:
        break;
    }
#if ESP2_BLE_HID
    const int hv = hitToInt(hit);
    if (hv >= hitToInt(CcHit::DiskFile0) && hv <= hitToInt(CcHit::DiskFileMax)) {
        const int idx = g_listScroll + (hv - hitToInt(CcHit::DiskFile0));
        if (idx >= 0 && idx < g_diskFileCount && svc.mountDrive1) {
            char path[96]{};
            std::snprintf(path, sizeof(path), "/esp2/disks/%s", g_diskFiles[idx]);
            if (!svc.mountDrive1(path)) {
                setErr("cc.err.disk");
            } else {
                g_screen = Screen::Disks;
            }
        }
        return;
    }
    if (hv >= hitToInt(CcHit::BleScanItem0) && hv < hitToInt(CcHit::BleScanItem0) + 24) {
        const int idx = hv - hitToInt(CcHit::BleScanItem0);
        esp2_ble::DiscoveredDevice d{};
        if (esp2_ble::getDiscovered(static_cast<size_t>(idx), &d)) {
            std::strncpy(g_bleAddr, d.address, sizeof(g_bleAddr) - 1);
            esp2_ble::cancelScanAsync();
            g_pairPending = true;
            if (!esp2_ble::requestPair(d.address)) {
                setErr("cc.err.ble_pair");
                g_pairPending = false;
            }
        }
        return;
    }
    if (hv >= hitToInt(CcHit::BleKnown0) && hv < hitToInt(CcHit::BleKnown0) + 8) {
        const int idx = hv - hitToInt(CcHit::BleKnown0);
        const auto &cfg = esp2_ble::config();
        if (idx >= 0 && static_cast<size_t>(idx) < cfg.knownCount) {
            std::strncpy(g_bleAddr, cfg.known[static_cast<size_t>(idx)].address, sizeof(g_bleAddr) - 1);
            g_screen = Screen::BleDetail;
        }
        return;
    }
#else
    const int hv = hitToInt(hit);
    if (hv >= hitToInt(CcHit::DiskFile0) && hv <= hitToInt(CcHit::DiskFileMax)) {
        const int idx = g_listScroll + (hv - hitToInt(CcHit::DiskFile0));
        if (idx >= 0 && idx < g_diskFileCount && svc.mountDrive1) {
            char path[96]{};
            std::snprintf(path, sizeof(path), "/esp2/disks/%s", g_diskFiles[idx]);
            if (!svc.mountDrive1(path)) {
                setErr("cc.err.disk");
            } else {
                g_screen = Screen::Disks;
            }
        }
    }
#endif
}

static void drawRow(Arduino_GFX *gfx, const CcUiViewport &ui, int vis, const char *text, bool on,
                    bool disabled, uint8_t textSz = 2, bool focused = false) {
    const CcRect r = ccListRowRect(ui.w, ui.h, vis);
#if defined(ESP2_PLATFORM_WAVESHARE_RLCD42)
    // Mono: mid-greys vanish — invert for focus OR active value; thick frame = focus.
    const bool invert = focused || on;
    const uint16_t fill = invert ? kFg : 0x0000;
    const uint16_t ink = disabled ? kDim : (invert ? 0x0000 : kFg);
    const uint16_t border = invert ? 0x0000 : kFg;
#else
    const uint16_t fill = focused ? kPress : (on ? kSel : kCard);
    const uint16_t ink = disabled ? kDim : kFg;
    const uint16_t border = focused ? kAccent : kFg;
#endif
    gfx->fillRect(ui.x + r.x, ui.y + r.y, r.w, r.h, fill);
    gfx->drawRect(ui.x + r.x, ui.y + r.y, r.w, r.h, border);
    if (focused) {
        gfx->drawRect(ui.x + r.x + 1, ui.y + r.y + 1, r.w - 2, r.h - 2, border);
        gfx->drawRect(ui.x + r.x + 2, ui.y + r.y + 2, r.w - 4, r.h - 4, border);
    }
#if defined(ESP2_PLATFORM_WAVESHARE_RLCD42)
    if (on && !focused) {
        gfx->fillRect(ui.x + r.x + 4, ui.y + r.y + 6, 6, r.h - 12, 0x0000);
    }
#endif
    const int ty = ui.y + r.y + r.h / 2 - (textSz > 1 ? 8 : 4);
    const int tx = ui.x + r.x + 10;
    drawLabelSz(gfx, tx, ty, ink, text, textSz, fill);
}

} // namespace

bool isOpen() { return g_open; }

bool uiDirty() { return g_uiDirty; }

void open() {
    if (!g_open) {
        pauseOpen();
        g_screen = Screen::Home;
        g_focusIdx = 0;
    }
    g_open = true;
    g_error = nullptr;
    g_panelCleared = false;
    rebuildFocusList();
    markUiFull();
    if (g_ptr.gate == esp_bracket::CcGate::Closed) {
        g_ptr.gate = esp_bracket::CcGate::Interactive;
    }
}

void close() { resumeClose(); }

void tick(uint32_t nowMs) {
    if (!g_open) {
        return;
    }
#if ESP2_BLE_HID
    if (g_pairUntil != 0 && nowMs >= g_pairUntil) {
        g_pairUntil = 0;
        if (g_screen == Screen::BleScan) {
            g_screen = Screen::BleKnown;
        }
        g_pairPending = false;
        markUiFull();
    }
    if (g_screen == Screen::BleScan && g_pairPending) {
        const auto st = esp2_ble::status();
        if (!st.opBusy && st.connected) {
            g_pairUntil = nowMs + 1500;
            markUi();
        } else if (!st.opBusy && st.lastOpState == esp2_ble::LinkState::Error) {
            g_pairPending = false;
            g_pairUntil = 0;
            setErr("cc.err.ble_pair");
        }
    }
    static size_t lastDisc = 0;
    static bool lastScanning = false;
    static uint32_t lastBleSig = 0;
    if (g_screen == Screen::BleScan) {
        const auto st = esp2_ble::status();
        const size_t n = esp2_ble::discoveredCount();
        if (n != lastDisc || st.scanning != lastScanning) {
            lastDisc = n;
            lastScanning = st.scanning;
            markUi();
        }
        static uint32_t lastScanKick = 0;
        if (!st.scanning && !st.opBusy && !g_pairPending && n == 0 &&
            nowMs - lastScanKick >= 2500) {
            lastScanKick = nowMs;
            (void)esp2_ble::beginScanAsync(12);
        }
    }
    if (g_screen == Screen::BleKnown || g_screen == Screen::BleDetail ||
        g_screen == Screen::BluetoothHub) {
        const auto st = esp2_ble::status();
        uint32_t sig = static_cast<uint32_t>(st.opBusy) | (static_cast<uint32_t>(st.scanning) << 1) |
                       (static_cast<uint32_t>(st.connected) << 2) |
                       (static_cast<uint32_t>(st.lastOpState) << 8);
        const auto &cfg = esp2_ble::config();
        for (size_t i = 0; i < cfg.knownCount; ++i) {
            sig = sig * 33u + static_cast<uint32_t>(esp2_ble::linkStateForAddress(cfg.known[i].address));
        }
        if (sig != lastBleSig) {
            lastBleSig = sig;
            markUi();
        }
    }
    if (g_screen == Screen::JoyTest) {
        static uint32_t lastJoyMs = 0;
        if (nowMs - lastJoyMs >= 80) {
            lastJoyMs = nowMs;
            markUi();
        }
    }
#else
    (void)nowMs;
#endif
}

void setLayoutContext(const CcLayoutContext &ctx) {
    const bool ch = ctx.ui.x != g_layout.ui.x || ctx.ui.y != g_layout.ui.y || ctx.ui.w != g_layout.ui.w ||
                    ctx.ui.h != g_layout.ui.h || ctx.present != g_layout.present ||
                    ctx.surface != g_layout.surface;
    g_layout = ctx;
    if (ch) {
        markUiFull();
    }
}

void draw(Arduino_GFX *gfx, int panelW, int panelH) {
    if (!g_open || !gfx || !g_uiDirty) {
        return;
    }
    g_uiDirty = false;
    const CcUiViewport ui = activeUi(panelW, panelH);
    if (!g_panelCleared) {
        gfx->fillScreen(kBg);
        g_panelCleared = true;
    }
    gfx->fillRect(ui.x, ui.y, ui.w, ui.h, kBg);
    g_clearUi = false;
    g_drawnScreen = g_screen;
    const auto &svc = esp2_cc_svc::services();

    switch (g_screen) {
    case Screen::Home: {
        rebuildFocusList();
        gfx->fillRect(ui.x, ui.y, ui.w, kCcHeaderH, kHeader);
        gfx->drawFastHLine(ui.x, ui.y + kCcHeaderH - 1, ui.w, kFg);
        const char *homeTitle = esp2_i18x::t("cc.title");
        const CcHit foc = focusedHit();
        if (g_pressedHit >= 0 && g_pressedHit < kCcHomeTiles) {
            homeTitle = esp2_i18x::t(homeKey(g_pressedHit));
        } else if (hitToInt(foc) >= 0 && hitToInt(foc) < kCcHomeTiles) {
            homeTitle = esp2_i18x::t(homeKey(hitToInt(foc)));
        }
        drawLabelSz(gfx, ui.x + 8, ui.y + 12, kFg, homeTitle, 2, kHeader);
        for (int i = 0; i < kCcHomeTiles; ++i) {
            const CcRect r = ccHomeTileRect(ui.w, ui.h, i);
            const bool pr = g_pressedHit == i || isFocused(static_cast<CcHit>(i));
            CcRect abs{ui.x + r.x, ui.y + r.y, r.w, r.h};
#if defined(ESP2_PLATFORM_WAVESHARE_RLCD42)
            // Mono flush collapses mid-greys: invert focused tile (paper + dark ink).
            const uint16_t tileFill = pr ? kFg : 0x0000;
            const uint16_t ink = pr ? 0x0000 : kFg;
            fillRoundish(gfx, abs, tileFill);
            gfx->drawRect(abs.x, abs.y, abs.w, abs.h, pr ? 0x0000 : kFg);
            if (pr) {
                gfx->drawRect(abs.x + 2, abs.y + 2, abs.w - 4, abs.h - 4, 0x0000);
            }
            drawHomeIcon(gfx, abs, i, ink, tileFill);
#else
            const uint16_t tileFill = pr ? kPress : kCard;
            fillRoundish(gfx, abs, tileFill);
            if (pr) {
                gfx->drawRect(abs.x, abs.y, abs.w, abs.h, kAccent);
                gfx->drawRect(abs.x + 1, abs.y + 1, abs.w - 2, abs.h - 2, kAccent);
            }
            drawHomeIcon(gfx, abs, i, pr ? kAccent : kFg, tileFill);
#endif
        }
        drawFooterResume(gfx, ui,
                         g_pressedHit == hitToInt(CcHit::Resume) || isFocused(CcHit::Resume));
#if defined(ESP2_PLATFORM_WAVESHARE_RLCD42)
        drawLabelSz(gfx, ui.x + 8, ui.y + ui.h - 14, kDim, esp2_i18x::t("cc.btn_hint"), 1, kBg);
#endif
        break;
    }
    case Screen::Display: {
        auto p = svc.present ? svc.present() : esp2_cc_svc::PresentSnap{};
        drawHeader(gfx, ui, esp2_i18x::t("cc.display"), true);
        const int n = displayOptionCount();
        const int slots = ccListContentRows(ui.h, n);
        if (g_listScroll > n - slots) {
            g_listScroll = n > slots ? n - slots : 0;
        }
        if (g_listScroll < 0) {
            g_listScroll = 0;
        }
        rebuildFocusList();
        for (int i = 0; i < slots && g_listScroll + i < n; ++i) {
            const int idx = g_listScroll + i;
            const CcHit h = displayOptionHit(idx);
            drawRow(gfx, ui, i, displayOptionLabel(idx), displayOptionOn(idx, p), false, 2,
                    isFocused(h));
        }
        drawPager(gfx, ui, n);
        drawFooterBackResume(gfx, ui);
        break;
    }
    case Screen::Disks: {
        rebuildFocusList();
        drawHeader(gfx, ui, esp2_i18x::t("cc.disks"), true);
        auto d = svc.disks ? svc.disks() : esp2_cc_svc::DiskSnap{};
        char l1[48]{};
        std::snprintf(l1, sizeof(l1), "%s: %s", esp2_i18x::t("cc.drive1"),
                      d.drive1Mounted ? d.drive1Name : esp2_i18x::t("cc.disk.empty"));
        drawRow(gfx, ui, 0, l1, false, false, 2, false);
        drawRow(gfx, ui, 1, esp2_i18x::t("cc.drive2_unavail"), false, true, 2, false);
        drawRow(gfx, ui, 2, esp2_i18x::t("cc.disk.mount"), false, !d.sdUsable, 2,
                isFocused(CcHit::DiskBrowse));
        drawRow(gfx, ui, 3, esp2_i18x::t("cc.disk.eject"), false, !d.drive1Mounted, 2,
                isFocused(CcHit::DiskEject));
        drawFooterBackResume(gfx, ui);
        break;
    }
    case Screen::DiskBrowse: {
        drawHeader(gfx, ui, esp2_i18x::t("cc.disk.mount"), true);
        if (g_diskFileCount == 0) {
            drawLabel(gfx, ui.x + 8, ui.y + kCcHeaderH + 16, kDim, esp2_i18x::t("cc.err.disk"));
        }
        const int vr = ccListVisibleRows(ui.h);
        for (int i = 0; i < vr && g_listScroll + i < g_diskFileCount; ++i) {
            drawRow(gfx, ui, i, g_diskFiles[g_listScroll + i], false, false);
        }
        drawPager(gfx, ui, g_diskFileCount);
        drawFooterBackResume(gfx, ui);
        break;
    }
    case Screen::Input: {
        drawHeader(gfx, ui, esp2_i18x::t("cc.input"), true);
        auto in = svc.input ? svc.input() : esp2_cc_svc::InputSnap{};
        drawRow(gfx, ui, 0, esp2_i18x::t("cc.input.joytest"), false, false);
        drawRow(gfx, ui, 1, esp2_i18x::t("cc.input.keyboard"), in.keyboard, false);
        drawRow(gfx, ui, 2, esp2_i18x::t("cc.input.gamepad"), in.gamepad, false);
        drawRow(gfx, ui, 3, esp2_i18x::t("cc.input.pointer"), in.pointer, false);
        drawFooterBackResume(gfx, ui);
        break;
    }
    case Screen::JoyTest: {
        drawHeader(gfx, ui, esp2_i18x::t("cc.input.joytest"), true);
        int y = ui.y + kCcHeaderH + 8;
        drawLabel(gfx, ui.x + 8, y, kDim, esp2_i18x::t("cc.joy.hint"));
        y += 16;
#if ESP2_BLE_HID
        const esp2_ble::PadDiag d = esp2_ble::latestPadDiag();
        auto fmtN = [](char *out, size_t cap, int16_t v) {
            int32_t n = (static_cast<int32_t>(v) * 100) / 32767;
            const char sign = n < 0 ? '-' : '+';
            if (n < 0) {
                n = -n;
            }
            std::snprintf(out, cap, "%c%d.%02d", sign, static_cast<int>(n / 100),
                          static_cast<int>(n % 100));
        };
        const char *hat = "-";
        switch (d.hat) {
        case 0:
            hat = "UP";
            break;
        case 1:
            hat = "UP-R";
            break;
        case 2:
            hat = "RIGHT";
            break;
        case 3:
            hat = "DN-R";
            break;
        case 4:
            hat = "DOWN";
            break;
        case 5:
            hat = "DN-L";
            break;
        case 6:
            hat = "LEFT";
            break;
        case 7:
            hat = "UP-L";
            break;
        default:
            break;
        }
        char nx[8]{};
        char ny[8]{};
        fmtN(nx, sizeof(nx), d.leftX);
        fmtN(ny, sizeof(ny), d.leftY);
        char line[52]{};
        std::snprintf(line, sizeof(line), "X %s  Y %s", nx, ny);
        drawLabel(gfx, ui.x + 8, y, kFg, line);
        y += 14;
        std::snprintf(line, sizeof(line), "Hat %s  B1 %s  B2 %s", hat,
                      (d.buttons & 1u) ? "ON" : "OFF", (d.buttons & 2u) ? "ON" : "OFF");
        drawLabel(gfx, ui.x + 8, y, kFg, line);
        y += 16;
        std::snprintf(line, sizeof(line), "Paddle X %3u  Paddle Y %3u",
                      static_cast<unsigned>(d.p0), static_cast<unsigned>(d.p1));
        drawLabel(gfx, ui.x + 8, y, kAccent, line);
        y += 14;
        std::snprintf(line, sizeof(line), "Button 0 %d    Button 1 %d", d.pb0 ? 1 : 0,
                      d.pb1 ? 1 : 0);
        drawLabel(gfx, ui.x + 8, y, kAccent, line);
        y += 16;
        std::snprintf(line, sizeof(line), "age %u  raw ", static_cast<unsigned>(d.notifyAgeMs));
        drawLabel(gfx, ui.x + 8, y, kDim, line);
        y += 14;
        char hex[48]{};
        size_t hp = 0;
        for (uint8_t i = 0; i < d.lastLen && hp + 3 < sizeof(hex); ++i) {
            hp += static_cast<size_t>(
                std::snprintf(hex + hp, sizeof(hex) - hp, "%02X%s", d.lastRaw[i],
                              (i + 1 < d.lastLen) ? " " : ""));
        }
        drawLabel(gfx, ui.x + 8, y, kAccent, hex[0] ? hex : "-");
#else
        drawLabel(gfx, ui.x + 8, y, kDim, esp2_i18x::t("cc.err.ble_off"));
#endif
        drawFooterBackResume(gfx, ui);
        break;
    }
    case Screen::BluetoothHub: {
        drawHeader(gfx, ui, esp2_i18x::t("cc.bluetooth"), true);
        const CcRect add = hubActionRect(ui.w, ui.h, 0);
        const CcRect list = hubActionRect(ui.w, ui.h, 1);
        gfx->fillRect(ui.x + add.x, ui.y + add.y, add.w, add.h, kCard);
        gfx->drawRect(ui.x + add.x, ui.y + add.y, add.w, add.h, kFg);
        gfx->fillRect(ui.x + list.x, ui.y + list.y, list.w, list.h, kCard);
        gfx->drawRect(ui.x + list.x, ui.y + list.y, list.w, list.h, kFg);
        drawLabelSz(gfx, ui.x + add.x + 12, ui.y + add.y + add.h / 2 - 8, kFg,
                    esp2_i18x::t("ble.add_device"), 2, kCard);
        drawLabelSz(gfx, ui.x + list.x + 12, ui.y + list.y + list.h / 2 - 8, kFg,
                    esp2_i18x::t("ble.devices"), 2, kCard);
        if (!bleOn()) {
            drawLabelSz(gfx, ui.x + 8, ui.y + ui.h - kCcFooterH - 16, kFg,
                        esp2_i18x::t("cc.err.ble_off"), 1);
        }
        drawFooterBackResume(gfx, ui);
        break;
    }
#if ESP2_BLE_HID
    case Screen::BleScan: {
        const auto st = esp2_ble::status();
        const int n = static_cast<int>(esp2_ble::discoveredCount());
        char head[40]{};
        if (st.scanning) {
            std::snprintf(head, sizeof(head), "%s %d", esp2_i18x::t("ble.searching"), n);
        } else if (st.opBusy) {
            std::snprintf(head, sizeof(head), "%s", esp2_i18x::t("ble.state.connecting"));
        } else if (n == 0) {
            std::snprintf(head, sizeof(head), "%s", esp2_i18x::t("ble.no_devices"));
        } else {
            std::snprintf(head, sizeof(head), "%d-%d/%d", g_listScroll + 1,
                          (g_listScroll + ccListContentRows(ui.h, n) < n
                               ? g_listScroll + ccListContentRows(ui.h, n)
                               : n),
                          n);
        }
        drawHeader(gfx, ui, head, false);
        const int slots = ccListContentRows(ui.h, n);
        if (n == 0 && !st.scanning && !st.opBusy) {
            drawRow(gfx, ui, 0, esp2_i18x::t("ble.no_devices"), false, true, 2);
        }
        for (int i = 0; i < slots && g_listScroll + i < n; ++i) {
            esp2_ble::DiscoveredDevice d{};
            if (!esp2_ble::getDiscovered(static_cast<size_t>(g_listScroll + i), &d)) {
                continue;
            }
            char line[40]{};
            formatBleLine(line, sizeof(line), d.name, d.address, d.kind);
            drawRow(gfx, ui, i, line, false, false, 2);
        }
        drawPager(gfx, ui, n);
        drawFooterBackResume(gfx, ui);
        break;
    }
    case Screen::BleKnown: {
        drawHeader(gfx, ui, esp2_i18x::t("ble.devices"), true);
        const auto &cfg = esp2_ble::config();
        if (cfg.knownCount == 0) {
            drawRow(gfx, ui, 0, esp2_i18x::t("ble.no_known"), false, true, 2);
        } else {
            const int n = static_cast<int>(cfg.knownCount);
            const int slots = ccListContentRows(ui.h, n);
            for (int i = 0; i < slots && g_listScroll + i < n; ++i) {
                const size_t idx = static_cast<size_t>(g_listScroll + i);
                char line[56]{};
                char nm[40]{};
                formatBleLine(nm, sizeof(nm), cfg.known[idx].name, cfg.known[idx].address,
                              kindFromKnownType(cfg.known[idx].type));
                const auto st = esp2_ble::linkStateForAddress(cfg.known[idx].address);
                const char *stKey = (st == esp2_ble::LinkState::Connected) ? "ble.state.connected"
                                                                           : "ble.state.offline";
                if (st == esp2_ble::LinkState::Connecting) {
                    stKey = "ble.state.connecting";
                } else if (st == esp2_ble::LinkState::Pairing) {
                    stKey = "ble.state.pairing";
                } else if (st == esp2_ble::LinkState::Error) {
                    stKey = "ble.state.error";
                }
                std::snprintf(line, sizeof(line), "%s  %s", nm, esp2_i18x::t(stKey));
                drawRow(gfx, ui, i, line, st == esp2_ble::LinkState::Connected, false, 2);
            }
            drawPager(gfx, ui, n);
        }
        drawFooterBackResume(gfx, ui);
        break;
    }
    case Screen::BleDetail: {
        drawHeader(gfx, ui, esp2_i18x::t("ble.device_info"), true);
        drawLabel(gfx, ui.x + 8, ui.y + kCcHeaderH + 8, kFg, g_bleAddr);
        const auto lst = esp2_ble::linkStateForAddress(g_bleAddr);
        const char *stKey = "ble.state.offline";
        if (lst == esp2_ble::LinkState::Connected) {
            stKey = "ble.state.connected";
        } else if (lst == esp2_ble::LinkState::Connecting) {
            stKey = "ble.state.connecting";
        } else if (lst == esp2_ble::LinkState::Pairing) {
            stKey = "ble.state.pairing";
        } else if (lst == esp2_ble::LinkState::Error) {
            stKey = "ble.state.error";
        }
        drawRow(gfx, ui, 0, esp2_i18x::t(stKey), lst == esp2_ble::LinkState::Connected, true, 2);
        drawRow(gfx, ui, 1, esp2_i18x::t("ble.action.connect"), false, false);
        drawRow(gfx, ui, 2, esp2_i18x::t("ble.action.disconnect"), false, false);
        drawRow(gfx, ui, 3, esp2_i18x::t("ble.action.forget"), false, false);
        drawFooterBackResume(gfx, ui);
        break;
    }
    case Screen::BleForget: {
        drawHeader(gfx, ui, esp2_i18x::t("ble.forget_confirm_title"), true);
        drawLabel(gfx, ui.x + 8, ui.y + kCcHeaderH + 8, kFg, esp2_i18x::t("ble.forget_confirm_body"));
        drawRow(gfx, ui, 0, esp2_i18x::t("ble.forget_yes"), false, false);
        drawRow(gfx, ui, 1, esp2_i18x::t("ble.forget_no"), false, false);
        drawFooterBackResume(gfx, ui);
        break;
    }
#endif
    case Screen::Storage: {
        drawHeader(gfx, ui, esp2_i18x::t("cc.storage"), true);
        auto st = svc.storage ? svc.storage() : esp2_cc_svc::StorageSnap{};
        drawRow(gfx, ui, 0,
                st.present ? esp2_i18x::t("cc.sd.present") : esp2_i18x::t("cc.sd.missing"), false,
                false);
        drawRow(gfx, ui, 1,
                st.owner == esp2_cc_svc::SdOwnerUi::Computer ? esp2_i18x::t("cc.owner.computer")
                                                            : esp2_i18x::t("cc.owner.esp2"),
                false, false);
        drawRow(gfx, ui, 2,
                st.fatOk ? esp2_i18x::t("cc.sd.fat") : esp2_i18x::t("cc.sd.no_fs"), false, false);
        drawRow(gfx, ui, 3, esp2_i18x::t("cc.sd.share"), false, !st.mscSupported);
        drawRow(gfx, ui, 4, esp2_i18x::t("cc.sd.return"), false,
                st.owner != esp2_cc_svc::SdOwnerUi::Computer);
        drawFooterBackResume(gfx, ui);
        break;
    }
    case Screen::StorageShareConfirm: {
        drawHeader(gfx, ui, esp2_i18x::t("cc.sd.share"), false);
        drawLabel(gfx, ui.x + 8, ui.y + kCcHeaderH + 10, kFg, esp2_i18x::t("cc.sd.share_warn"));
        drawRow(gfx, ui, 0, esp2_i18x::t("cc.confirm"), false, false);
        drawRow(gfx, ui, 1, esp2_i18x::t("cc.cancel"), false, false);
        drawFooterBackResume(gfx, ui);
        break;
    }
    case Screen::Settings: {
        drawHeader(gfx, ui, esp2_i18x::t("cc.settings"), true);
        drawRow(gfx, ui, 0, esp2_i18x::t("cc.locale.en"),
                esp2_i18x::locale() == esp2_i18x::Locale::EnUs, false);
        drawRow(gfx, ui, 1, esp2_i18x::t("cc.locale.de"),
                esp2_i18x::locale() == esp2_i18x::Locale::DeDe, false);
        drawFooterBackResume(gfx, ui);
        break;
    }
    case Screen::Reset: {
        drawHeader(gfx, ui, esp2_i18x::t("cc.reset"), true);
        drawRow(gfx, ui, 0, esp2_i18x::t("cc.reset.apple"), false, false);
        drawRow(gfx, ui, 1, esp2_i18x::t("cc.reset.cold"), false, false);
        drawRow(gfx, ui, 2, esp2_i18x::t("cc.reset.esp"), false, false);
        drawFooterBackResume(gfx, ui);
        break;
    }
    case Screen::Confirm: {
        drawHeader(gfx, ui,
                   g_confirm == ConfirmKind::RestartEsp ? esp2_i18x::t("cc.reset.esp")
                                                        : esp2_i18x::t("cc.reset.cold"),
                   false);
        drawLabel(gfx, ui.x + 8, ui.y + kCcHeaderH + 10, kFg, esp2_i18x::t("cc.confirm_body"));
        drawRow(gfx, ui, 0, esp2_i18x::t("cc.confirm"), false, false);
        drawRow(gfx, ui, 1, esp2_i18x::t("cc.cancel"), false, false);
        drawFooterBackResume(gfx, ui);
        break;
    }
    default:
        break;
    }
    drawError(gfx, ui);
}

bool onPointer(uint16_t x, uint16_t y, bool pressed, bool released, bool contact, bool wakeConsumed,
               int panelW, int panelH) {
    if (pressed && !g_open && millis() < g_blockOpenUntil) {
        return false;
    }
    const CcUiViewport ui = activeUi(panelW, panelH);
    int lx = static_cast<int>(x);
    int ly = static_cast<int>(y);
    const bool inside = ccToUiLocal(ui, lx, ly, &lx, &ly);
    int hit = hitToInt(CcHit::None);
    if (inside) {
        hit = hitToInt(hitTest(ui.w, ui.h, lx, ly));
    }
    if (pressed) {
        const CcEvent ev = g_ptr.onPressed(wakeConsumed);
        if (ev == CcEvent::OpenMenu) {
            open();
            g_pressedHit = -1;
            return true;
        }
        if (g_ptr.gate == esp_bracket::CcGate::WaitRelease) {
            g_pressedHit = -1;
            return true;
        }
        if (!inside) {
            g_pressedHit = -1;
            return true;
        }
        g_ptr.onPressedHit(hit);
        if (g_pressedHit != hit) {
            g_pressedHit = hit;
            markUi();
        }
        g_downX = static_cast<int>(x);
        g_downY = static_cast<int>(y);
        return true;
    }
    if (released) {
        if (g_ptr.gate == esp_bracket::CcGate::WaitRelease) {
            (void)g_ptr.onReleased(hit);
            g_pressedHit = -1;
            g_downX = -1;
            return g_open;
        }
        const int pressHit = g_pressedHit;
        const int dx = g_downX >= 0 ? static_cast<int>(x) - g_downX : 0;
        const int dy = g_downY >= 0 ? static_cast<int>(y) - g_downY : 0;
        const int adx = dx < 0 ? -dx : dx;
        const int ady = dy < 0 ? -dy : dy;
        const bool tap = adx < 36 && ady < 36;
        if (!tap && g_screen != Screen::Home && g_downX >= 0) {
            if (ady > 96 && adx < 40) {
                const int dir = dy > 0 ? -1 : 1;
                if (g_screen == Screen::DiskBrowse) {
                    g_listScroll += dir;
                    if (g_listScroll < 0) {
                        g_listScroll = 0;
                    }
                    markUi();
                } else if (g_screen == Screen::Display || g_screen == Screen::BleScan ||
                           g_screen == Screen::BleKnown) {
                    g_listScroll += dir;
                    if (g_listScroll < 0) {
                        g_listScroll = 0;
                    }
                    markUi();
                }
                g_downX = -1;
                g_pressedHit = -1;
                return g_open;
            }
            if (adx > 110 && ady < 48) {
                g_downX = -1;
                g_pressedHit = -1;
                activate(CcHit::Back);
                return g_open;
            }
        }
        int useHit = hit;
        if (tap && pressHit != hitToInt(CcHit::None)) {
            useHit = pressHit;
        }
        g_downX = -1;
        g_pressedHit = -1;
        const CcEvent ev = g_ptr.onReleased(useHit);
        if (ev == CcEvent::Activate) {
            activate(static_cast<CcHit>(useHit));
            rebuildFocusList();
            g_focusIdx = 0;
        }
        return g_open;
    }
    (void)contact;
    return g_open;
}

void onButton(Button button) {
    if (button == Button::Activate && !g_open) {
        open();
        return;
    }
    if (!g_open) {
        if (button == Button::Back) {
            return;
        }
        if (button == Button::Next) {
            open();
        }
        return;
    }
    rebuildFocusList();
    if (button == Button::Next) {
        if (g_focusCount > 0) {
            g_focusIdx = (g_focusIdx + 1) % g_focusCount;
            markUi();
        }
        return;
    }
    if (button == Button::Back) {
        activate(CcHit::Back);
        if (g_open) {
            rebuildFocusList();
            g_focusIdx = 0;
            markUi();
        }
        return;
    }
    const CcHit h = focusedHit();
    if (h != CcHit::None) {
        activate(h);
        if (g_open) {
            rebuildFocusList();
            g_focusIdx = 0;
            for (int i = 0; i < g_focusCount; ++i) {
                if (g_focusHits[i] == hitToInt(h)) {
                    g_focusIdx = i;
                    break;
                }
            }
            markUi();
        }
    }
}

} // namespace esp2_cc
