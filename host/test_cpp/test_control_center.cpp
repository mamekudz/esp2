/**
 * Host unit test: Control Center layout, touch consume, coords, pause semantics.
 */
#include "esp_bracket/control_center.hpp"

#include <cstdio>

static int g_fails = 0;

#define CHECK(cond)                                                                                \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                            \
            ++g_fails;                                                                             \
        }                                                                                          \
    } while (0)

int main() {
    using namespace esp_bracket;

    CHECK(static_cast<int>(SystemTouchMode::Menu) == 0);

    // Waveshare 280x456
    CHECK(ccHomeCols(280) == 2);
    CHECK(ccHomeColsFor(280, 456) == 2);
    const CcRect t0 = ccHomeTileRect(280, 456, 0);
    CHECK(t0.w >= kCcMinHit);
    CHECK(t0.h >= kCcMinHit);
    CHECK(ccRectHit(t0, t0.x + 4, t0.y + 4));
    CHECK(!ccRectHit(t0, -1, t0.y));
    const CcRect foot = ccFooterRect(280, 456);
    CHECK(foot.h >= kCcMinHit);
    CHECK(ccHomeHit(280, 456, foot.x + 10, foot.y + 10) == CcHit::Resume);
    CHECK(ccHomeHit(280, 456, t0.x + 8, t0.y + 8) == CcHit::Display);
    CHECK(ccHomeHit(280, 456, 0, kCcHeaderH) == CcHit::None);
    CHECK(ccHomeHit(280, 456, 280 - 10, 8) != CcHit::Resume);

    // diymore 240x320 — 3-column home grid
    CHECK(ccHomeCols(240) == 2);
    CHECK(ccHomeColsFor(240, 320) == 3);
    const CcRect d0 = ccHomeTileRect(240, 320, 0);
    CHECK(d0.w >= kCcMinHit);
    CHECK(d0.h >= kCcMinHit);
    CHECK(ccHomeHit(240, 320, d0.x + 5, d0.y + 5) == CcHit::Display);
    const CcRect dFoot = ccFooterRect(240, 320);
    CHECK(ccHomeHit(240, 320, 20, dFoot.y + 4) == CcHit::Resume);
    CHECK(ccRectHit(ccSubBackRect(280, 456), 10, 456 - 10));
    CHECK(!ccRectHit(ccSubBackRect(280, 456), 200, 456 - 10));
    CHECK(ccRectHit(ccSubResumeRect(280, 456), 200, 456 - 10));

    uint16_t x = 0, y = 0;
    mapTouchToPanel(10, 20, 280, 456, false, &x, &y);
    CHECK(x == 10 && y == 20);
    mapTouchToPanel(10, 20, 280, 456, true, &x, &y);
    CHECK(x == 20);
    CHECK(y == 280 - 1 - 10);

    CcPointer p;
    CHECK(p.onPressed(true) == CcEvent::None); // wake consume
    CHECK(p.gate == CcGate::Closed);
    CHECK(p.onPressed(false) == CcEvent::OpenMenu);
    CHECK(p.gate == CcGate::WaitRelease);
    CHECK(p.onPressed(false) == CcEvent::None); // opening finger ignored
    p.onPressedHit(static_cast<int>(CcHit::Display));
    CHECK(p.onReleased(static_cast<int>(CcHit::Display)) == CcEvent::None);
    CHECK(p.gate == CcGate::Interactive);
    p.onPressedHit(static_cast<int>(CcHit::Display));
    CHECK(p.onReleased(static_cast<int>(CcHit::Display)) == CcEvent::Activate);
    p.onPressedHit(static_cast<int>(CcHit::Disks));
    CHECK(p.onReleased(static_cast<int>(CcHit::None)) == CcEvent::None); // empty space
    p.close();
    CHECK(p.gate == CcGate::Closed);

    CHECK(kCcBleHubActions == 2);

    const CcUiViewport wsFull =
        ccComputeSystemUiViewport(280, 456, CcPresentMode::Classic, CcUiSurface::FullPanel, {});
    CHECK(wsFull.x == 0 && wsFull.y == 0 && wsFull.w == 280 && wsFull.h == 456);
    const CcUiViewport wsEnc = ccProvisionalEnclosureVisible(280, 456);
    CHECK(wsEnc.y >= 0 && wsEnc.h > 0);
    CHECK(wsEnc.y + wsEnc.h <= 456);
    CHECK(wsEnc.h < 456);
    CHECK(ccDefaultUiSurface(280, 456) == CcUiSurface::EnclosureVisible);
    CHECK(ccDefaultUiSurface(240, 320) == CcUiSurface::FullPanel);
    const CcUiViewport diy =
        ccComputeSystemUiViewport(240, 320, CcPresentMode::Classic, CcUiSurface::FullPanel, {});
    CHECK(diy.w == 240 && diy.h == 320);
    const CcUiViewport landEnc = ccComputeSystemUiViewport(
        280, 456, CcPresentMode::Landscape, CcUiSurface::EnclosureVisible, {});
    CHECK(landEnc.w == wsEnc.w && landEnc.h == wsEnc.h);
    CHECK(ccTouchInUiViewport(wsEnc, wsEnc.x + 4, wsEnc.y + 4));
    CHECK(!ccTouchInUiViewport(wsEnc, 10, 400));
    int lx = 0, ly = 0;
    CHECK(ccToUiLocal(wsEnc, wsEnc.x + 5, wsEnc.y + 9, &lx, &ly));
    CHECK(lx == 5 && ly == 9);
    CHECK(!ccToUiLocal(wsEnc, 10, 400, &lx, &ly));
    CcViewportOverride over{};
    over.enabled = true;
    over.x = 0;
    over.y = 0;
    over.w = 280;
    over.h = 456;
    const CcUiViewport dev = ccComputeSystemUiViewport(
        280, 456, CcPresentMode::Classic, CcUiSurface::EnclosureVisible, over);
    CHECK(dev.h == 456);

    const int vrEnc = ccListVisibleRows(wsEnc.h);
    CHECK(vrEnc >= 1);
    const CcRect lastRow = ccListRowRect(wsEnc.w, wsEnc.h, vrEnc - 1);
    CHECK(ccRowFullyInsideBody(wsEnc, lastRow));
    CHECK(ccListRowH(wsEnc.h) >= kCcMinHit);
    CHECK(ccListRowH(456) >= kCcMinHit);
    const CcRect back = ccSubBackRect(wsEnc.w, wsEnc.h);
    const CcRect resume = ccSubResumeRect(wsEnc.w, wsEnc.h);
    CHECK(back.y + back.h <= wsEnc.h);
    CHECK(resume.y + resume.h <= wsEnc.h);
    CHECK(ccHomeColsFor(wsEnc.w, wsEnc.h) == 3);
    const CcRect tile = ccHomeTileRect(wsEnc.w, wsEnc.h, 0);
    CHECK(tile.h >= kCcMinHit);
    const CcRect dlg0 = ccListRowRect(wsEnc.w, wsEnc.h, 0);
    const CcRect dlg1 = ccListRowRect(wsEnc.w, wsEnc.h, 1);
    CHECK(ccRowFullyInsideBody(wsEnc, dlg0));
    CHECK(ccRowFullyInsideBody(wsEnc, dlg1));

    if (g_fails != 0) {
        std::printf("test_control_center: FAIL (%d)\n", g_fails);
        return 1;
    }
    std::printf("test_control_center: PASS\n");
    return 0;
}
