#include "esp2_settings_ui.hpp"

#include "esp2_control_center.hpp"

namespace esp2_settings {

bool isOpen() { return esp2_cc::isOpen(); }
void open() { esp2_cc::open(); }
void close() { esp2_cc::close(); }
void draw(Arduino_GFX *gfx, int panelW, int panelH) { esp2_cc::draw(gfx, panelW, panelH); }
void tick(uint32_t nowMs) { esp2_cc::tick(nowMs); }

bool onTouchTap(uint16_t x, uint16_t y, int panelW, int panelH, bool wakeConsumed) {
    return esp2_cc::onPointer(x, y, true, false, true, wakeConsumed, panelW, panelH);
}

void onKeyboardNav(bool, bool, bool activate, bool back) {
    if (back) {
        esp2_cc::close();
        return;
    }
    if (activate && !esp2_cc::isOpen()) {
        esp2_cc::open();
    }
}

} // namespace esp2_settings
