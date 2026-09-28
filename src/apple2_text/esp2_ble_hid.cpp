#include "esp2_ble_hid.hpp"

#include "esp_bracket/gamepad_mapper.hpp"
#include "esp_bracket/key_map.hpp"

#include <NimBLEDevice.h>
#include <SD.h>

#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace esp2_ble {
namespace {

static constexpr uint16_t kUuidHidService = 0x1812;
static constexpr uint16_t kUuidReportMap = 0x2A4B;
static constexpr uint16_t kUuidReport = 0x2A4D;
static constexpr uint16_t kUuidHidInfo = 0x2A4A;

esp2_ble_config::BluetoothConfig g_cfg = esp2_ble_config::defaultBluetoothConfig();
esp_bracket::LocalControlEngine g_engine;
LocalActionHandler g_actionHandler = nullptr;
KeyInjectFn g_keyInject = nullptr;
PadInjectFn g_padInject = nullptr;

bool g_ready = false;
bool g_scanning = false;
NimBLEClient *g_client = nullptr;
bool g_connected = false;
char g_connAddr[20]{};
char g_connName[40]{};
esp2_ble_config::DeviceType g_connType = esp2_ble_config::DeviceType::Unknown;
bool g_isKeyboard = false;
bool g_isGamepad = false;

DiscoveredDevice g_discovered[kScanMax]{};
size_t g_discoveredCount = 0;

esp_bracket::GamepadState g_pad{};
esp_bracket::GamepadMapper g_mapper{};

uint8_t g_prevKeys[6]{};
uint8_t g_prevMod = 0;

portMUX_TYPE g_hidMux = portMUX_INITIALIZER_UNLOCKED;
struct RawHid {
    bool valid = false;
    uint8_t len = 0;
    uint8_t data[64]{};
    uint32_t seq = 0;
};
RawHid g_latest{};
uint32_t g_hidSeq = 0;

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

ScanDeviceKind classifyAdvertisement(NimBLEAdvertisedDevice *d) {
    if (!d) {
        return ScanDeviceKind::Unknown;
    }
    // Appearance / service UUID hints — preserve advertised name as-is (incl. BlueShift).
    if (d->isAdvertisingService(NimBLEUUID((uint16_t)kUuidHidService))) {
        // Without Report Map we only know HID; refine after connect.
        return ScanDeviceKind::OtherHid;
    }
    const std::string name = d->getName();
    // Heuristic name hints only for UI classification — not vendor special-cases.
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
    if (has("keyboard") || has("kbd")) {
        return ScanDeviceKind::Keyboard;
    }
    if (has("gamepad") || has("controller") || has("joystick") || has("joypad")) {
        return ScanDeviceKind::Gamepad;
    }
    return ScanDeviceKind::Unknown;
}

ScanDeviceKind refineFromReportMap(const uint8_t *map, uint16_t len) {
    bool gamepad = false, joystick = false, keyboard = false;
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
                if (usage == 0x04) {
                    joystick = true;
                }
                if (usage == 0x05) {
                    gamepad = true;
                }
                if (usage == 0x06) {
                    keyboard = true;
                }
            } else if (usagePage == 0x07) {
                keyboard = true;
            }
        }
    }
    if (keyboard && !(gamepad || joystick)) {
        return ScanDeviceKind::Keyboard;
    }
    if (gamepad || joystick) {
        return ScanDeviceKind::Gamepad;
    }
    if (keyboard) {
        return ScanDeviceKind::Keyboard;
    }
    return ScanDeviceKind::OtherHid;
}

uint8_t hidUsageToApple7(uint8_t usage, bool shift, bool ctrl) {
    // Letters A–Z
    if (usage >= 0x04 && usage <= 0x1D) {
        char c = static_cast<char>('A' + (usage - 0x04));
        uint8_t out = 0;
        if (esp_bracket::AppleIIKeyMap::mapHostKey(c, shift, ctrl, &out)) {
            return out;
        }
        return esp_bracket::AppleIIKeyMap::fromAscii(c);
    }
    // Digits 1–9,0
    if (usage >= 0x1E && usage <= 0x27) {
        const char digits[] = "1234567890";
        return esp_bracket::AppleIIKeyMap::fromAscii(digits[usage - 0x1E]);
    }
    switch (usage) {
    case 0x28:
        return esp_bracket::AppleIIKeyMap::fromAscii('\r'); // Enter
    case 0x2A:
        return 0x08; // Backspace
    case 0x2C:
        return esp_bracket::AppleIIKeyMap::fromAscii(' ');
    case 0x29:
        return 0x1B; // Escape
    case 0x2B:
        return 0x09; // Tab
    default:
        return 0;
    }
}

void onHidNotify(NimBLERemoteCharacteristic *chr, uint8_t *data, size_t length, bool) {
    (void)chr;
    if (!data || length == 0) {
        return;
    }
    RawHid r{};
    r.valid = true;
    r.seq = ++g_hidSeq;
    r.len = static_cast<uint8_t>(length > sizeof(r.data) ? sizeof(r.data) : length);
    std::memcpy(r.data, data, r.len);
    portENTER_CRITICAL(&g_hidMux);
    g_latest = r;
    portEXIT_CRITICAL(&g_hidMux);
}

class ClientCb : public NimBLEClientCallbacks {
public:
    void onConnect(NimBLEClient *) override {
        g_connected = true;
        logf("connected");
    }
    void onDisconnect(NimBLEClient *) override {
        g_connected = false;
        g_connAddr[0] = 0;
        logf("disconnected");
    }
};

ClientCb g_clientCb;

bool connectAndSubscribe(const char *address, uint8_t addrType, const char *nameHint) {
    if (!g_ready || !address || !address[0]) {
        return false;
    }
    NimBLEDevice::getScan()->stop();
    if (!g_client) {
        g_client = NimBLEDevice::createClient();
        g_client->setClientCallbacks(&g_clientCb, false);
    }
    if (g_client->isConnected()) {
        g_client->disconnect();
        delay(200);
    }

    NimBLEAddress addr(address, addrType);
    logf("connect addr=%s type=%u", address, static_cast<unsigned>(addrType));
    if (!g_client->connect(addr)) {
        logf("connect FAIL");
        return false;
    }

    // Optional secure connection / bond
    if (!g_client->secureConnection()) {
        logf("secureConnection soft-fail (continuing)");
    }

    NimBLERemoteService *hid = g_client->getService(NimBLEUUID(kUuidHidService));
    if (!hid) {
        logf("no HID service 0x1812");
        g_client->disconnect();
        return false;
    }

    g_isKeyboard = false;
    g_isGamepad = false;
    auto *reportMap = hid->getCharacteristic(NimBLEUUID(kUuidReportMap));
    if (reportMap) {
        std::string v = reportMap->readValue();
        ScanDeviceKind kind =
            refineFromReportMap(reinterpret_cast<const uint8_t *>(v.data()),
                                static_cast<uint16_t>(v.size()));
        g_isKeyboard = (kind == ScanDeviceKind::Keyboard);
        g_isGamepad = (kind == ScanDeviceKind::Gamepad);
        logf("reportMap len=%u kind=%s", static_cast<unsigned>(v.size()), kindName(kind));
    }
    if (!g_isKeyboard && !g_isGamepad) {
        // Fall back to name heuristics
        ScanDeviceKind hint = ScanDeviceKind::Unknown;
        if (nameHint && nameHint[0]) {
            std::string fakeName = nameHint;
            // minimal: keyboard/gamepad substrings
            std::string lower = fakeName;
            for (char &c : lower) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            if (lower.find("keyboard") != std::string::npos) {
                hint = ScanDeviceKind::Keyboard;
            } else if (lower.find("game") != std::string::npos ||
                       lower.find("controller") != std::string::npos) {
                hint = ScanDeviceKind::Gamepad;
            }
        }
        g_isKeyboard = (hint == ScanDeviceKind::Keyboard);
        g_isGamepad = (hint == ScanDeviceKind::Gamepad) || (hint == ScanDeviceKind::Unknown);
    }

    auto *charsPtr = hid->getCharacteristics(true);
    if (!charsPtr) {
        logf("no HID characteristics");
        g_client->disconnect();
        return false;
    }
    bool subscribed = false;
    for (auto *ch : *charsPtr) {
        if (!ch) {
            continue;
        }
        if (ch->getUUID().equals(NimBLEUUID(kUuidReport)) || ch->canNotify()) {
            if (ch->canNotify()) {
                if (ch->subscribe(true, onHidNotify)) {
                    subscribed = true;
                }
            }
        }
    }
    if (!subscribed) {
        logf("no HID input notify subscribed");
        // Keep link for diagnostics; still mark connected.
    }

    std::strncpy(g_connAddr, address, sizeof(g_connAddr) - 1);
    if (nameHint) {
        std::strncpy(g_connName, nameHint, sizeof(g_connName) - 1);
    }
    g_connType = g_isKeyboard ? esp2_ble_config::DeviceType::Keyboard
                              : (g_isGamepad ? esp2_ble_config::DeviceType::Gamepad
                                             : esp2_ble_config::DeviceType::Unknown);
    g_connected = true;
    std::memset(g_prevKeys, 0, sizeof(g_prevKeys));
    g_prevMod = 0;
    g_pad = {};
    g_pad.connected = true;
    logf("HID ready keyboard=%d gamepad=%d", g_isKeyboard ? 1 : 0, g_isGamepad ? 1 : 0);
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
    if (len < 3) {
        return;
    }
    const uint8_t mod = data[0];
    const bool ctrl = (mod & 0x11) != 0;
    const bool shift = (mod & 0x22) != 0;
    const bool alt = (mod & 0x44) != 0;
    const bool meta = (mod & 0x88) != 0;

    // Detect newly pressed keys
    for (int i = 2; i < len && i < 8; ++i) {
        const uint8_t usage = data[i];
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
        for (int i = 2; i < len && i < 8; ++i) {
            if (data[i] == prev) {
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
    for (int i = 2, k = 0; i < len && i < 8 && k < 6; ++i) {
        if (data[i]) {
            g_prevKeys[k++] = data[i];
        }
    }
    g_prevMod = mod;
}

void handleGamepadReport(const uint8_t *data, uint8_t len, uint32_t nowMs) {
    // Generic-ish decode: many BLE pads use X,Y at [0],[1] and buttons in later bytes.
    // This is intentionally coarse — refined after physical Report Map capture.
    if (len < 3) {
        return;
    }
    auto axis = [](uint8_t v) -> int16_t {
        // 0..255 → -32768..32767 centered at 128
        const int c = static_cast<int>(v) - 128;
        return static_cast<int16_t>(c * 256);
    };
    g_pad.connected = true;
    g_pad.leftX = axis(data[0]);
    g_pad.leftY = axis(data[1]);
    uint8_t buttons = 0;
    if (len >= 4) {
        buttons = data[len >= 6 ? 5 : 3];
    } else if (len >= 3) {
        buttons = data[2];
    }
    g_pad.btnA = (buttons & 0x01) != 0;
    g_pad.btnB = (buttons & 0x02) != 0;
    g_pad.btnX = (buttons & 0x04) != 0;
    g_pad.btnY = (buttons & 0x08) != 0;
    g_pad.btnL1 = (buttons & 0x10) != 0;
    g_pad.btnR1 = (buttons & 0x20) != 0;
    g_pad.btnSelect = (buttons & 0x40) != 0;
    g_pad.btnStart = (buttons & 0x80) != 0;

    const esp_bracket::LocalAction local = g_engine.evaluateGamepad(g_pad, nowMs);
    if (local != esp_bracket::LocalAction::None) {
        fireAction(local, "ble_gamepad");
    }
    if (g_engine.chordConsumingGamepad(g_pad)) {
        return; // do not forward constituent buttons to Apple II
    }

    const auto pads = g_mapper.toPaddles(g_pad);
    const auto joy = g_mapper.toJoystick(g_pad);
    if (g_padInject) {
        g_padInject(static_cast<uint8_t>(pads.p0 > 255 ? 255 : pads.p0),
                    static_cast<uint8_t>(pads.p1 > 255 ? 255 : pads.p1), joy.button0, joy.button1,
                    false);
    }
}

void processLatestHid(uint32_t nowMs) {
    RawHid r{};
    portENTER_CRITICAL(&g_hidMux);
    r = g_latest;
    g_latest.valid = false;
    portEXIT_CRITICAL(&g_hidMux);
    if (!r.valid) {
        return;
    }
    if (g_isKeyboard) {
        handleKeyboardReport(r.data, r.len, nowMs);
    } else {
        handleGamepadReport(r.data, r.len, nowMs);
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

} // namespace

bool init() {
    if (g_ready) {
        return true;
    }
    NimBLEDevice::init("ESP][");
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
    NimBLEDevice::setSecurityAuth(true /*bonding*/, false /*mitm*/, true /*sc*/);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
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
    scan->setInterval(45);
    scan->setWindow(15);
    scan->setMaxResults(kScanMax);
    logf("scan start sec=%u", static_cast<unsigned>(durationSec));
    NimBLEScanResults results = scan->start(durationSec, false);
    const int n = results.getCount();
    for (int i = 0; i < n && g_discoveredCount < kScanMax; ++i) {
        NimBLEAdvertisedDevice d = results.getDevice(i);
        DiscoveredDevice hit{};
        std::string name = d.getName();
        if (name.empty()) {
            // Skip unnamed unless it advertises HID — still useful for pair by addr.
            if (!d.isAdvertisingService(NimBLEUUID((uint16_t)kUuidHidService))) {
                continue;
            }
            std::strncpy(hit.name, "(unnamed)", sizeof(hit.name) - 1);
        } else {
            std::strncpy(hit.name, name.c_str(), sizeof(hit.name) - 1);
        }
        std::strncpy(hit.address, d.getAddress().toString().c_str(), sizeof(hit.address) - 1);
        hit.addrType = d.getAddressType();
        hit.rssi = d.getRSSI();
        hit.kind = classifyAdvertisement(&d);
        hit.known = esp2_ble_config::findKnownDevice(g_cfg, hit.address) >= 0;
        hit.connected = g_connected && addrEq(g_connAddr, hit.address);
        g_discovered[g_discoveredCount++] = hit;
    }
    scan->clearResults();
    g_scanning = false;
    logf("scan done count=%u", static_cast<unsigned>(g_discoveredCount));
    return true;
}

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
    if (!SD.exists("/esp2")) {
        SD.mkdir("/esp2");
    }
    if (!SD.exists("/esp2/config")) {
        SD.mkdir("/esp2/config");
    }
    File f = SD.open(esp2_ble_config::kBluetoothConfigPath, FILE_WRITE);
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
    if (!SD.exists(esp2_ble_config::kBluetoothConfigPath)) {
        g_cfg = esp2_ble_config::defaultBluetoothConfig();
        g_engine.setConfig(g_cfg.control);
        logf("config missing → defaults");
        return false;
    }
    File f = SD.open(esp2_ble_config::kBluetoothConfigPath, FILE_READ);
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
    std::strncpy(kd.name, name && name[0] ? name : g_connName, sizeof(kd.name) - 1);
    if (g_isKeyboard || kind == ScanDeviceKind::Keyboard) {
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
    return connectAndSubscribe(address, addrType, name);
}

void disconnect() {
    if (g_client && g_client->isConnected()) {
        g_client->disconnect();
    }
    g_connected = false;
    g_connAddr[0] = 0;
}

bool forgetAddress(const char *address) {
    if (!esp2_ble_config::forgetKnownDevice(&g_cfg, address)) {
        return false;
    }
    if (g_connected && addrEq(g_connAddr, address)) {
        disconnect();
    }
    // Best-effort: clear NimBLE bond store entry is stack-version dependent.
    saveConfigToSd();
    logf("forgot addr=%s", address);
    return true;
}

BleHostStatus status() {
    BleHostStatus s{};
    s.ready = g_ready;
    s.scanning = g_scanning;
    s.connected = g_connected && g_client && g_client->isConnected();
    std::strncpy(s.connectedAddr, g_connAddr, sizeof(s.connectedAddr) - 1);
    std::strncpy(s.connectedName, g_connName, sizeof(s.connectedName) - 1);
    s.connectedType = g_connType;
    s.knownCount = g_cfg.knownCount;
    s.discoveredCount = g_discoveredCount;
    // V1 budget: one active HID host connection (keyboard OR gamepad).
    // Multiple devices may be *known*; reconnect prefers one at a time.
    s.maxSimultaneousConnections = 1;
    return s;
}

void service(uint32_t nowMs) {
    if (!g_ready) {
        return;
    }
    if (g_connected) {
        processLatestHid(nowMs);
    }
}

void setLocalActionHandler(LocalActionHandler handler) { g_actionHandler = handler; }

void setInjectors(KeyInjectFn keyFn, PadInjectFn padFn) {
    g_keyInject = keyFn;
    g_padInject = padFn;
}

esp_bracket::GamepadState latestGamepad() { return g_pad; }

} // namespace esp2_ble
