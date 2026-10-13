#include "esp2_touch.hpp"

#include <Arduino.h>

#include "esp2_board.h"

#include <Wire.h>

namespace esp2_touch {
namespace {

static bool g_ready = false;
static bool g_stableDown = false;
static uint8_t g_downStreak = 0;
static uint8_t g_upStreak = 0;
static uint16_t g_lastX = 0;
static uint16_t g_lastY = 0;

#if defined(ESP2_PLATFORM_DIYMORE_ES3C28P)
static constexpr uint8_t kFtAddr = DIYMORE_FT6336G_I2C_ADDR;
static constexpr int kSda = PIN_DIYMORE_TP_SDA;
static constexpr int kScl = PIN_DIYMORE_TP_SCL;
static constexpr int kInt = PIN_DIYMORE_TP_INT;
#elif defined(ESP2_PLATFORM_WAVESHARE_RLCD42)
static constexpr uint8_t kFtAddr = 0;
static constexpr int kSda = -1;
static constexpr int kScl = -1;
static constexpr int kInt = -1;
#else
static constexpr uint8_t kFtAddr = FT3168_I2C_ADDR;
static constexpr int kSda = PIN_TOUCH_SDA;
static constexpr int kScl = PIN_TOUCH_SCL;
static constexpr int kInt = PIN_TP_INT;
#endif

static bool pollRaw(uint16_t *x, uint16_t *y, bool *down) {
    Wire.beginTransmission(kFtAddr);
    Wire.write(0x02);
    if (Wire.endTransmission(false) != 0) {
        return false;
    }
    if (Wire.requestFrom(static_cast<int>(kFtAddr), 1) != 1) {
        return false;
    }
    const uint8_t td = Wire.read() & 0x0F;
    *down = td > 0;
    *x = g_lastX;
    *y = g_lastY;
    if (!*down) {
        return true;
    }
    Wire.beginTransmission(kFtAddr);
    Wire.write(0x03);
    if (Wire.endTransmission(false) != 0) {
        return true;
    }
    if (Wire.requestFrom(static_cast<int>(kFtAddr), 4) != 4) {
        return true;
    }
    const uint8_t xh = Wire.read();
    const uint8_t xl = Wire.read();
    const uint8_t yh = Wire.read();
    const uint8_t yl = Wire.read();
    *x = (static_cast<uint16_t>(xh & 0x0F) << 8) | xl;
    *y = (static_cast<uint16_t>(yh & 0x0F) << 8) | yl;
    if (*x >= ESP2_PANEL_WIDTH) {
        *x = ESP2_PANEL_WIDTH - 1;
    }
    if (*y >= ESP2_PANEL_HEIGHT) {
        *y = ESP2_PANEL_HEIGHT - 1;
    }
    g_lastX = *x;
    g_lastY = *y;
    return true;
}

static bool ftWrite(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(kFtAddr);
    Wire.write(reg);
    Wire.write(val);
    return Wire.endTransmission() == 0;
}

} // namespace

bool initAfterDisplay() {
#if defined(ESP2_PLATFORM_WAVESHARE_RLCD42)
    g_ready = false;
    return false; // no touch controller on RLCD 4.2
#endif
    Wire.begin(kSda, kScl);
    Wire.beginTransmission(kFtAddr);
    if (Wire.endTransmission() != 0) {
        g_ready = false;
        return false;
    }
    if (!ftWrite(0x00, 0x00) || !ftWrite(0xA4, 0x00) || !ftWrite(0xA5, 0x00)) {
        g_ready = false;
        return false;
    }
    pinMode(kInt, INPUT_PULLUP);
    g_ready = true;
    g_stableDown = false;
    g_downStreak = 0;
    g_upStreak = 0;
    return true;
}

bool ready() { return g_ready; }

Sample poll() {
    Sample s{};
    if (!g_ready) {
        return s;
    }
    uint16_t x = 0;
    uint16_t y = 0;
    bool down = false;
    if (!pollRaw(&x, &y, &down)) {
        s.x = g_lastX;
        s.y = g_lastY;
        s.contact = g_stableDown;
        return s;
    }
    s.x = x;
    s.y = y;
    if (down) {
        g_upStreak = 0;
        if (g_downStreak < 8) {
            ++g_downStreak;
        }
    } else {
        g_downStreak = 0;
        if (g_upStreak < 8) {
            ++g_upStreak;
        }
    }
    bool stable = g_stableDown;
    if (g_stableDown) {
        if (g_upStreak >= 3) {
            stable = false;
        }
    } else if (g_downStreak >= 3) {
        stable = true;
    }
    s.contact = stable;
    s.pressed = stable && !g_stableDown;
    s.released = !stable && g_stableDown;
    g_stableDown = stable;
    return s;
}

} // namespace esp2_touch
