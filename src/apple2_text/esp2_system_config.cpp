#include "esp2_system_config.hpp"

#include <cctype>
#include <cstdio>
#include <cstring>

namespace esp2_config {
namespace {

void setErr(char *err, size_t errLen, const char *msg) {
    if (!err || errLen == 0) {
        return;
    }
    strncpy(err, msg ? msg : "error", errLen - 1);
    err[errLen - 1] = 0;
}

bool isEsp2Path(const char *p) {
    if (!p || p[0] != '/' || strncmp(p, "/esp2/", 6) != 0) {
        return false;
    }
    if (strstr(p, "..") != nullptr) {
        return false;
    }
    const size_t n = strlen(p);
    return n > 6 && n < kPathMax && p[n - 1] != '/';
}

const char *skipWs(const char *p, const char *end) {
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) {
        ++p;
    }
    return p;
}

/** Find `"key"` then optional whitespace and `:`. Returns pointer after `:`. */
const char *findKey(const char *json, size_t len, const char *key) {
    if (!json || !key) {
        return nullptr;
    }
    char needle[64];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char *end = json + len;
    const char *p = json;
    while (p < end) {
        const char *hit = strstr(p, needle);
        if (!hit || hit >= end) {
            return nullptr;
        }
        const char *after = hit + strlen(needle);
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
    if (strncmp(p, "null", 4) == 0) {
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
    if (strncmp(p, "true", 4) == 0) {
        *out = true;
        return true;
    }
    if (strncmp(p, "false", 5) == 0) {
        *out = false;
        return true;
    }
    return false;
}

bool parseUIntAfter(const char *afterColon, const char *end, uint32_t *out) {
    const char *p = skipWs(afterColon, end);
    if (!p || !out || p >= end || !isdigit(static_cast<unsigned char>(*p))) {
        return false;
    }
    uint32_t v = 0;
    while (p < end && isdigit(static_cast<unsigned char>(*p))) {
        v = v * 10u + static_cast<uint32_t>(*p - '0');
        ++p;
    }
    *out = v;
    return true;
}

bool parseIntAfter(const char *afterColon, const char *end, int *out) {
    uint32_t u = 0;
    if (!parseUIntAfter(afterColon, end, &u)) {
        return false;
    }
    *out = static_cast<int>(u);
    return true;
}

} // namespace

SystemConfig parseSystemConfigJson(const char *json, size_t len, char *err, size_t errLen) {
    SystemConfig cfg = defaultSystemConfig();
    cfg.loaded = true;
    if (!json || len == 0) {
        setErr(err, errLen, "empty");
        cfg.valid = false;
        return cfg;
    }
    const char *end = json + len;

    const char *sv = findKey(json, len, "schemaVersion");
    if (sv) {
        int ver = 0;
        if (parseIntAfter(sv, end, &ver)) {
            cfg.schemaVersion = ver;
        }
    }
    if (cfg.schemaVersion != kSchemaVersion) {
        setErr(err, errLen, "schemaVersion");
        cfg.valid = false;
        return cfg;
    }

    if (const char *p = findKey(json, len, "rom")) {
        parseStringAfter(p, end, cfg.romPath, sizeof(cfg.romPath));
    }
    if (const char *p = findKey(json, len, "drive1")) {
        parseStringAfter(p, end, cfg.drive1, sizeof(cfg.drive1));
    }
    if (const char *p = findKey(json, len, "drive2")) {
        char tmp[kPathMax]{};
        if (parseStringAfter(p, end, tmp, sizeof(tmp))) {
            if (tmp[0]) {
                strncpy(cfg.drive2, tmp, sizeof(cfg.drive2) - 1);
                cfg.hasDrive2 = true;
            }
        }
    }
    if (const char *p = findKey(json, len, "bootFromDisk")) {
        parseBoolAfter(p, end, &cfg.bootFromDisk);
    }
    if (const char *p = findKey(json, len, "macro")) {
        parseStringAfter(p, end, cfg.startupMacro, sizeof(cfg.startupMacro));
    } else if (const char *p = findKey(json, len, "startupMacro")) {
        parseStringAfter(p, end, cfg.startupMacro, sizeof(cfg.startupMacro));
    }
    if (const char *p = findKey(json, len, "orientation")) {
        char o[32]{};
        if (parseStringAfter(p, end, o, sizeof(o))) {
            if (strcmp(o, "landscape") == 0) {
                cfg.orientation = Orient::Landscape;
            } else if (strcmp(o, "classic") == 0) {
                cfg.orientation = Orient::Classic;
            } else {
                setErr(err, errLen, "orientation");
                cfg.valid = false;
                return cfg;
            }
        }
    }
    // Prefer presentation.monitor; fall back to legacy presentation.color.
    bool monitorSet = false;
    if (const char *p = findKey(json, len, "monitor")) {
        char m[32]{};
        if (parseStringAfter(p, end, m, sizeof(m))) {
            if (strcmp(m, "white") == 0 || strcmp(m, "sharp") == 0 || strcmp(m, "mono") == 0 ||
                strcmp(m, "monochrome") == 0) {
                cfg.monitor = Monitor::White;
                monitorSet = true;
            } else if (strcmp(m, "green") == 0) {
                cfg.monitor = Monitor::Green;
                monitorSet = true;
            } else if (strcmp(m, "amber") == 0) {
                cfg.monitor = Monitor::Amber;
                monitorSet = true;
            } else if (strcmp(m, "artifact") == 0 || strcmp(m, "artifactColor") == 0) {
                cfg.monitor = Monitor::Artifact;
                monitorSet = true;
            } else {
                setErr(err, errLen, "monitor");
                cfg.valid = false;
                return cfg;
            }
        }
    }
    if (!monitorSet) {
        if (const char *p = findKey(json, len, "color")) {
            char c[32]{};
            if (parseStringAfter(p, end, c, sizeof(c))) {
                if (strcmp(c, "artifact") == 0 || strcmp(c, "artifactColor") == 0) {
                    cfg.monitor = Monitor::Artifact;
                } else if (strcmp(c, "sharp") == 0 || strcmp(c, "white") == 0) {
                    cfg.monitor = Monitor::White;
                } else if (strcmp(c, "green") == 0) {
                    cfg.monitor = Monitor::Green;
                } else if (strcmp(c, "amber") == 0) {
                    cfg.monitor = Monitor::Amber;
                } else {
                    setErr(err, errLen, "color");
                    cfg.valid = false;
                    return cfg;
                }
            }
        }
    }
    if (const char *p = findKey(json, len, "effect")) {
        char e[32]{};
        if (parseStringAfter(p, end, e, sizeof(e))) {
            if (strcmp(e, "crt") == 0 || strcmp(e, "crt_tv") == 0 || strcmp(e, "tv") == 0) {
                cfg.effect = Effect::Crt;
            } else if (strcmp(e, "clean") == 0 || strcmp(e, "sharp") == 0 ||
                       strcmp(e, "off") == 0 || strcmp(e, "none") == 0) {
                cfg.effect = Effect::Clean;
            } else {
                setErr(err, errLen, "effect");
                cfg.valid = false;
                return cfg;
            }
        }
    }
    if (const char *p = findKey(json, len, "screensaverSeconds")) {
        parseUIntAfter(p, end, &cfg.screensaverSeconds);
    }
    // usb.storageMode: "normal" | "auto" (also accept top-level usbStorageMode)
    if (const char *p = findKey(json, len, "storageMode")) {
        char m[32]{};
        if (parseStringAfter(p, end, m, sizeof(m))) {
            if (strcmp(m, "auto") == 0 || strcmp(m, "automount") == 0) {
                cfg.usbStorageAuto = true;
            } else {
                cfg.usbStorageAuto = false;
            }
        }
    }

    char verr[64]{};
    if (!validateSystemConfig(&cfg, verr, sizeof(verr))) {
        setErr(err, errLen, verr[0] ? verr : "validate");
        cfg.valid = false;
        return cfg;
    }
    cfg.valid = true;
    if (err && errLen) {
        err[0] = 0;
    }
    return cfg;
}

bool validateSystemConfig(SystemConfig *cfg, char *err, size_t errLen) {
    if (!cfg) {
        setErr(err, errLen, "null");
        return false;
    }
    if (cfg->romPath[0] && !isEsp2Path(cfg->romPath)) {
        setErr(err, errLen, "rom_path");
        cfg->romPath[0] = 0;
        return false;
    }
    if (cfg->drive1[0] && !isEsp2Path(cfg->drive1)) {
        setErr(err, errLen, "drive1_path");
        cfg->drive1[0] = 0;
        return false;
    }
    if (cfg->hasDrive2) {
        if (!cfg->drive2[0] || !isEsp2Path(cfg->drive2)) {
            setErr(err, errLen, "drive2_path");
            cfg->hasDrive2 = false;
            cfg->drive2[0] = 0;
            return false;
        }
    }
    if (cfg->screensaverSeconds > 86400u) {
        setErr(err, errLen, "screensaverSeconds");
        return false;
    }
    for (const char *p = cfg->startupMacro; *p; ++p) {
        if (!(isalnum(static_cast<unsigned char>(*p)) || *p == '-' || *p == '_')) {
            setErr(err, errLen, "macro_id");
            cfg->startupMacro[0] = 0;
            return false;
        }
    }
    return true;
}

} // namespace esp2_config
