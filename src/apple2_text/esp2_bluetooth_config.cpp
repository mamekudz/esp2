#include "esp2_bluetooth_config.hpp"

#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <cstring>

namespace esp2_ble_config {
namespace {

void setErr(char *err, size_t errLen, const char *msg) {
    if (!err || errLen == 0) {
        return;
    }
    std::strncpy(err, msg ? msg : "error", errLen - 1);
    err[errLen - 1] = 0;
}

const char *skipWs(const char *p, const char *end) {
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) {
        ++p;
    }
    return p;
}

const char *findKey(const char *json, size_t len, const char *key) {
    if (!json || !key) {
        return nullptr;
    }
    char needle[64];
    std::snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char *end = json + len;
    const char *p = json;
    while (p < end) {
        const char *hit = std::strstr(p, needle);
        if (!hit || hit >= end) {
            return nullptr;
        }
        const char *after = hit + std::strlen(needle);
        after = skipWs(after, end);
        if (after < end && *after == ':') {
            return after + 1;
        }
        p = hit + 1;
    }
    return nullptr;
}

bool parseStringAfter(const char *afterColon, const char *end, char *out, size_t outLen) {
    if (!afterColon || !out || outLen == 0) {
        return false;
    }
    const char *p = skipWs(afterColon, end);
    if (p >= end) {
        return false;
    }
    if (std::strncmp(p, "null", 4) == 0) {
        out[0] = 0;
        return true;
    }
    if (*p != '"') {
        return false;
    }
    ++p;
    size_t i = 0;
    while (p < end && *p != '"' && i + 1 < outLen) {
        if (*p == '\\' && p + 1 < end) {
            ++p;
        }
        out[i++] = *p++;
    }
    out[i] = 0;
    return p < end && *p == '"';
}

bool parseBoolAfter(const char *afterColon, const char *end, bool *out) {
    const char *p = skipWs(afterColon, end);
    if (!p || !out) {
        return false;
    }
    if (std::strncmp(p, "true", 4) == 0) {
        *out = true;
        return true;
    }
    if (std::strncmp(p, "false", 5) == 0) {
        *out = false;
        return true;
    }
    return false;
}

bool parseIntAfter(const char *afterColon, const char *end, int *out) {
    const char *p = skipWs(afterColon, end);
    if (!p || !out || p >= end) {
        return false;
    }
    char *ep = nullptr;
    long v = std::strtol(p, &ep, 10);
    if (ep == p) {
        return false;
    }
    *out = static_cast<int>(v);
    return true;
}

bool addrEqual(const char *a, const char *b) {
    if (!a || !b) {
        return false;
    }
    while (*a && *b) {
        const char ca = static_cast<char>(std::tolower(static_cast<unsigned char>(*a)));
        const char cb = static_cast<char>(std::tolower(static_cast<unsigned char>(*b)));
        if (ca != cb) {
            return false;
        }
        ++a;
        ++b;
    }
    return *a == 0 && *b == 0;
}

uint32_t parseButtonToken(const char *tok) {
    if (!tok) {
        return 0;
    }
    if (std::strcmp(tok, "a") == 0) return esp_bracket::GpA;
    if (std::strcmp(tok, "b") == 0) return esp_bracket::GpB;
    if (std::strcmp(tok, "x") == 0) return esp_bracket::GpX;
    if (std::strcmp(tok, "y") == 0) return esp_bracket::GpY;
    if (std::strcmp(tok, "l1") == 0) return esp_bracket::GpL1;
    if (std::strcmp(tok, "r1") == 0) return esp_bracket::GpR1;
    if (std::strcmp(tok, "l2") == 0) return esp_bracket::GpL2;
    if (std::strcmp(tok, "r2") == 0) return esp_bracket::GpR2;
    if (std::strcmp(tok, "start") == 0) return esp_bracket::GpStart;
    if (std::strcmp(tok, "select") == 0) return esp_bracket::GpSelect;
    if (std::strcmp(tok, "home") == 0) return esp_bracket::GpHome;
    if (std::strcmp(tok, "up") == 0) return esp_bracket::GpDpadUp;
    if (std::strcmp(tok, "down") == 0) return esp_bracket::GpDpadDown;
    if (std::strcmp(tok, "left") == 0) return esp_bracket::GpDpadLeft;
    if (std::strcmp(tok, "right") == 0) return esp_bracket::GpDpadRight;
    return 0;
}

/** Parse ["start","select"] array into mask. */
uint32_t parseButtonArray(const char *p, const char *end) {
    p = skipWs(p, end);
    if (p >= end || *p != '[') {
        return 0;
    }
    ++p;
    uint32_t mask = 0;
    while (p < end && *p != ']') {
        p = skipWs(p, end);
        if (*p == '"') {
            ++p;
            char tok[16]{};
            size_t i = 0;
            while (p < end && *p != '"' && i + 1 < sizeof(tok)) {
                tok[i++] = static_cast<char>(std::tolower(static_cast<unsigned char>(*p++)));
            }
            tok[i] = 0;
            if (p < end && *p == '"') {
                ++p;
            }
            mask |= parseButtonToken(tok);
        } else if (*p == ',' || *p == ' ') {
            ++p;
        } else {
            ++p;
        }
    }
    return mask;
}

void parseKeyboardBinding(const char *objStart, const char *end, esp_bracket::KeyboardBinding *out) {
    if (!objStart || !out) {
        return;
    }
    // Bound search to this object roughly (until matching }) — simple scan.
    const char *p = skipWs(objStart, end);
    if (p >= end || *p != '{') {
        return;
    }
    const char *scope = p;
    // Use findKey from scope for nested keys — may false-positive; ok for V1.
    bool ctrl = false, alt = false, shift = false, meta = false;
    const char *k;
    if ((k = findKey(scope, static_cast<size_t>(end - scope), "ctrl"))) {
        parseBoolAfter(k, end, &ctrl);
    }
    if ((k = findKey(scope, static_cast<size_t>(end - scope), "alt"))) {
        parseBoolAfter(k, end, &alt);
    }
    if ((k = findKey(scope, static_cast<size_t>(end - scope), "shift"))) {
        parseBoolAfter(k, end, &shift);
    }
    if ((k = findKey(scope, static_cast<size_t>(end - scope), "meta"))) {
        parseBoolAfter(k, end, &meta);
    }
    char keyStr[8]{};
    uint16_t usage = 0;
    if ((k = findKey(scope, static_cast<size_t>(end - scope), "key"))) {
        if (parseStringAfter(k, end, keyStr, sizeof(keyStr)) && keyStr[0]) {
            usage = asciiToHidUsage(keyStr[0]);
        }
    }
    if ((k = findKey(scope, static_cast<size_t>(end - scope), "hidUsage"))) {
        int v = 0;
        if (parseIntAfter(k, end, &v) && v > 0 && v < 0x100) {
            usage = static_cast<uint16_t>(v);
        }
    }
    if (usage == 0) {
        out->valid = false;
        return;
    }
    out->ctrl = ctrl;
    out->alt = alt;
    out->shift = shift;
    out->meta = meta;
    out->hidUsage = usage;
    out->ascii = keyStr[0] ? keyStr[0] : 0;
    out->valid = true;
}

void parseGamepadBinding(const char *objStart, const char *end, esp_bracket::GamepadChordBinding *out) {
    if (!objStart || !out) {
        return;
    }
    const char *p = skipWs(objStart, end);
    if (p >= end || *p != '{') {
        return;
    }
    const char *scope = p;
    const char *k;
    uint32_t buttons = 0;
    if ((k = findKey(scope, static_cast<size_t>(end - scope), "buttons"))) {
        buttons = parseButtonArray(skipWs(k, end), end);
    }
    int hold = 1500;
    if ((k = findKey(scope, static_cast<size_t>(end - scope), "holdMs"))) {
        parseIntAfter(k, end, &hold);
    }
    if (buttons == 0) {
        out->valid = false;
        return;
    }
    out->buttons = buttons;
    out->holdMs = hold > 0 ? static_cast<uint32_t>(hold) : 1500;
    out->valid = true;
}

void parseActionBindings(const char *objStart, const char *end, esp_bracket::LocalBindingSet *out) {
    if (!objStart || !out) {
        return;
    }
    const char *p = skipWs(objStart, end);
    if (p >= end || *p != '{') {
        return;
    }
    const char *scope = p;
    const char *k;
    if ((k = findKey(scope, static_cast<size_t>(end - scope), "keyboard"))) {
        parseKeyboardBinding(skipWs(k, end), end, &out->keyboard);
    }
    if ((k = findKey(scope, static_cast<size_t>(end - scope), "gamepad"))) {
        parseGamepadBinding(skipWs(k, end), end, &out->gamepad);
    }
}

} // namespace

int findKnownDevice(const BluetoothConfig &cfg, const char *address) {
    if (!address || !address[0]) {
        return -1;
    }
    for (size_t i = 0; i < cfg.knownCount; ++i) {
        if (addrEqual(cfg.known[i].address, address)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool upsertKnownDevice(BluetoothConfig *cfg, const KnownDevice &dev) {
    if (!cfg || !dev.address[0]) {
        return false;
    }
    const int idx = findKnownDevice(*cfg, dev.address);
    if (idx >= 0) {
        cfg->known[static_cast<size_t>(idx)] = dev;
        return true;
    }
    if (cfg->knownCount >= kMaxKnownDevices) {
        return false;
    }
    cfg->known[cfg->knownCount++] = dev;
    return true;
}

bool forgetKnownDevice(BluetoothConfig *cfg, const char *address) {
    const int idx = findKnownDevice(*cfg, address);
    if (idx < 0 || !cfg) {
        return false;
    }
    for (size_t i = static_cast<size_t>(idx) + 1; i < cfg->knownCount; ++i) {
        cfg->known[i - 1] = cfg->known[i];
    }
    --cfg->knownCount;
    cfg->known[cfg->knownCount] = KnownDevice{};
    return true;
}

BluetoothConfig parseBluetoothConfigJson(const char *json, size_t len, char *err, size_t errLen) {
    BluetoothConfig cfg = defaultBluetoothConfig();
    if (!json || len == 0) {
        setErr(err, errLen, "empty");
        cfg.valid = false;
        return cfg;
    }
    const char *end = json + len;
    const char *k = findKey(json, len, "schemaVersion");
    int ver = 0;
    if (!k || !parseIntAfter(k, end, &ver) || ver != kSchemaVersion) {
        setErr(err, errLen, "schema");
        cfg.valid = false;
        return cfg;
    }

    // knownDevices array — walk objects loosely
    k = findKey(json, len, "knownDevices");
    if (k) {
        const char *p = skipWs(k, end);
        if (p < end && *p == '[') {
            ++p;
            while (p < end && *p != ']' && cfg.knownCount < kMaxKnownDevices) {
                p = skipWs(p, end);
                if (*p == '{') {
                    KnownDevice d{};
                    const char *obj = p;
                    // advance to matching } roughly
                    int depth = 0;
                    const char *q = p;
                    while (q < end) {
                        if (*q == '{') {
                            ++depth;
                        } else if (*q == '}') {
                            --depth;
                            if (depth == 0) {
                                ++q;
                                break;
                            }
                        }
                        ++q;
                    }
                    const size_t objLen = static_cast<size_t>(q - obj);
                    const char *kk;
                    if ((kk = findKey(obj, objLen, "address"))) {
                        parseStringAfter(kk, q, d.address, sizeof(d.address));
                    }
                    if ((kk = findKey(obj, objLen, "name"))) {
                        parseStringAfter(kk, q, d.name, sizeof(d.name));
                    }
                    char typeStr[16]{};
                    if ((kk = findKey(obj, objLen, "deviceType"))) {
                        parseStringAfter(kk, q, typeStr, sizeof(typeStr));
                        d.type = parseDeviceType(typeStr);
                    }
                    int at = 0;
                    if ((kk = findKey(obj, objLen, "addrType"))) {
                        parseIntAfter(kk, q, &at);
                        d.addrType = static_cast<uint8_t>(at & 0xFF);
                    }
                    bool ar = true;
                    if ((kk = findKey(obj, objLen, "autoReconnect"))) {
                        parseBoolAfter(kk, q, &ar);
                    }
                    d.autoReconnect = ar;
                    bool bonded = false;
                    if ((kk = findKey(obj, objLen, "bonded"))) {
                        parseBoolAfter(kk, q, &bonded);
                    }
                    d.bonded = bonded;
                    if (d.address[0]) {
                        cfg.known[cfg.knownCount++] = d;
                    }
                    p = q;
                } else if (*p == ',') {
                    ++p;
                } else {
                    ++p;
                }
            }
        }
    }

    k = findKey(json, len, "control");
    if (k) {
        const char *ctrl = skipWs(k, end);
        if (ctrl < end && *ctrl == '{') {
            const char *kk;
            if ((kk = findKey(ctrl, static_cast<size_t>(end - ctrl), "resetAppleIi"))) {
                parseActionBindings(skipWs(kk, end), end, &cfg.control.resetAppleIi);
            }
            if ((kk = findKey(ctrl, static_cast<size_t>(end - ctrl), "openSettings"))) {
                parseActionBindings(skipWs(kk, end), end, &cfg.control.openSettings);
            }
        }
    }

    // Sanitize via engine defaults for any invalid slot.
    esp_bracket::LocalControlEngine eng;
    eng.setConfig(cfg.control);
    cfg.control = eng.config();

    cfg.loaded = true;
    cfg.valid = true;
    setErr(err, errLen, "");
    return cfg;
}

size_t serializeBluetoothConfigJson(const BluetoothConfig &cfg, char *out, size_t outLen) {
    if (!out || outLen < 64) {
        return 0;
    }
    size_t n = 0;
    auto append = [&](const char *s) -> bool {
        const size_t L = std::strlen(s);
        if (n + L + 1 >= outLen) {
            return false;
        }
        std::memcpy(out + n, s, L);
        n += L;
        out[n] = 0;
        return true;
    };

    if (!append("{\n  \"schemaVersion\": 1,\n  \"knownDevices\": [\n")) {
        return 0;
    }
    for (size_t i = 0; i < cfg.knownCount; ++i) {
        const KnownDevice &d = cfg.known[i];
        char line[240];
        std::snprintf(
            line, sizeof(line),
            "    {\"address\":\"%s\",\"addrType\":%u,\"name\":\"%s\",\"deviceType\":\"%s\","
            "\"autoReconnect\":%s,\"bonded\":%s}%s\n",
            d.address, static_cast<unsigned>(d.addrType), d.name[0] ? d.name : "",
            deviceTypeName(d.type), d.autoReconnect ? "true" : "false",
            d.bonded ? "true" : "false", (i + 1 < cfg.knownCount) ? "," : "");
        if (!append(line)) {
            return 0;
        }
    }

    const auto &c = cfg.control;
    char rk[8]{};
    char sk[8]{};
    if (c.resetAppleIi.keyboard.ascii) {
        rk[0] = static_cast<char>(
            std::toupper(static_cast<unsigned char>(c.resetAppleIi.keyboard.ascii)));
    } else {
        rk[0] = 'R';
    }
    if (c.openSettings.keyboard.ascii) {
        sk[0] = static_cast<char>(
            std::toupper(static_cast<unsigned char>(c.openSettings.keyboard.ascii)));
    } else {
        sk[0] = 'S';
    }

    auto buttonList = [](uint32_t m, char *buf, size_t sz) {
        buf[0] = 0;
        size_t pos = 0;
        auto add = [&](const char *name, uint32_t bit) {
            if (!(m & bit)) {
                return;
            }
            if (pos > 0 && pos + 1 < sz) {
                buf[pos++] = ',';
            }
            const int w =
                std::snprintf(buf + pos, sz > pos ? sz - pos : 0, "\"%s\"", name);
            if (w > 0) {
                pos += static_cast<size_t>(w);
            }
        };
        add("a", esp_bracket::GpA);
        add("b", esp_bracket::GpB);
        add("x", esp_bracket::GpX);
        add("y", esp_bracket::GpY);
        add("l1", esp_bracket::GpL1);
        add("r1", esp_bracket::GpR1);
        add("start", esp_bracket::GpStart);
        add("select", esp_bracket::GpSelect);
        add("home", esp_bracket::GpHome);
        if (pos < sz) {
            buf[pos] = 0;
        }
    };
    char rb[64]{};
    char sb[64]{};
    buttonList(c.resetAppleIi.gamepad.buttons, rb, sizeof(rb));
    buttonList(c.openSettings.gamepad.buttons, sb, sizeof(sb));

    if (!append("  ],\n  \"control\": {\n")) {
        return 0;
    }
    char block[512];
    std::snprintf(
        block, sizeof(block),
        "    \"resetAppleIi\": {\n"
        "      \"keyboard\": {\"ctrl\":%s,\"alt\":%s,\"shift\":%s,\"meta\":%s,\"key\":\"%s\"},\n"
        "      \"gamepad\": {\"buttons\":[%s],\"holdMs\":%u}\n"
        "    },\n"
        "    \"openSettings\": {\n"
        "      \"keyboard\": {\"ctrl\":%s,\"alt\":%s,\"shift\":%s,\"meta\":%s,\"key\":\"%s\"},\n"
        "      \"gamepad\": {\"buttons\":[%s],\"holdMs\":%u}\n"
        "    }\n"
        "  }\n}\n",
        c.resetAppleIi.keyboard.ctrl ? "true" : "false",
        c.resetAppleIi.keyboard.alt ? "true" : "false",
        c.resetAppleIi.keyboard.shift ? "true" : "false",
        c.resetAppleIi.keyboard.meta ? "true" : "false", rk, rb[0] ? rb : "\"start\",\"b\"",
        static_cast<unsigned>(c.resetAppleIi.gamepad.holdMs),
        c.openSettings.keyboard.ctrl ? "true" : "false",
        c.openSettings.keyboard.alt ? "true" : "false",
        c.openSettings.keyboard.shift ? "true" : "false",
        c.openSettings.keyboard.meta ? "true" : "false", sk, sb[0] ? sb : "\"start\",\"select\"",
        static_cast<unsigned>(c.openSettings.gamepad.holdMs));
    if (!append(block)) {
        return 0;
    }
    return n;
}

} // namespace esp2_ble_config
