/**
 * Host unit test: BLE UI list viewport + HID classification + virtual pointer.
 */
#include "esp_bracket/ble_ui_model.hpp"

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

    auto v = computeListViewport(20, 0, 8);
    CHECK(v.scrollTop == 0);
    CHECK(v.selected == 0);

    v = computeListViewport(20, 10, 8);
    CHECK(v.scrollTop == 3);
    CHECK(v.selected == 10);

    v = computeListViewport(20, 19, 8);
    CHECK(v.scrollTop == 12);
    CHECK(v.selected == 19);

    v = computeListViewport(5, 3, 8);
    CHECK(v.scrollTop == 0);

    CHECK(classifyHidKinds(true, true, false, false) == BleHidKind::Composite);
    CHECK(classifyHidKinds(true, false, false, false) == BleHidKind::Keyboard);
    CHECK(classifyHidKinds(false, true, false, false) == BleHidKind::Mouse);

    int16_t x = 0;
    x = accumulateRelativeAxis(x, 1, 3);
    CHECK(x == 768);
    x = accumulateRelativeAxis(x, -1, 3);
    CHECK(x == 0);

    if (g_fails != 0) {
        std::printf("test_ble_ui_model: FAIL (%d)\n", g_fails);
        return 1;
    }
    std::printf("test_ble_ui_model: PASS\n");
    return 0;
}
