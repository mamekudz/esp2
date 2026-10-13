#include "esp2_ble_hid.hpp"

#include "esp_bracket/ble_ui_model.hpp"
#include "esp_bracket/gamepad_mapper.hpp"
#include "esp_bracket/key_map.hpp"

#include <NimBLEDevice.h>
#include "esp2_sd_bus.hpp"

#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

namespace esp2_ble {
namespace {

static constexpr uint16_t kUuidHidService = 0x1812;
static constexpr uint16_t kUuidReportMap = 0x2A4B;
static constexpr uint16_t kUuidReport = 0x2A4D;
static constexpr uint16_t kUuidHidInfo = 0x2A4A;
static constexpr uint16_t kUuidHidControl = 0x2A4C;
static constexpr uint16_t kUuidProtocolMode = 0x2A4E;
static constexpr uint16_t kUuidBootKbIn = 0x2A22;
static constexpr uint16_t kUuidBootMouseIn = 0x2A33;

esp2_ble_config::BluetoothConfig g_cfg = esp2_ble_config::defaultBluetoothConfig();
esp_bracket::LocalControlEngine g_engine;
LocalActionHandler g_actionHandler = nullptr;
KeyInjectFn g_keyInject = nullptr;
PadInjectFn g_padInject = nullptr;

bool g_ready = false;
bool g_scanning = false;
bool g_scanAsync = false;
uint32_t g_scanEndMs = 0;
bool g_suppressAutoReconnect = false;
uint32_t g_nextAutoReconnectMs = 0;
size_t g_autoReconnectIndex = 0;
static constexpr uint32_t kAutoReconnectRetryMs = 3000;
char g_reconnectBackoffAddr[20]{};
uint32_t g_reconnectBackoffUntilMs = 0;
LinkState g_opState = LinkState::KnownOffline;
bool g_opBusy = false;
char g_pendingPair[20]{};
char g_opAddr[20]{};
QueueHandle_t g_bleOpQueue = nullptr;
TaskHandle_t g_bleWorker = nullptr;

static constexpr size_t kMaxHidLinks = 2;
struct HidConn {
    NimBLEClient *client = nullptr;
    bool live = false;
    bool keyboard = false;
    bool gamepad = false;
    char addr[20]{};
    char name[40]{};
    esp2_ble_config::DeviceType type = esp2_ble_config::DeviceType::Unknown;
};
HidConn g_links[kMaxHidLinks]{};
NimBLEClient *g_client = nullptr;
bool g_connected = false;
char g_connAddr[20]{};
char g_connName[40]{};
esp2_ble_config::DeviceType g_connType = esp2_ble_config::DeviceType::Unknown;
bool g_isKeyboard = false;
bool g_isGamepad = false;
bool g_isMouse = false; // Generic Desktop Mouse / relative pointer on same HID link

DiscoveredDevice g_discovered[kScanMax]{};
size_t g_discoveredCount = 0;

esp_bracket::GamepadState g_pad{};
esp_bracket::GamepadMapper g_mapper{};

uint8_t g_prevKeys[6]{};
uint8_t g_prevPadKeyMask = 0;
bool g_prevPadBtnA = false;
uint8_t g_prevMod = 0;

bool g_hidMonitor = false;
uint32_t g_hidMonLastMs = 0;
PadMapMode g_padMode = PadMapMode::Joystick;
uint8_t g_padSensitivity = 3;
int16_t g_virtX = 0; // -32768..32767 for GamepadState axes
int16_t g_virtY = 0;
bool g_mouseBtn0 = false;
bool g_mouseBtn1 = false;
PadDiag g_diag{};

portMUX_TYPE g_hidMux = portMUX_INITIALIZER_UNLOCKED;
struct RawHid {
    bool valid = false;
    uint8_t len = 0;
    uint8_t fromKb = 0;
    uint8_t fromGp = 0;
    uint8_t data[64]{};
    uint32_t seq = 0;
};
static constexpr size_t kHidQ = 16;
RawHid g_hidQ[kHidQ]{};
uint8_t g_hidQHead = 0;
uint8_t g_hidQTail = 0;
uint8_t g_hidQCount = 0;
uint32_t g_hidSeq = 0;
uint8_t g_hidLogLeft = 0;
volatile bool g_authComplete = false;
volatile uint32_t g_hidNotifyCount = 0;
volatile uint32_t g_lastHidNotifyMs = 0;
uint32_t g_bleHbMs = 0;

struct HidCaps {
    bool keyboard = false;
    bool mouse = false;
    bool gamepad = false;
    bool joystick = false;
};
HidCaps g_caps{};

void logf(const char *fmt, ...);
void fireAction(esp_bracket::LocalAction a, const char *src);

void logf(const char *fmt, ...) {
    char buf[200];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    Serial.printf("[BLE] %s\n", buf);
}

bool addrEq(const char *a, const char *b) {
    if (!a || !b) {
        return false;
    }
    while (*a && *b) {
        if (std::tolower(static_cast<unsigned char>(*a)) !=
            std::tolower(static_cast<unsigned char>(*b))) {
            return false;
        }
        ++a;
        ++b;
    }
    return *a == 0 && *b == 0;
}

int hidLinkCount() {
    int n = 0;
    for (size_t i = 0; i < kMaxHidLinks; ++i) {
        if (g_links[i].live && g_links[i].client && g_links[i].client->isConnected()) {
            ++n;
        }
    }
    return n;
}

bool hidAddrLive(const char *address) {
    if (!address || !address[0]) {
        return false;
    }
    for (size_t i = 0; i < kMaxHidLinks; ++i) {
        if (g_links[i].live && addrEq(g_links[i].addr, address) && g_links[i].client &&
            g_links[i].client->isConnected()) {
            return true;
        }
    }
    return false;
}

void refreshConnectedGlobals() {
    g_connected = false;
    g_connAddr[0] = 0;
    g_connName[0] = 0;
    g_connType = esp2_ble_config::DeviceType::Unknown;
    for (size_t i = 0; i < kMaxHidLinks; ++i) {
        if (!g_links[i].live || !g_links[i].client || !g_links[i].client->isConnected()) {
            continue;
        }
        if (!g_connected) {
            std::strncpy(g_connAddr, g_links[i].addr, sizeof(g_connAddr) - 1);
            std::strncpy(g_connName, g_links[i].name, sizeof(g_connName) - 1);
            g_connType = g_links[i].type;
        }
        g_connected = true;
    }
}

int allocHidLink(const char *address) {
    if (address && address[0]) {
        for (size_t i = 0; i < kMaxHidLinks; ++i) {
            if (addrEq(g_links[i].addr, address)) {
                return static_cast<int>(i);
            }
        }
    }
    for (size_t i = 0; i < kMaxHidLinks; ++i) {
        const bool up = g_links[i].live && g_links[i].client && g_links[i].client->isConnected();
        if (!up) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void disconnectAddress(const char *address) {
    for (size_t i = 0; i < kMaxHidLinks; ++i) {
        if (!g_links[i].client) {
            continue;
        }
        if (address && address[0] && !addrEq(g_links[i].addr, address)) {
            continue;
        }
        logf("disconnect slot=%u addr=%s live=%d", static_cast<unsigned>(i), g_links[i].addr,
             g_links[i].live ? 1 : 0);
        g_links[i].client->disconnect();
        g_links[i].live = false;
    }
    refreshConnectedGlobals();
}

bool payloadHasUuid16(NimBLEAdvertisedDevice *d, uint16_t uuid) {
    if (!d) {
        return false;
    }
    const uint8_t *p = d->getPayload();
    const size_t plen = d->getPayloadLength();
    if (!p || plen < 4) {
        return false;
    }
    size_t i = 0;
    while (i + 1 < plen) {
        const uint8_t fieldLen = p[i];
        if (fieldLen == 0) {
            break;
        }
        if (i + 1 + fieldLen > plen) {
            break;
        }
        const uint8_t type = p[i + 1];
        if ((type == 0x02 || type == 0x03 || type == 0x14 || type == 0x15) && fieldLen >= 3) {
            for (size_t j = 0; j + 1 < static_cast<size_t>(fieldLen); j += 2) {
                const uint16_t u = static_cast<uint16_t>(p[i + 2 + j] | (p[i + 3 + j] << 8));
                if (u == uuid) {
                    return true;
                }
            }
        }
        if (type == 0x16 && fieldLen >= 3) {
            const uint16_t u = static_cast<uint16_t>(p[i + 2] | (p[i + 3] << 8));
            if (u == uuid) {
                return true;
            }
        }
        i += static_cast<size_t>(fieldLen) + 1;
    }
    return false;
}

ScanDeviceKind classifyAdvertisement(NimBLEAdvertisedDevice *d) {
    if (!d) {
        return ScanDeviceKind::Unknown;
    }
    const bool hidSvc = d->isAdvertisingService(NimBLEUUID((uint16_t)kUuidHidService)) ||
                        payloadHasUuid16(d, kUuidHidService);
    if (d->haveAppearance()) {
        const uint16_t appearance = d->getAppearance();
        if (appearance == 0x03C1) {
            return ScanDeviceKind::Keyboard;
        }
        if (appearance == 0x03C4 || appearance == 0x03C3) {
            return ScanDeviceKind::Gamepad;
        }
        if (appearance == 0x03C2 && hidSvc) {
            return ScanDeviceKind::OtherHid;
        }
    }
    if (hidSvc) {
        return ScanDeviceKind::OtherHid;
    }
    const std::string name = d->getName();
    auto has = [&](const char *sub) {
        if (name.empty() || !sub) {
            return false;
        }
        std::string lower = name;
        for (char &c : lower) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return lower.find(sub) != std::string::npos;
    };
    if (has("keyboard") || has("kbd") || has("dialpad")) {
        return ScanDeviceKind::Keyboard;
    }
    if (has("gamepad") || has("controller") || has("joystick") || has("joypad") || has("t3") ||
        has("terios")) {
        return ScanDeviceKind::Gamepad;
    }
    return ScanDeviceKind::Unknown;
}

HidCaps parseReportMapCaps(const uint8_t *map, uint16_t len) {
    HidCaps caps{};
    uint8_t usagePage = 0;
    for (uint16_t i = 0; i < len;) {
        uint8_t b = map[i++];
        uint8_t size = b & 0x03;
        if (size == 3) {
            size = 4;
        }
        uint8_t type = (b >> 2) & 0x03;
        uint8_t tag = (b >> 4) & 0x0F;
        uint32_t data = 0;
        for (uint8_t s = 0; s < size && i < len; ++s) {
            data |= (static_cast<uint32_t>(map[i++]) << (8 * s));
        }
        if (type == 1 && tag == 0 && size >= 1) {
            usagePage = static_cast<uint8_t>(data & 0xFF);
        } else if (type == 2 && tag == 0 && size >= 1) {
            const uint8_t usage = static_cast<uint8_t>(data & 0xFF);
            if (usagePage == 0x01) {
                if (usage == 0x01 || usage == 0x02) {
                    caps.mouse = true;
                }
                if (usage == 0x04) {
                    caps.joystick = true;
                }
                if (usage == 0x05) {
                    caps.gamepad = true;
                }
                if (usage == 0x06) {
                    caps.keyboard = true;
                }
            } else if (usagePage == 0x07) {
                caps.keyboard = true;
            } else if (usagePage == 0x0D) {
                // Digitizer / touch — treat as absolute-capable pointer family.
                caps.mouse = true;
            }
        }
    }
    return caps;
}

ScanDeviceKind kindFromCaps(const HidCaps &caps) {
    const bool pointerLike = caps.mouse || caps.gamepad || caps.joystick;
    if (caps.keyboard && pointerLike) {
        return ScanDeviceKind::Composite;
    }
    if (caps.keyboard) {
        return ScanDeviceKind::Keyboard;
    }
    if (caps.gamepad || caps.joystick) {
        return ScanDeviceKind::Gamepad;
    }
    if (caps.mouse) {
        return ScanDeviceKind::OtherHid;
    }
    return ScanDeviceKind::OtherHid;
}

ScanDeviceKind refineFromReportMap(const uint8_t *map, uint16_t len) {
    return kindFromCaps(parseReportMapCaps(map, len));
}

bool isHidKeyUsage(uint8_t u) {
    return (u >= 0x04 && u <= 0x31) || u == 0x28 || u == 0x2A || u == 0x2C || u == 0x29 ||
           u == 0x2B || u == 0x4F || u == 0x50 || u == 0x51 || u == 0x52 || u == 0x58;
}

/** Fill standard boot keyboard [mod][reserved][6 usages]. */
bool extractBootKeyboard(const uint8_t *data, uint8_t len, uint8_t out[8]) {
    if (!data || !out || len < 3) {
        return false;
    }
    std::memset(out, 0, 8);
    // BLE reports are often shorter than 8 bytes. Require reserved==0 so mouse
    // [id][buttons][dx][dy] is not treated as keys (dx in the reserved slot).
    if (len >= 4 && data[0] >= 1 && data[0] <= 15 && (data[1] & 0xE0) == 0 && data[2] == 0) {
        const uint8_t n = static_cast<uint8_t>(len - 1);
        std::memcpy(out, data + 1, n > 8 ? 8 : n);
        return true;
    }
    // Classic boot without Report ID: [mod][reserved=0][keys…] (8-byte USB boot).
    if (len >= 8 && len <= 10 && (data[0] & 0xE0) == 0 && data[1] == 0) {
        std::memcpy(out, data, 8);
        return true;
    }
    // Short BLE boot packets. Require a real key usage so [buttons][0][dy] mouse
    // frames are not treated as keys — except arrows live in those slots.
    if (len >= 3 && len <= 7 && (data[0] & 0xE0) == 0 && data[1] == 0) {
        bool anyKey = false;
        for (uint8_t i = 2; i < len && i < 8; ++i) {
            if (isHidKeyUsage(data[i])) {
                anyKey = true;
                break;
            }
        }
        if (anyKey) {
            std::memcpy(out, data, len);
            return true;
        }
    }
    return false;
}

bool looksLikeBootKeyboard(const uint8_t *data, uint8_t len) {
    uint8_t boot[8]{};
    return extractBootKeyboard(data, len, boot);
}

bool analogStickByte(uint8_t v) {
    return v >= 40 && v <= 216;
}

bool analogPairAt(const uint8_t *data, uint8_t i, uint8_t len) {
    return data && (i + 1u) < len && analogStickByte(data[i]) && analogStickByte(data[i + 1]);
}

bool looksLikeButtonFirstPad(const uint8_t *data, uint8_t len) {
    if (!data || len < 6) {
        return false;
    }
    uint8_t off = 0;
    if (data[0] >= 1 && data[0] <= 15) {
        off = 1;
    }
    if (off >= len) {
        return false;
    }
    if (data[off] >= 40) {
        return false;
    }
    return analogPairAt(data, static_cast<uint8_t>(off + 2), len) ||
           analogPairAt(data, static_cast<uint8_t>(off + 3), len);
}

void injectDigitalPadKeys();

void noteHidRaw(const uint8_t *data, uint8_t len) {
    g_diag.lastLen = len > sizeof(g_diag.lastRaw) ? sizeof(g_diag.lastRaw) : len;
    if (data && g_diag.lastLen) {
        std::memcpy(g_diag.lastRaw, data, g_diag.lastLen);
    }
}

void finishPadApply(uint32_t nowMs) {
    g_pad.connected = true;
    g_diag.connected = true;
    g_diag.leftX = g_pad.leftX;
    g_diag.leftY = g_pad.leftY;
    g_diag.hat = 0x0F;
    if (g_pad.dpadUp && g_pad.dpadRight) {
        g_diag.hat = 1;
    } else if (g_pad.dpadRight && g_pad.dpadDown) {
        g_diag.hat = 3;
    } else if (g_pad.dpadDown && g_pad.dpadLeft) {
        g_diag.hat = 5;
    } else if (g_pad.dpadLeft && g_pad.dpadUp) {
        g_diag.hat = 7;
    } else if (g_pad.dpadUp) {
        g_diag.hat = 0;
    } else if (g_pad.dpadRight) {
        g_diag.hat = 2;
    } else if (g_pad.dpadDown) {
        g_diag.hat = 4;
    } else if (g_pad.dpadLeft) {
        g_diag.hat = 6;
    }
    const bool fire =
        g_pad.btnA || g_pad.btnB || g_pad.btnX || g_pad.btnY || g_pad.btnL1 || g_pad.btnR1;
    const auto pads = g_mapper.toPaddles(g_pad);
    g_diag.p0 = static_cast<uint8_t>(pads.p0 > 255 ? 255 : pads.p0);
    g_diag.p1 = static_cast<uint8_t>(pads.p1 > 255 ? 255 : pads.p1);
    g_diag.pb0 = fire;
    g_diag.pb1 = g_pad.btnB || g_pad.btnX;

    const esp_bracket::LocalAction local = g_engine.evaluateGamepad(g_pad, nowMs);
    if (local != esp_bracket::LocalAction::None) {
        fireAction(local, "ble_gamepad");
    }
    if (g_engine.chordConsumingGamepad(g_pad)) {
        return;
    }
    if (g_padInject) {
        g_padInject(g_diag.p0, g_diag.p1, g_diag.pb0, g_diag.pb1, false);
    }
    injectDigitalPadKeys();
}

bool looksLikeAbsAxes(const uint8_t *p, uint8_t n) {
    if (!p || n < 2) {
        return false;
    }
    return analogStickByte(p[0]) && analogStickByte(p[1]);
}

bool looksLikeAbsPad(const uint8_t *data, uint8_t len) {
    if (!data || len < 3) {
        return false;
    }
    if (len >= 4 && data[0] >= 1 && data[0] <= 15 && looksLikeAbsAxes(data + 1, static_cast<uint8_t>(len - 1))) {
        return true;
    }
    return looksLikeAbsAxes(data, len);
}

bool looksLikeRelativeMouse(const uint8_t *data, uint8_t len) {
    // Do not treat mouse/pad axis bytes as HID key usages (0x29 in dx is ESC
    // and pauses Star Blazer). Keyboard is only extractBootKeyboard / fromKb.
    if (!data || looksLikeAbsPad(data, len) || looksLikeButtonFirstPad(data, len)) {
        return false;
    }
    const uint8_t *p = data;
    uint8_t n = len;
    if (n >= 4 && n <= 8 && data[0] >= 1 && data[0] <= 4 && (data[1] & 0xF8) == 0) {
        p = data + 1;
        n = static_cast<uint8_t>(len - 1);
    }
    if (n < 3 || n > 8) {
        return false;
    }
    if ((p[0] & 0xF8) != 0) {
        return false;
    }
    return true;
}

int16_t clampAxis(int32_t v) {
    if (v > 32767) {
        return 32767;
    }
    if (v < -32768) {
        return -32768;
    }
    return static_cast<int16_t>(v);
}

void applyHatNibble(uint8_t n) {
    if (n > 8 && n != 0x0F) {
        return;
    }
    g_pad.dpadUp = false;
    g_pad.dpadDown = false;
    g_pad.dpadLeft = false;
    g_pad.dpadRight = false;
    if (n == 8 || n == 0x0F) {
        return;
    }
    g_pad.dpadUp = (n == 0 || n == 1 || n == 7);
    g_pad.dpadRight = (n == 1 || n == 2 || n == 3);
    g_pad.dpadDown = (n == 3 || n == 4 || n == 5);
    g_pad.dpadLeft = (n == 5 || n == 6 || n == 7);
}

/** II+ titles typically use ←/→ plus A/Z, not IIe up/down. Edge-triggered. */
void injectDigitalPadKeys() {
    if (!g_keyInject) {
        return;
    }
    constexpr int16_t kDig = 10000;
    uint8_t mask = 0;
    if (g_pad.dpadLeft || g_pad.leftX < -kDig) {
        mask |= 1u;
    }
    if (g_pad.dpadRight || g_pad.leftX > kDig) {
        mask |= 2u;
    }
    if (g_pad.dpadUp || g_pad.leftY < -kDig) {
        mask |= 4u;
    }
    if (g_pad.dpadDown || g_pad.leftY > kDig) {
        mask |= 8u;
    }
    const uint8_t keys[4] = {0x08, 0x15, 0x41, 0x5A}; // ← → A Z
    for (int i = 0; i < 4; ++i) {
        const uint8_t bit = static_cast<uint8_t>(1u << i);
        if ((mask & bit) != 0 && (g_prevPadKeyMask & bit) == 0) {
            g_keyInject(keys[i], true);
            logf("padkey apple7=0x%02X", static_cast<unsigned>(keys[i]));
        } else if ((mask & bit) == 0 && (g_prevPadKeyMask & bit) != 0) {
            g_keyInject(keys[i], false);
        }
    }
    g_prevPadKeyMask = mask;
    const bool fire =
        g_pad.btnA || g_pad.btnB || g_pad.btnX || g_pad.btnY || g_pad.btnL1 || g_pad.btnR1;
    if (fire && !g_prevPadBtnA) {
        g_keyInject(0x20, true);
        logf("padkey apple7=0x20");
    } else if (!fire && g_prevPadBtnA) {
        g_keyInject(0x20, false);
    }
    g_prevPadBtnA = fire;
}

void emitPadFromVirtual(uint32_t nowMs) {
    g_pad.connected = true;
    g_pad.leftX = g_virtX;
    g_pad.leftY = g_virtY;
    g_pad.btnA = g_mouseBtn0;
    g_pad.btnB = g_mouseBtn1;
    finishPadApply(nowMs);
}

void handleRelativePointerReport(const uint8_t *data, uint8_t len, uint32_t nowMs) {
    if (len < 3) {
        return;
    }
    uint8_t off = 0;
    if (len >= 4 && data[0] >= 1 && data[0] <= 4 && (data[1] & 0xF8) == 0) {
        off = 1;
    }
    if (len < off + 3) {
        return;
    }
    const uint8_t buttons = data[off];
    const int8_t dx = static_cast<int8_t>(data[off + 1]);
    const int8_t dy = static_cast<int8_t>(data[off + 2]);
    g_mouseBtn0 = (buttons & 0x01) != 0;
    g_mouseBtn1 = (buttons & 0x02) != 0;
    g_virtX = esp_bracket::accumulateRelativeAxis(g_virtX, dx, g_padSensitivity);
    g_virtY = esp_bracket::accumulateRelativeAxis(g_virtY, dy, g_padSensitivity);
    emitPadFromVirtual(nowMs);
}

void hidMonDump(const char *kind, const uint8_t *data, uint8_t len, uint32_t nowMs) {
    if (!g_hidMonitor) {
        return;
    }
    if (nowMs - g_hidMonLastMs < 80) {
        return;
    }
    g_hidMonLastMs = nowMs;
    char hex[96]{};
    size_t hp = 0;
    for (uint8_t i = 0; i < len && hp + 3 < sizeof(hex); ++i) {
        hp += static_cast<size_t>(
            snprintf(hex + hp, sizeof(hex) - hp, "%02X%s", data[i], (i + 1 < len) ? " " : ""));
    }
    Serial.printf("#BLE HID kind=%s len=%u mode=%s virtX=%d virtY=%d btn0=%d btn1=%d data=%s\n",
                  kind ? kind : "?", static_cast<unsigned>(len), padMapModeName(g_padMode),
                  static_cast<int>(g_virtX), static_cast<int>(g_virtY), g_mouseBtn0 ? 1 : 0,
                  g_mouseBtn1 ? 1 : 0, hex);
}

uint8_t hidUsageToApple7(uint8_t usage, bool shift, bool ctrl) {
    (void)shift;
    // Letters: uppercase unless Control is set in the boot modifier byte
    // (Report ID is stripped before this; Ctrl-C → $03 for Star Blazer etc.).
    if (usage >= 0x04 && usage <= 0x1D) {
        const char letter = static_cast<char>('A' + (usage - 0x04));
        if (ctrl) {
            uint8_t out = 0;
            if (esp_bracket::AppleIIKeyMap::mapHostKey(static_cast<int>(letter), false, true,
                                                       &out)) {
                return out;
            }
        }
        return esp_bracket::AppleIIKeyMap::fromAscii(letter);
    }
    // Digits 1–9,0
    if (usage >= 0x1E && usage <= 0x27) {
        const char digits[] = "1234567890";
        return esp_bracket::AppleIIKeyMap::fromAscii(digits[usage - 0x1E]);
    }
    switch (usage) {
    case 0x28:
        return esp_bracket::AppleIIKeyMap::fromAscii('\r'); // Enter
    case 0x58:
        return esp_bracket::AppleIIKeyMap::fromAscii('\r'); // Keypad Enter
    case 0x2A:
        return 0x08; // Backspace
    case 0x2C:
        return esp_bracket::AppleIIKeyMap::fromAscii(' ');
    case 0x29:
        return 0x1B; // Escape
    case 0x2B:
        return 0x09; // Tab
    case 0x4F: // Right arrow → Apple II → (Ctrl-U)
        return 0x15;
    case 0x50: // Left arrow → Apple II ← (Ctrl-H)
        return 0x08;
    case 0x51: // Down → line feed (catalog / IIe down)
        return 0x0A;
    case 0x52: // Up → vertical tab (catalog / IIe up)
        return 0x0B;
    default:
        return 0;
    }
}

void pushHidReport(const RawHid &r) {
    portENTER_CRITICAL(&g_hidMux);
    if (g_hidQCount == kHidQ) {
        g_hidQTail = static_cast<uint8_t>((g_hidQTail + 1) % kHidQ);
        g_hidQCount--;
    }
    g_hidQ[g_hidQHead] = r;
    g_hidQHead = static_cast<uint8_t>((g_hidQHead + 1) % kHidQ);
    g_hidQCount++;
    portEXIT_CRITICAL(&g_hidMux);
}

bool popHidReport(RawHid *out) {
    if (!out) {
        return false;
    }
    portENTER_CRITICAL(&g_hidMux);
    if (g_hidQCount == 0) {
        portEXIT_CRITICAL(&g_hidMux);
        return false;
    }
    *out = g_hidQ[g_hidQTail];
    g_hidQTail = static_cast<uint8_t>((g_hidQTail + 1) % kHidQ);
    g_hidQCount--;
    portEXIT_CRITICAL(&g_hidMux);
    return true;
}

void onHidNotify(NimBLERemoteCharacteristic *chr, uint8_t *data, size_t length, bool) {
    if (!data || length == 0) {
        return;
    }
    RawHid r{};
    r.valid = true;
    r.seq = ++g_hidSeq;
    r.len = static_cast<uint8_t>(length > sizeof(r.data) ? sizeof(r.data) : length);
    std::memcpy(r.data, data, r.len);
    if (chr) {
        NimBLERemoteService *svc = chr->getRemoteService();
        NimBLEClient *cl = svc ? svc->getClient() : nullptr;
        for (size_t i = 0; i < kMaxHidLinks; ++i) {
            if (g_links[i].client == cl) {
                r.fromKb = g_links[i].keyboard ? 1 : 0;
                r.fromGp = g_links[i].gamepad ? 1 : 0;
                break;
            }
        }
    }
    pushHidReport(r);
    g_hidNotifyCount = g_hidNotifyCount + 1;
    g_lastHidNotifyMs = millis();
    if (g_hidLogLeft > 0) {
        g_hidLogLeft--;
        char hex[96]{};
        size_t hp = 0;
        for (uint8_t i = 0; i < r.len && hp + 3 < sizeof(hex); ++i) {
            hp += static_cast<size_t>(
                snprintf(hex + hp, sizeof(hex) - hp, "%02X%s", r.data[i],
                         (i + 1 < r.len) ? " " : ""));
        }
        logf("notify n=%u %s", static_cast<unsigned>(r.len), hex);
    }
}

class ClientCb : public NimBLEClientCallbacks {
public:
    void onConnect(NimBLEClient *c) override {
        for (size_t i = 0; i < kMaxHidLinks; ++i) {
            if (g_links[i].client == c) {
                g_links[i].live = true;
            }
        }
        refreshConnectedGlobals();
        logf("connected links=%d", hidLinkCount());
    }
    void onDisconnect(NimBLEClient *c) override {
        for (size_t i = 0; i < kMaxHidLinks; ++i) {
            if (g_links[i].client == c) {
                g_links[i].live = false;
                logf("disconnected slot=%u addr=%s", static_cast<unsigned>(i), g_links[i].addr);
            }
        }
        refreshConnectedGlobals();
    }
    void onAuthenticationComplete(ble_gap_conn_desc *desc) override {
        if (!desc) {
            logf("auth desc=null");
            g_authComplete = true;
            return;
        }
        logf("auth enc=%u bonded=%u authenticated=%u",
             static_cast<unsigned>(desc->sec_state.encrypted),
             static_cast<unsigned>(desc->sec_state.bonded),
             static_cast<unsigned>(desc->sec_state.authenticated));
        g_authComplete = true;
    }
};

ClientCb g_clientCb;

NimBLEClient *ensureHidClient(int slot) {
    if (slot < 0 || slot >= static_cast<int>(kMaxHidLinks)) {
        return nullptr;
    }
    if (!g_links[slot].client) {
        g_links[slot].client = NimBLEDevice::createClient();
        if (g_links[slot].client) {
            g_links[slot].client->setClientCallbacks(&g_clientCb, false);
            g_links[slot].client->setConnectTimeout(8);
        }
    }
    return g_links[slot].client;
}

bool subscribeHidInputs(NimBLERemoteService *hid) {
    if (!hid) {
        return false;
    }
    auto *charsPtr = hid->getCharacteristics(true);
    if (!charsPtr) {
        return false;
    }
    bool subscribed = false;
    for (auto *ch : *charsPtr) {
        if (!ch) {
            continue;
        }
        (void)ch->getDescriptors(true);
        const char *tag = ch->getUUID().toString().c_str();
        if (ch->canNotify()) {
            if (ch->subscribe(true, onHidNotify, true)) {
                subscribed = true;
                logf("subscribe notify %s handle=%u", tag,
                     static_cast<unsigned>(ch->getHandle()));
            } else {
                logf("subscribe FAIL %s handle=%u", tag,
                     static_cast<unsigned>(ch->getHandle()));
            }
        } else if (ch->canIndicate()) {
            if (ch->subscribe(false, onHidNotify, true)) {
                subscribed = true;
                logf("subscribe indicate %s", tag);
            }
        }
    }
    const uint8_t ledOff = 0;
    for (auto *ch : *charsPtr) {
        if (!ch || !ch->getUUID().equals(NimBLEUUID(kUuidReport))) {
            continue;
        }
        if (!(ch->canWrite() || ch->canWriteNoResponse())) {
            continue;
        }
        if (ch->canNotify() || ch->canIndicate()) {
            continue;
        }
        (void)ch->writeValue(&ledOff, 1, true);
        logf("write hidOutput leds=0");
    }
    return subscribed;
}

bool connectAndSubscribe(const char *address, uint8_t addrType, const char *nameHint) {
    if (!g_ready || !address || !address[0]) {
        return false;
    }
    if (hidAddrLive(address)) {
        logf("already connected addr=%s", address);
        return true;
    }
    const int slot = allocHidLink(address);
    if (slot < 0) {
        logf("no free HID slot (max=%u)", static_cast<unsigned>(kMaxHidLinks));
        return false;
    }
    NimBLEDevice::getScan()->stop();
    g_client = ensureHidClient(slot);
    if (!g_client) {
        return false;
    }
    if (g_client->isConnected()) {
        g_client->disconnect();
        delay(200);
    }
    std::strncpy(g_links[slot].addr, address, sizeof(g_links[slot].addr) - 1);

    NimBLEAddress addr(address, addrType);
    logf("connect addr=%s type=%u", address, static_cast<unsigned>(addrType));
    if (!g_client->connect(addr)) {
        const uint8_t other = addrType == 1 ? 0 : 1;
        logf("connect retry type=%u", static_cast<unsigned>(other));
        NimBLEAddress addr2(address, other);
        if (!g_client->connect(addr2)) {
            logf("connect FAIL");
            return false;
        }
    }
    g_authComplete = false;
    if (!g_client->secureConnection()) {
        logf("secureConnection soft-fail (continuing)");
        (void)NimBLEDevice::startSecurity(g_client->getConnId());
    }
    for (int i = 0; i < 40 && !g_authComplete; ++i) {
        delay(50);
    }
    NimBLEConnInfo info = g_client->getConnInfo();
    logf("auth wait complete=%d enc=%d bonded=%d mtu=%u", g_authComplete ? 1 : 0,
         info.isEncrypted() ? 1 : 0, info.isBonded() ? 1 : 0,
         static_cast<unsigned>(info.getMTU()));
    delay(80);
    (void)g_client->discoverAttributes();
    g_client->updateConnParams(12, 24, 0, 400);

    NimBLERemoteService *hid = nullptr;
    for (int n = 0; n < 8 && !hid; ++n) {
        hid = g_client->getService(NimBLEUUID(kUuidHidService));
        if (!hid) {
            delay(120);
        }
    }
    if (!hid) {
        logf("no HID service 0x1812");
        g_client->disconnect();
        return false;
    }

    g_isKeyboard = false;
    g_isGamepad = false;
    g_isMouse = false;
    g_caps = {};
    auto *reportMap = hid->getCharacteristic(NimBLEUUID(kUuidReportMap));
    if (reportMap) {
        std::string v = reportMap->readValue();
        g_caps = parseReportMapCaps(reinterpret_cast<const uint8_t *>(v.data()),
                                    static_cast<uint16_t>(v.size()));
        const ScanDeviceKind kind = kindFromCaps(g_caps);
        g_isKeyboard = g_caps.keyboard;
        g_isGamepad = g_caps.gamepad || g_caps.joystick;
        g_isMouse = g_caps.mouse;
        if (g_isKeyboard) {
            g_isMouse = true;
        }
        logf("reportMap len=%u kind=%s kb=%d mouse=%d gp=%d joy=%d",
             static_cast<unsigned>(v.size()), kindName(kind), g_caps.keyboard ? 1 : 0,
             g_caps.mouse ? 1 : 0, g_caps.gamepad ? 1 : 0, g_caps.joystick ? 1 : 0);
        // Compact hex fingerprint (first 24 bytes) for compatibility diagnostics.
        char fp[80]{};
        const size_t nfp = v.size() < 24 ? v.size() : 24;
        size_t hp = 0;
        for (size_t i = 0; i < nfp && hp + 3 < sizeof(fp); ++i) {
            hp += static_cast<size_t>(snprintf(
                fp + hp, sizeof(fp) - hp, "%02X%s", static_cast<uint8_t>(v[i]),
                (i + 1 < nfp) ? "" : ""));
        }
        logf("reportMap_fp=%s%s", fp, v.size() > 24 ? "…" : "");
    }
    auto nameLooksLike = [](const char *n, const char *sub) -> bool {
        if (!n || !n[0] || !sub) {
            return false;
        }
        std::string lower = n;
        for (char &c : lower) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return lower.find(sub) != std::string::npos;
    };
    const bool namePad = nameLooksLike(nameHint, "game") || nameLooksLike(nameHint, "controller") ||
                         nameLooksLike(nameHint, "joystick") || nameLooksLike(nameHint, "joypad") ||
                         nameLooksLike(nameHint, "t3") || nameLooksLike(nameHint, "terios");
    if (namePad) {
        g_isGamepad = true;
    }
    if (!g_isKeyboard && !g_isGamepad && !g_isMouse) {
        // Fall back to name heuristics
        ScanDeviceKind hint = ScanDeviceKind::Unknown;
        if (nameHint && nameHint[0]) {
            if (nameLooksLike(nameHint, "keyboard") || nameLooksLike(nameHint, "dialpad")) {
                hint = ScanDeviceKind::Keyboard;
            } else if (namePad) {
                hint = ScanDeviceKind::Gamepad;
            }
        }
        g_isKeyboard = (hint == ScanDeviceKind::Keyboard);
        g_isGamepad = (hint == ScanDeviceKind::Gamepad);
        if (!g_isKeyboard && !g_isGamepad) {
            // Unknown HID: accept keyboard-shaped reports + pointer reports.
            g_isKeyboard = true;
            g_isMouse = true;
        }
    }

    auto writeU8Early = [](NimBLERemoteCharacteristic *ch, uint8_t v, const char *tag) {
        if (!ch) {
            return;
        }
        if (!(ch->canWrite() || ch->canWriteNoResponse())) {
            return;
        }
        if (ch->writeValue(&v, 1, true)) {
            logf("write %s=0x%02X", tag, static_cast<unsigned>(v));
        } else {
            logf("write FAIL %s", tag);
        }
    };

    std::strncpy(g_connAddr, address, sizeof(g_connAddr) - 1);
    if (nameHint && nameHint[0]) {
        std::strncpy(g_connName, nameHint, sizeof(g_connName) - 1);
    }
    {
        NimBLERemoteService *gap = g_client->getService(NimBLEUUID((uint16_t)0x1800));
        if (gap) {
            NimBLERemoteCharacteristic *dn = gap->getCharacteristic(NimBLEUUID((uint16_t)0x2A00));
            if (dn) {
                const std::string v = dn->readValue();
                if (!v.empty()) {
                    std::strncpy(g_connName, v.c_str(), sizeof(g_connName) - 1);
                    if (nameLooksLike(g_connName, "t3") || nameLooksLike(g_connName, "terios") ||
                        nameLooksLike(g_connName, "game") ||
                        nameLooksLike(g_connName, "controller")) {
                        g_isGamepad = true;
                    }
                }
            }
        }
    }

    writeU8Early(hid->getCharacteristic(NimBLEUUID(kUuidHidControl)), 1, "hidControl");
    writeU8Early(hid->getCharacteristic(NimBLEUUID(kUuidProtocolMode)), 0, "protocolMode");
    bool subscribed = subscribeHidInputs(hid);
    writeU8Early(hid->getCharacteristic(NimBLEUUID(kUuidProtocolMode)), 1, "protocolMode");
    subscribed = subscribeHidInputs(hid) || subscribed;
    delay(150);
    subscribed = subscribeHidInputs(hid) || subscribed;

    if (!subscribed) {
        logf("no HID input notify subscribed");
        // Keep link for diagnostics; still mark connected.
    }
    g_hidLogLeft = 64;
    if (g_connName[0]) {
        for (size_t i = 0; i < g_discoveredCount; ++i) {
            if (addrEq(g_discovered[i].address, address)) {
                std::strncpy(g_discovered[i].name, g_connName, sizeof(g_discovered[i].name) - 1);
                break;
            }
        }
    }
    g_connType = g_isKeyboard ? esp2_ble_config::DeviceType::Keyboard
                              : (g_isGamepad ? esp2_ble_config::DeviceType::Gamepad
                                             : esp2_ble_config::DeviceType::Unknown);
    std::strncpy(g_links[slot].addr, address, sizeof(g_links[slot].addr) - 1);
    std::strncpy(g_links[slot].name, g_connName, sizeof(g_links[slot].name) - 1);
    g_links[slot].type = g_connType;
    g_links[slot].keyboard = g_isKeyboard;
    g_links[slot].gamepad = g_isGamepad || g_isMouse;
    g_links[slot].live = true;
    g_connected = true;
    if (g_isKeyboard) {
        std::memset(g_prevKeys, 0, sizeof(g_prevKeys));
        g_prevMod = 0;
    }
    if (g_isGamepad || g_isMouse) {
        g_pad = {};
        g_virtX = 0;
        g_virtY = 0;
        g_mouseBtn0 = false;
        g_mouseBtn1 = false;
    }
    g_pad.connected = true;
    refreshConnectedGlobals();
    logf("HID ready slot=%d links=%d keyboard=%d mouse=%d gamepad=%d", slot, hidLinkCount(),
         g_isKeyboard ? 1 : 0, g_isMouse ? 1 : 0, g_isGamepad ? 1 : 0);
    return true;
}

void fireAction(esp_bracket::LocalAction a, const char *src) {
    if (a == esp_bracket::LocalAction::None) {
        return;
    }
    logf("local action=%s source=%s", esp_bracket::localActionName(a), src ? src : "-");
    if (g_actionHandler) {
        g_actionHandler(a, src);
    }
}

void handleKeyboardReport(const uint8_t *data, uint8_t len, uint32_t nowMs) {
    (void)nowMs;
    uint8_t boot[8]{};
    if (!extractBootKeyboard(data, len, boot)) {
        if (looksLikeRelativeMouse(data, len) || looksLikeAbsPad(data, len)) {
            return;
        }
        uint8_t k = 0;
        const uint8_t start = (len >= 1 && data[0] >= 1 && data[0] <= 15) ? 1 : 0;
        for (uint8_t i = start; i < len && k < 6; ++i) {
            const uint8_t u = data[i];
            // Skip Escape (0x29): it is a common mouse/pad axis byte and toggles
            // Star Blazer pause. Real ESC still arrives via extractBootKeyboard.
            if (u != 0x29 && isHidKeyUsage(u)) {
                boot[2 + k] = u;
                ++k;
            }
        }
        if (k == 0) {
            return;
        }
    }
    const uint8_t *p = boot;
    const uint8_t n = 8;
    const uint8_t mod = p[0];
    const bool ctrl = (mod & 0x11) != 0;
    const bool shift = (mod & 0x22) != 0;
    const bool alt = (mod & 0x44) != 0;
    const bool meta = (mod & 0x88) != 0;

    // Detect newly pressed keys
    for (int i = 2; i < n && i < 8; ++i) {
        const uint8_t usage = p[i];
        if (usage == 0) {
            continue;
        }
        bool wasDown = false;
        for (int j = 0; j < 6; ++j) {
            if (g_prevKeys[j] == usage) {
                wasDown = true;
                break;
            }
        }
        if (wasDown) {
            continue;
        }

        esp_bracket::KeyboardEvent ev{};
        ev.hidUsage = usage;
        ev.pressed = true;
        ev.ctrl = ctrl;
        ev.shift = shift;
        ev.alt = alt;
        ev.meta = meta;

        const esp_bracket::LocalAction local = g_engine.evaluateKeyboard(ev);
        if (local != esp_bracket::LocalAction::None) {
            fireAction(local, "ble_keyboard");
            continue; // consumed
        }

        const uint8_t apple7 = hidUsageToApple7(usage, shift, ctrl);
        if (apple7 && g_keyInject) {
            logf("key usage=0x%02X apple7=0x%02X", static_cast<unsigned>(usage),
                 static_cast<unsigned>(apple7));
            g_keyInject(apple7, true);
        }
    }

    // Key-ups for Apple II path (optional)
    for (int j = 0; j < 6; ++j) {
        const uint8_t prev = g_prevKeys[j];
        if (prev == 0) {
            continue;
        }
        bool still = false;
        for (int i = 2; i < n && i < 8; ++i) {
            if (p[i] == prev) {
                still = true;
                break;
            }
        }
        if (!still && g_keyInject) {
            const uint8_t apple7 = hidUsageToApple7(prev, false, false);
            if (apple7) {
                g_keyInject(apple7, false);
            }
        }
    }

    std::memset(g_prevKeys, 0, sizeof(g_prevKeys));
    for (int i = 2, k = 0; i < n && i < 8 && k < 6; ++i) {
        if (p[i]) {
            g_prevKeys[k++] = p[i];
        }
    }
    g_prevMod = mod;
}

void handleGamepadReport(const uint8_t *data, uint8_t len, uint32_t nowMs) {
    if (!data || len < 2) {
        return;
    }
    noteHidRaw(data, len);
    auto axis8 = [](uint8_t v) -> int16_t {
        const int c = static_cast<int>(v) - 128;
        return static_cast<int16_t>(c * 256);
    };
    uint8_t off = 0;
    if (len >= 3 && data[0] >= 1 && data[0] <= 15) {
        off = 1;
    }
    int axisAt = -1;
    for (uint8_t i = off; i + 1 < len; ++i) {
        if (analogPairAt(data, i, len)) {
            axisAt = static_cast<int>(i);
            break;
        }
    }
    if (axisAt < 0) {
        axisAt = static_cast<int>(off);
        if (axisAt + 1 >= static_cast<int>(len)) {
            return;
        }
    }
    g_pad.leftX = axis8(data[static_cast<uint8_t>(axisAt)]);
    g_pad.leftY = axis8(data[static_cast<uint8_t>(axisAt + 1)]);
    if (axisAt + 3 < static_cast<int>(len) && analogPairAt(data, static_cast<uint8_t>(axisAt + 2), len)) {
        g_pad.rightX = axis8(data[static_cast<uint8_t>(axisAt + 2)]);
        g_pad.rightY = axis8(data[static_cast<uint8_t>(axisAt + 3)]);
    }

    uint16_t buttons = 0;
    uint8_t hat = 0x0F;
    int btnShift = 0;
    for (uint8_t i = off; i < len && btnShift < 16; ++i) {
        if (static_cast<int>(i) == axisAt || static_cast<int>(i) == axisAt + 1) {
            continue;
        }
        if (static_cast<int>(i) == axisAt + 2 || static_cast<int>(i) == axisAt + 3) {
            if (analogPairAt(data, static_cast<uint8_t>(axisAt + 2), len)) {
                continue;
            }
        }
        const uint8_t v = data[i];
        const uint8_t n = static_cast<uint8_t>(v & 0x0F);
        if (v == 0x0F || (v <= 8 && n <= 8)) {
            hat = n;
            applyHatNibble(n);
            continue;
        }
        buttons |= static_cast<uint16_t>(v) << btnShift;
        btnShift += 8;
    }
    g_diag.buttons = buttons;
    g_pad.btnA = (buttons & 0x0001u) != 0;
    g_pad.btnB = (buttons & 0x0002u) != 0;
    g_pad.btnX = (buttons & 0x0004u) != 0;
    g_pad.btnY = (buttons & 0x0008u) != 0;
    g_pad.btnL1 = (buttons & 0x0010u) != 0;
    g_pad.btnR1 = (buttons & 0x0020u) != 0;
    g_pad.btnSelect = (buttons & 0x0040u) != 0;
    g_pad.btnStart = (buttons & 0x0080u) != 0;
    if ((buttons & 0xFF00u) != 0 && (buttons & 0x00FFu) == 0) {
        g_pad.btnA = (buttons & 0x0100u) != 0;
        g_pad.btnB = (buttons & 0x0200u) != 0;
        g_pad.btnX = (buttons & 0x0400u) != 0;
        g_pad.btnY = (buttons & 0x0800u) != 0;
    }
    static uint16_t prevLoggedBtn = 0xFFFFu;
    static uint32_t lastPadLogMs = 0;
    if (buttons != prevLoggedBtn || nowMs - lastPadLogMs > 400) {
        prevLoggedBtn = buttons;
        lastPadLogMs = nowMs;
        logf("pad lx=%d ly=%d btn=0x%04X hat=%u", static_cast<int>(g_pad.leftX),
             static_cast<int>(g_pad.leftY), static_cast<unsigned>(buttons),
             static_cast<unsigned>(hat));
    }
    finishPadApply(nowMs);
}

void processOneHid(const RawHid &r, uint32_t nowMs) {
    if (g_hidLogLeft > 0) {
        g_hidLogLeft--;
        char hex[96]{};
        size_t hp = 0;
        for (uint8_t i = 0; i < r.len && hp + 3 < sizeof(hex); ++i) {
            hp += static_cast<size_t>(
                snprintf(hex + hp, sizeof(hex) - hp, "%02X%s", r.data[i],
                         (i + 1 < r.len) ? " " : ""));
        }
        logf("rpt n=%u %s", static_cast<unsigned>(r.len), hex);
    }

    uint8_t boot[8]{};
    const bool kbExtract = extractBootKeyboard(r.data, r.len, boot);
    const bool absShape = looksLikeAbsPad(r.data, r.len) && !kbExtract;
    const bool btnFirst = looksLikeButtonFirstPad(r.data, r.len) && !kbExtract;
    const bool mouseShape =
        looksLikeRelativeMouse(r.data, r.len) && !kbExtract && !absShape && !btnFirst;
    noteHidRaw(r.data, r.len);

    if (kbExtract || (r.fromKb && !mouseShape && !absShape && !btnFirst)) {
        hidMonDump("keyboard", r.data, r.len, nowMs);
        handleKeyboardReport(r.data, r.len, nowMs);
        return;
    }
    if (absShape || btnFirst) {
        hidMonDump("gamepad", r.data, r.len, nowMs);
        handleGamepadReport(r.data, r.len, nowMs);
        return;
    }
    if (mouseShape) {
        hidMonDump("mouse_rel", r.data, r.len, nowMs);
        handleRelativePointerReport(r.data, r.len, nowMs);
        return;
    }
    if (r.fromGp) {
        hidMonDump("gamepad", r.data, r.len, nowMs);
        handleGamepadReport(r.data, r.len, nowMs);
        return;
    }
    if (r.fromKb) {
        hidMonDump("keyboard", r.data, r.len, nowMs);
        handleKeyboardReport(r.data, r.len, nowMs);
        return;
    }
    hidMonDump("raw", r.data, r.len, nowMs);
}

void processLatestHid(uint32_t nowMs) {
    RawHid r{};
    int n = 0;
    while (n < static_cast<int>(kHidQ) && popHidReport(&r)) {
        processOneHid(r, nowMs);
        ++n;
    }
}

const DiscoveredDevice *findDiscovered(const char *address) {
    for (size_t i = 0; i < g_discoveredCount; ++i) {
        if (addrEq(g_discovered[i].address, address)) {
            return &g_discovered[i];
        }
    }
    return nullptr;
}

bool devicePassesHidFilter(NimBLEAdvertisedDevice *d) {
    return d != nullptr;
}

bool nameIsUseful(const char *n) {
    return n && n[0] && std::strcmp(n, "(unnamed)") != 0;
}

int discoverScore(const DiscoveredDevice &d) {
    int s = d.rssi;
    if (nameIsUseful(d.name)) {
        s += 120;
    }
    if (d.kind != ScanDeviceKind::Unknown) {
        s += 60;
    }
    return s;
}

void extractAdvertisedName(NimBLEAdvertisedDevice *d, char *out, size_t outSz) {
    if (!out || outSz == 0) {
        return;
    }
    out[0] = 0;
    if (!d) {
        return;
    }
    if (d->haveName()) {
        const std::string n = d->getName();
        if (!n.empty()) {
            std::strncpy(out, n.c_str(), outSz - 1);
            out[outSz - 1] = 0;
            return;
        }
    }
    const uint8_t *p = d->getPayload();
    const size_t plen = d->getPayloadLength();
    if (!p || plen < 3) {
        return;
    }
    size_t i = 0;
    while (i + 1 < plen) {
        const uint8_t fieldLen = p[i];
        if (fieldLen == 0) {
            break;
        }
        if (i + 1 + fieldLen > plen) {
            break;
        }
        const uint8_t type = p[i + 1];
        if ((type == 0x08 || type == 0x09) && fieldLen >= 2) {
            const size_t nlen = static_cast<size_t>(fieldLen - 1);
            const size_t copy = nlen < outSz - 1 ? nlen : outSz - 1;
            std::memcpy(out, p + i + 2, copy);
            out[copy] = 0;
            if (type == 0x09) {
                return;
            }
        }
        i += static_cast<size_t>(fieldLen) + 1;
    }
}

void upsertDiscoveredFromAd(NimBLEAdvertisedDevice *d) {
    if (!d) {
        return;
    }
    if (!devicePassesHidFilter(d)) {
        return;
    }
    const std::string addrStr = d->getAddress().toString();
    char nm[sizeof(DiscoveredDevice::name)]{};
    extractAdvertisedName(d, nm, sizeof(nm));
    const ScanDeviceKind kind = classifyAdvertisement(d);
    for (size_t i = 0; i < g_discoveredCount; ++i) {
        if (!addrEq(g_discovered[i].address, addrStr.c_str())) {
            continue;
        }
        g_discovered[i].rssi = d->getRSSI();
        g_discovered[i].kind = kind;
        g_discovered[i].known = esp2_ble_config::findKnownDevice(g_cfg, g_discovered[i].address) >= 0;
        g_discovered[i].connected = hidAddrLive(g_discovered[i].address);
        if (nameIsUseful(nm) && !nameIsUseful(g_discovered[i].name)) {
            std::strncpy(g_discovered[i].name, nm, sizeof(g_discovered[i].name) - 1);
        } else if (nameIsUseful(nm) && std::strlen(nm) > std::strlen(g_discovered[i].name)) {
            std::strncpy(g_discovered[i].name, nm, sizeof(g_discovered[i].name) - 1);
        }
        return;
    }
    DiscoveredDevice hit{};
    if (nameIsUseful(nm)) {
        std::strncpy(hit.name, nm, sizeof(hit.name) - 1);
    }
    if (!nameIsUseful(hit.name) && kind == ScanDeviceKind::Unknown) {
        return;
    }
    std::strncpy(hit.address, addrStr.c_str(), sizeof(hit.address) - 1);
    hit.addrType = d->getAddressType();
    hit.rssi = d->getRSSI();
    hit.kind = kind;
    hit.known = esp2_ble_config::findKnownDevice(g_cfg, hit.address) >= 0;
    hit.connected = hidAddrLive(hit.address);
    if (g_discoveredCount < kScanMax) {
        g_discovered[g_discoveredCount++] = hit;
        return;
    }
    int worst = -1;
    int worstScore = 100000;
    for (size_t i = 0; i < g_discoveredCount; ++i) {
        const int sc = discoverScore(g_discovered[i]);
        if (sc < worstScore) {
            worstScore = sc;
            worst = static_cast<int>(i);
        }
    }
    if (worst >= 0 && discoverScore(hit) > worstScore) {
        g_discovered[static_cast<size_t>(worst)] = hit;
    }
}

class ScanCb : public NimBLEAdvertisedDeviceCallbacks {
public:
    void onResult(NimBLEAdvertisedDevice *advertisedDevice) override {
        if (!advertisedDevice) {
            return;
        }
        upsertDiscoveredFromAd(advertisedDevice);
    }
};

static void onScanComplete(NimBLEScanResults results) {
    const int n = results.getCount();
    for (int i = 0; i < n; ++i) {
        NimBLEAdvertisedDevice d = results.getDevice(i);
        upsertDiscoveredFromAd(&d);
    }
    g_scanning = false;
    g_scanAsync = false;
    g_opBusy = false;
    g_opState = LinkState::KnownOffline;
    logf("scan async end count=%u ads=%d", static_cast<unsigned>(g_discoveredCount), n);
}

ScanCb g_scanCb;

enum class BleOpKind : uint8_t { Connect = 1, Pair = 2, Disconnect = 3, Scan = 4 };

struct BleOpMsg {
    BleOpKind kind = BleOpKind::Connect;
    char address[20]{};
    uint8_t addrType = 0;
    uint32_t durationSec = 0;
};

void bleWorkerTask(void *) {
    BleOpMsg msg{};
    for (;;) {
        if (xQueueReceive(g_bleOpQueue, &msg, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        g_opBusy = true;
        switch (msg.kind) {
        case BleOpKind::Disconnect:
            g_opState = LinkState::Connecting;
            disconnectAddress(msg.address[0] ? msg.address : nullptr);
            g_opState = LinkState::KnownOffline;
            g_opBusy = false;
            break;
        case BleOpKind::Connect:
            g_opState = LinkState::Connecting;
            if (connectAddress(msg.address)) {
                g_opState = LinkState::Connected;
            } else {
                g_opState = LinkState::Error;
            }
            g_opBusy = false;
            break;
        case BleOpKind::Pair:
            g_opState = LinkState::Pairing;
            if (pairAddress(msg.address)) {
                g_opState = LinkState::Connected;
            } else {
                g_opState = LinkState::Error;
            }
            g_opBusy = false;
            break;
        case BleOpKind::Scan:
            g_opState = LinkState::Discovered;
            (void)scan(msg.durationSec != 0 ? msg.durationSec : kDefaultScanSec);
            g_scanAsync = false;
            g_scanning = false;
            g_suppressAutoReconnect = false;
            if (g_pendingPair[0]) {
                char paddr[20]{};
                std::strncpy(paddr, g_pendingPair, sizeof(paddr) - 1);
                g_pendingPair[0] = 0;
                g_opState = LinkState::Pairing;
                if (pairAddress(paddr)) {
                    g_opState = LinkState::Connected;
                } else {
                    g_opState = LinkState::Error;
                }
            }
            g_opBusy = false;
            break;
        }
    }
}

bool ensureBleWorker() {
    if (g_bleOpQueue && g_bleWorker) {
        return true;
    }
    if (!g_bleOpQueue) {
        g_bleOpQueue = xQueueCreate(4, sizeof(BleOpMsg));
    }
    if (!g_bleOpQueue) {
        return false;
    }
    if (!g_bleWorker) {
        xTaskCreatePinnedToCore(bleWorkerTask, "blehid", 12288, nullptr, 2, &g_bleWorker, 1);
    }
    return g_bleWorker != nullptr;
}

bool queueBleOp(BleOpKind kind, const char *address, uint8_t addrType) {
    if (!ensureBleWorker()) {
        return false;
    }
    BleOpMsg msg{};
    msg.kind = kind;
    msg.addrType = addrType;
    msg.durationSec = 0;
    if (address) {
        std::strncpy(msg.address, address, sizeof(msg.address) - 1);
        std::strncpy(g_opAddr, address, sizeof(g_opAddr) - 1);
    } else {
        g_opAddr[0] = 0;
    }
    if (kind == BleOpKind::Pair || kind == BleOpKind::Connect) {
        g_opState = LinkState::Connecting;
    }
    if (xQueueSend(g_bleOpQueue, &msg, 0) != pdTRUE) {
        return false;
    }
    g_opBusy = true;
    return true;
}

bool queueFirstAutoReconnect() {
    if (!g_ready || g_suppressAutoReconnect) {
        return false;
    }
    if (hidLinkCount() >= static_cast<int>(kMaxHidLinks)) {
        return true;
    }
    if (g_scanning || g_pendingPair[0] || g_opBusy) {
        return false;
    }
    if (g_cfg.knownCount == 0) {
        return false;
    }
    const uint32_t now = millis();
    auto tryKnown = [&](bool keyboardsOnly) -> bool {
        for (size_t n = 0; n < g_cfg.knownCount; ++n) {
            const size_t i = (g_autoReconnectIndex + n) % g_cfg.knownCount;
            const auto &d = g_cfg.known[i];
            if (!d.autoReconnect || !d.address[0] || hidAddrLive(d.address)) {
                continue;
            }
            const bool isKb = d.type == esp2_ble_config::DeviceType::Keyboard;
            if (keyboardsOnly != isKb) {
                continue;
            }
            if (g_reconnectBackoffAddr[0] && addrEq(g_reconnectBackoffAddr, d.address) &&
                now < g_reconnectBackoffUntilMs) {
                continue;
            }
            g_autoReconnectIndex = (i + 1) % g_cfg.knownCount;
            logf("auto-reconnect addr=%s name=\"%s\" kb=%d links=%d", d.address, d.name,
                 isKb ? 1 : 0, hidLinkCount());
            uint8_t addrType = d.addrType;
            const DiscoveredDevice *found = findDiscovered(d.address);
            if (found) {
                addrType = found->addrType;
            }
            return queueBleOp(BleOpKind::Connect, d.address, addrType);
        }
        return false;
    };
    if (tryKnown(true)) {
        return true;
    }
    return tryKnown(false);
}

} // namespace

bool init() {
    if (g_ready) {
        return true;
    }
    NimBLEDevice::init("ESP][");
    NimBLEDevice::setMTU(517);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
    NimBLEDevice::setSecurityAuth(true /*bonding*/, false /*mitm*/, true /*sc*/);
    NimBLEDevice::setSecurityInitKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
    NimBLEDevice::setSecurityRespKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    NimBLEDevice::getScan()->setAdvertisedDeviceCallbacks(&g_scanCb, true);
    (void)ensureBleWorker();
    g_engine.setConfig(g_cfg.control);
    g_ready = true;
    logf("init ok bonding=1 max_simultaneous=%u",
         static_cast<unsigned>(status().maxSimultaneousConnections));
    return true;
}

bool ready() { return g_ready; }

bool scan(uint32_t durationSec) {
    if (!g_ready) {
        return false;
    }
    if (durationSec == 0) {
        durationSec = kDefaultScanSec;
    }
    if (durationSec > 30) {
        durationSec = 30;
    }
    g_scanning = true;
    g_discoveredCount = 0;
    std::memset(g_discovered, 0, sizeof(g_discovered));

    NimBLEScan *scan = NimBLEDevice::getScan();
    scan->stop();
    scan->setActiveScan(true);
    scan->setInterval(80);
    scan->setWindow(80);
    scan->setMaxResults(kScanMax);
    scan->setDuplicateFilter(false);
    logf("scan start sec=%u", static_cast<unsigned>(durationSec));
    NimBLEScanResults results = scan->start(durationSec, false);
    const int n = results.getCount();
    for (int i = 0; i < n; ++i) {
        NimBLEAdvertisedDevice d = results.getDevice(i);
        upsertDiscoveredFromAd(&d);
    }
    scan->clearResults();
    g_scanning = false;
    logf("scan done count=%u", static_cast<unsigned>(g_discoveredCount));
    return true;
}

bool beginScanAsync(uint32_t durationSec) {
    if (!g_ready) {
        logf("scan async not ready");
        return false;
    }
    if (g_scanning && g_scanAsync) {
        logf("scan async already running count=%u", static_cast<unsigned>(g_discoveredCount));
        return true;
    }
    if (!ensureBleWorker()) {
        return false;
    }
    g_suppressAutoReconnect = true;
    if (durationSec == 0) {
        durationSec = kDefaultScanSec;
    }
    if (durationSec > 30) {
        durationSec = 30;
    }
    BleOpMsg msg{};
    msg.kind = BleOpKind::Scan;
    msg.durationSec = durationSec;
    g_discoveredCount = 0;
    std::memset(g_discovered, 0, sizeof(g_discovered));
    g_scanning = true;
    g_scanAsync = true;
    g_opState = LinkState::Discovered;
    logf("scan async queue sec=%u busy=%d", static_cast<unsigned>(durationSec), g_opBusy ? 1 : 0);
    if (xQueueSend(g_bleOpQueue, &msg, 0) != pdTRUE) {
        g_scanning = false;
        g_scanAsync = false;
        logf("scan async FAIL queue");
        return false;
    }
    g_opBusy = true;
    return true;
}

void cancelScanAsync() {
    if (!g_scanning) {
        return;
    }
    NimBLEDevice::getScan()->stop();
    g_scanning = false;
    g_scanAsync = false;
}

bool scanAsyncActive() { return g_scanAsync && g_scanning; }

size_t discoveredCount() { return g_discoveredCount; }

bool getDiscovered(size_t index, DiscoveredDevice *out) {
    if (!out || index >= g_discoveredCount) {
        return false;
    }
    *out = g_discovered[index];
    return true;
}

const esp2_ble_config::BluetoothConfig &config() { return g_cfg; }

void setConfig(const esp2_ble_config::BluetoothConfig &cfg) {
    g_cfg = cfg;
    g_engine.setConfig(g_cfg.control);
}

bool saveConfigToSd() {
    char buf[3072];
    const size_t n = esp2_ble_config::serializeBluetoothConfigJson(g_cfg, buf, sizeof(buf));
    if (n == 0) {
        logf("serialize FAIL");
        return false;
    }
    if (!ESP2_SD.exists("/esp2")) {
        ESP2_SD.mkdir("/esp2");
    }
    if (!ESP2_SD.exists("/esp2/config")) {
        ESP2_SD.mkdir("/esp2/config");
    }
    File f = ESP2_SD.open(esp2_ble_config::kBluetoothConfigPath, FILE_WRITE);
    if (!f) {
        logf("save open FAIL path=%s", esp2_ble_config::kBluetoothConfigPath);
        return false;
    }
    f.write(reinterpret_cast<const uint8_t *>(buf), n);
    f.close();
    logf("saved known=%u path=%s", static_cast<unsigned>(g_cfg.knownCount),
         esp2_ble_config::kBluetoothConfigPath);
    return true;
}

bool loadConfigFromSd() {
    if (!ESP2_SD.exists(esp2_ble_config::kBluetoothConfigPath)) {
        g_cfg = esp2_ble_config::defaultBluetoothConfig();
        g_engine.setConfig(g_cfg.control);
        logf("config missing → defaults");
        return false;
    }
    File f = ESP2_SD.open(esp2_ble_config::kBluetoothConfigPath, FILE_READ);
    if (!f) {
        g_cfg = esp2_ble_config::defaultBluetoothConfig();
        g_engine.setConfig(g_cfg.control);
        return false;
    }
    char buf[3072];
    const size_t n = f.readBytes(buf, sizeof(buf) - 1);
    f.close();
    buf[n] = 0;
    char err[48]{};
    g_cfg = esp2_ble_config::parseBluetoothConfigJson(buf, n, err, sizeof(err));
    if (!g_cfg.valid) {
        logf("config parse FAIL %s → defaults", err);
        g_cfg = esp2_ble_config::defaultBluetoothConfig();
    }
    g_engine.setConfig(g_cfg.control);
    logf("config loaded known=%u valid=%d", static_cast<unsigned>(g_cfg.knownCount),
         g_cfg.valid ? 1 : 0);
    return g_cfg.valid;
}

bool pairAddress(const char *address) {
    if (!address || !address[0]) {
        return false;
    }
    const DiscoveredDevice *d = findDiscovered(address);
    uint8_t addrType = 1;
    const char *name = "";
    ScanDeviceKind kind = ScanDeviceKind::Unknown;
    if (d) {
        addrType = d->addrType;
        name = d->name;
        kind = d->kind;
    } else {
        const int ki = esp2_ble_config::findKnownDevice(g_cfg, address);
        if (ki >= 0) {
            addrType = g_cfg.known[static_cast<size_t>(ki)].addrType;
            name = g_cfg.known[static_cast<size_t>(ki)].name;
            kind = g_cfg.known[static_cast<size_t>(ki)].type ==
                           esp2_ble_config::DeviceType::Keyboard
                       ? ScanDeviceKind::Keyboard
                       : ScanDeviceKind::Gamepad;
        }
    }
    if (!connectAndSubscribe(address, addrType, name)) {
        return false;
    }
    esp2_ble_config::KnownDevice kd{};
    std::strncpy(kd.address, address, sizeof(kd.address) - 1);
    kd.addrType = addrType;
    const char *storeName = nameIsUseful(g_connName) ? g_connName : name;
    std::strncpy(kd.name, storeName ? storeName : "", sizeof(kd.name) - 1);
    if (g_isKeyboard || kind == ScanDeviceKind::Keyboard || kind == ScanDeviceKind::Composite) {
        kd.type = esp2_ble_config::DeviceType::Keyboard;
    } else if (g_isGamepad || kind == ScanDeviceKind::Gamepad) {
        kd.type = esp2_ble_config::DeviceType::Gamepad;
    } else {
        kd.type = esp2_ble_config::DeviceType::Unknown;
    }
    kd.autoReconnect = true;
    kd.bonded = true;
    esp2_ble_config::upsertKnownDevice(&g_cfg, kd);
    saveConfigToSd();
    return true;
}

bool connectAddress(const char *address) {
    if (!address || !address[0]) {
        return false;
    }
    uint8_t addrType = 1;
    const char *name = "";
    const DiscoveredDevice *d = findDiscovered(address);
    if (d) {
        addrType = d->addrType;
        name = d->name;
    } else {
        const int ki = esp2_ble_config::findKnownDevice(g_cfg, address);
        if (ki < 0) {
            logf("connect unknown addr=%s", address);
            return false;
        }
        addrType = g_cfg.known[static_cast<size_t>(ki)].addrType;
        name = g_cfg.known[static_cast<size_t>(ki)].name;
    }
    if (!connectAndSubscribe(address, addrType, name)) {
        std::strncpy(g_reconnectBackoffAddr, address, sizeof(g_reconnectBackoffAddr) - 1);
        g_reconnectBackoffUntilMs = millis() + 2500;
        logf("connect backoff addr=%s ms=2500", address);
        return false;
    }
    return true;
}

void disconnect() { disconnectAddress(nullptr); }

bool forgetAddress(const char *address) {
    if (!esp2_ble_config::forgetKnownDevice(&g_cfg, address)) {
        return false;
    }
    disconnectAddress(address);
    // Best-effort: clear NimBLE bond store entry is stack-version dependent.
    saveConfigToSd();
    logf("forgot addr=%s", address);
    return true;
}

BleHostStatus status() {
    BleHostStatus s{};
    s.ready = g_ready;
    s.scanning = g_scanning;
    s.opBusy = g_opBusy;
    s.lastOpState = g_opState;
    refreshConnectedGlobals();
    s.connected = g_connected;
    std::strncpy(s.connectedAddr, g_connAddr, sizeof(s.connectedAddr) - 1);
    std::strncpy(s.connectedName, g_connName, sizeof(s.connectedName) - 1);
    s.connectedType = g_connType;
    s.knownCount = g_cfg.knownCount;
    s.discoveredCount = g_discoveredCount;
    s.maxSimultaneousConnections = static_cast<uint8_t>(kMaxHidLinks);
    return s;
}

LinkState linkStateForAddress(const char *address) {
    if (!address || !address[0]) {
        return LinkState::KnownOffline;
    }
    if (hidAddrLive(address)) {
        return LinkState::Connected;
    }
    if (g_opBusy && addrEq(g_opAddr, address)) {
        return g_opState;
    }
    if (esp2_ble_config::findKnownDevice(g_cfg, address) >= 0) {
        return LinkState::KnownOffline;
    }
    for (size_t i = 0; i < g_discoveredCount; ++i) {
        if (addrEq(g_discovered[i].address, address)) {
            return LinkState::Discovered;
        }
    }
    return LinkState::KnownOffline;
}

bool connectedCaps(ConnectedCaps *out) {
    if (!out) {
        return false;
    }
    *out = ConnectedCaps{};
    if (!g_connected) {
        return false;
    }
    out->keyboard = g_caps.keyboard || g_isKeyboard;
    out->mouse = g_caps.mouse || g_isMouse;
    out->gamepad = g_caps.gamepad || g_isGamepad;
    out->joystick = g_caps.joystick;
    for (size_t i = 0; i < kMaxHidLinks; ++i) {
        if (!g_links[i].live) {
            continue;
        }
        out->keyboard = out->keyboard || g_links[i].keyboard;
        out->gamepad = out->gamepad || g_links[i].gamepad;
        out->mouse = out->mouse || g_links[i].gamepad;
    }
    return true;
}

bool requestConnect(const char *address) {
    if (!address || !address[0]) {
        return false;
    }
    g_suppressAutoReconnect = false;
    cancelScanAsync();
    uint8_t addrType = 1;
    const DiscoveredDevice *d = findDiscovered(address);
    if (d) {
        addrType = d->addrType;
    } else {
        const int ki = esp2_ble_config::findKnownDevice(g_cfg, address);
        if (ki < 0) {
            return false;
        }
        addrType = g_cfg.known[static_cast<size_t>(ki)].addrType;
    }
    return queueBleOp(BleOpKind::Connect, address, addrType);
}

bool requestPair(const char *address) {
    if (!address || !address[0]) {
        return false;
    }
    uint8_t addrType = 1;
    const DiscoveredDevice *d = findDiscovered(address);
    if (d) {
        addrType = d->addrType;
    }
    g_suppressAutoReconnect = false;
    cancelScanAsync();
    if (g_opBusy || g_scanning) {
        std::strncpy(g_pendingPair, address, sizeof(g_pendingPair) - 1);
        logf("pair queued after scan addr=%s", address);
        return true;
    }
    return queueBleOp(BleOpKind::Pair, address, addrType);
}

bool requestDisconnect(const char *address) {
    return queueBleOp(BleOpKind::Disconnect, address, 0);
}

bool requestAutoReconnect() {
    g_suppressAutoReconnect = false;
    g_nextAutoReconnectMs = 0;
    return queueFirstAutoReconnect();
}

void service(uint32_t nowMs) {
    if (!g_ready) {
        return;
    }
    refreshConnectedGlobals();
    processLatestHid(nowMs);
    if (nowMs - g_bleHbMs >= 5000) {
        g_bleHbMs = nowMs;
        const uint32_t age =
            (g_lastHidNotifyMs == 0) ? 0 : (nowMs - g_lastHidNotifyMs);
        const char *hbAddr = "-";
        int enc = -1;
        for (size_t i = 0; i < kMaxHidLinks; ++i) {
            if (g_links[i].live && g_links[i].client && g_links[i].client->isConnected()) {
                hbAddr = g_links[i].addr;
                enc = g_links[i].client->getConnInfo().isEncrypted() ? 1 : 0;
                break;
            }
        }
        logf("hb links=%d addr=%s enc=%d q=%u notify=%u age_ms=%u kb=%d", hidLinkCount(), hbAddr,
             enc, static_cast<unsigned>(g_hidQCount), static_cast<unsigned>(g_hidNotifyCount),
             static_cast<unsigned>(age), g_isKeyboard ? 1 : 0);
    }
    if (g_suppressAutoReconnect || g_scanning) {
        return;
    }
    if (hidLinkCount() >= static_cast<int>(kMaxHidLinks)) {
        return;
    }
    if (nowMs < g_nextAutoReconnectMs) {
        return;
    }
    g_nextAutoReconnectMs = nowMs + kAutoReconnectRetryMs;
    (void)queueFirstAutoReconnect();
}

void setLocalActionHandler(LocalActionHandler handler) { g_actionHandler = handler; }

void setInjectors(KeyInjectFn keyFn, PadInjectFn padFn) {
    g_keyInject = keyFn;
    g_padInject = padFn;
}

esp_bracket::GamepadState latestGamepad() { return g_pad; }

PadDiag latestPadDiag() {
    PadDiag d = g_diag;
    const uint32_t now = millis();
    d.notifyAgeMs = (g_lastHidNotifyMs == 0) ? 0 : (now - g_lastHidNotifyMs);
    d.connected = g_pad.connected;
    d.leftX = g_pad.leftX;
    d.leftY = g_pad.leftY;
    d.pb0 = d.pb0 || g_pad.btnA || g_pad.btnB || g_pad.btnX || g_pad.btnY;
    return d;
}

void setHidMonitor(bool enabled) { g_hidMonitor = enabled; }
bool hidMonitorEnabled() { return g_hidMonitor; }

void setPadMapMode(PadMapMode mode) { g_padMode = mode; }
PadMapMode padMapMode() { return g_padMode; }

void setPadSensitivity(uint8_t sensitivity) {
    if (sensitivity < 1) {
        sensitivity = 1;
    }
    if (sensitivity > 32) {
        sensitivity = 32;
    }
    g_padSensitivity = sensitivity;
}
uint8_t padSensitivity() { return g_padSensitivity; }

void recenterVirtualPad() {
    g_virtX = 0;
    g_virtY = 0;
    g_pad.leftX = 0;
    g_pad.leftY = 0;
}

} // namespace esp2_ble
