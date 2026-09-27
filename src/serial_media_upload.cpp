#include "serial_media_upload.hpp"

#include "board_pins.h"
#include "esp2_sd_bus.hpp"
#include "esp32_sd_storage.hpp"
#include "esp_bracket/sha256.hpp"

#include <Arduino.h>
#include <SD.h>
#include <cstdio>
#include <cstring>

namespace esp2_upload {
namespace {

using esp_bracket::Sha256;

volatile bool g_sessionActive = false;

bool startsWith(const char *s, const char *pfx) {
    if (!s || !pfx) {
        return false;
    }
    while (*pfx) {
        if (*s++ != *pfx++) {
            return false;
        }
    }
    return true;
}

bool ensureParentDirs(const char *absPath) {
    if (!absPath || absPath[0] != '/') {
        return false;
    }
    char tmp[kMaxPathLen + 1];
    std::snprintf(tmp, sizeof(tmp), "%s", absPath);
    // Walk components: /esp2/roms/file.rom -> mkdir /esp2, /esp2/roms
    char *slash = tmp + 1;
    while (*slash) {
        if (*slash == '/') {
            *slash = 0;
            if (!SD.exists(tmp)) {
                (void)SD.mkdir(tmp);
                delay(5);
                if (!SD.exists(tmp)) {
                    Serial.print("#NAK mkdir_path=");
                    Serial.println(tmp);
                    Serial.flush();
                    return false;
                }
            }
            *slash = '/';
        }
        ++slash;
    }
    return true;
}

bool readExact(uint8_t *dst, size_t n, uint32_t timeoutMs) {
    const uint32_t t0 = millis();
    size_t got = 0;
    while (got < n) {
        if (Serial.available()) {
            const int b = Serial.read();
            if (b < 0) {
                continue;
            }
            dst[got++] = static_cast<uint8_t>(b);
            continue;
        }
        if ((millis() - t0) > timeoutMs) {
            return false;
        }
        delay(1);
    }
    return true;
}

bool readU32(uint32_t *out, uint32_t timeoutMs) {
    uint8_t b[4];
    if (!readExact(b, 4, timeoutMs)) {
        return false;
    }
    *out = static_cast<uint32_t>(b[0]) | (static_cast<uint32_t>(b[1]) << 8) |
           (static_cast<uint32_t>(b[2]) << 16) | (static_cast<uint32_t>(b[3]) << 24);
    return true;
}

bool readU64(uint64_t *out, uint32_t timeoutMs) {
    uint32_t lo = 0, hi = 0;
    if (!readU32(&lo, timeoutMs) || !readU32(&hi, timeoutMs)) {
        return false;
    }
    *out = static_cast<uint64_t>(lo) | (static_cast<uint64_t>(hi) << 32);
    return true;
}

void ack(const char *msg) {
    Serial.print("#ACK ");
    Serial.println(msg);
    Serial.flush();
}

void nak(const char *msg) {
    Serial.print("#NAK ");
    Serial.println(msg);
    Serial.flush();
}

bool hashFile(const char *path, uint8_t digest[32], uint64_t *outSize) {
    File f = SD.open(path, FILE_READ);
    if (!f) {
        return false;
    }
    Sha256 h;
    uint8_t buf[512];
    uint64_t total = 0;
    for (;;) {
        const int n = f.read(buf, sizeof(buf));
        if (n <= 0) {
            break;
        }
        h.update(buf, static_cast<size_t>(n));
        total += static_cast<uint64_t>(n);
    }
    f.close();
    h.final(digest);
    if (outSize) {
        *outSize = total;
    }
    return true;
}

bool handleVerify() {
    uint32_t pathLen = 0;
    if (!readU32(&pathLen, 5000) || pathLen == 0 || pathLen > kMaxPathLen) {
        nak("verify_path");
        return false;
    }
    char raw[kMaxPathLen + 1];
    if (!readExact(reinterpret_cast<uint8_t *>(raw), pathLen, 5000)) {
        nak("verify_read");
        return false;
    }
    raw[pathLen] = 0;
    char path[kMaxPathLen + 1];
    if (!sanitizeEsp2Path(raw, path, sizeof(path))) {
        nak("verify_path_unsafe");
        return false;
    }
    if (!SD.exists(path)) {
        nak("verify_missing");
        return false;
    }
    uint8_t dig[32];
    uint64_t sz = 0;
    if (!hashFile(path, dig, &sz)) {
        nak("verify_hash");
        return false;
    }
    char hex[65];
    Sha256::toHex(dig, hex);
    Serial.print("#ACK VERIFY path=");
    Serial.print(path);
    Serial.print(" size=");
    Serial.print(static_cast<unsigned long>(sz));
    Serial.print(" sha256=");
    Serial.println(hex);
    Serial.flush();
    return true;
}

bool handleBeginSession() {
    uint32_t pathLen = 0;
    if (!readU32(&pathLen, 5000) || pathLen == 0 || pathLen > kMaxPathLen) {
        nak("begin_path");
        return false;
    }
    char raw[kMaxPathLen + 1];
    if (!readExact(reinterpret_cast<uint8_t *>(raw), pathLen, 5000)) {
        nak("begin_read");
        return false;
    }
    raw[pathLen] = 0;

    char finalPath[kMaxPathLen + 1];
    if (!sanitizeEsp2Path(raw, finalPath, sizeof(finalPath))) {
        nak("path_unsafe");
        return false;
    }

    uint64_t fileSize = 0;
    if (!readU64(&fileSize, 5000)) {
        nak("begin_size");
        return false;
    }
    if (fileSize == 0 || fileSize > kMaxFileBytes) {
        nak("size_limit");
        return false;
    }

    uint8_t expectSha[32];
    if (!readExact(expectSha, 32, 5000)) {
        nak("begin_sha");
        return false;
    }

    uint32_t chunkPref = 0;
    if (!readU32(&chunkPref, 5000)) {
        nak("begin_chunk");
        return false;
    }
    uint32_t chunk = chunkPref ? chunkPref : kDefaultChunk;
    if (chunk > kMaxChunk) {
        chunk = kMaxChunk;
    }
    if (chunk < 256) {
        chunk = 256;
    }

    char staging[kMaxPathLen + 16];
    if (!makeStagingPath(finalPath, staging, sizeof(staging))) {
        nak("staging_path");
        return false;
    }
    if (!ensureParentDirs(finalPath)) {
        nak("mkdir");
        return false;
    }
    if (SD.exists(staging)) {
        SD.remove(staging);
    }

    File out = SD.open(staging, FILE_WRITE);
    if (!out) {
        nak("open_staging");
        return false;
    }

    char hexExpect[65];
    Sha256::toHex(expectSha, hexExpect);
    Serial.print("#ACK BEGIN path=");
    Serial.print(finalPath);
    Serial.print(" size=");
    Serial.print(static_cast<unsigned long>(fileSize));
    Serial.print(" chunk=");
    Serial.print(chunk);
    Serial.print(" sha256=");
    Serial.println(hexExpect);
    Serial.flush();
    delay(30);

    Sha256 hasher;
    uint64_t received = 0;
    uint32_t expectSeq = 0;
    uint8_t *buf = static_cast<uint8_t *>(malloc(chunk));
    if (!buf) {
        out.close();
        SD.remove(staging);
        nak("oom");
        return false;
    }

    bool ok = false;
    for (;;) {
        uint32_t magic = 0;
        if (!readU32(&magic, 30000) || magic != kMagic) {
            nak("frame_magic");
            break;
        }
        uint8_t ver = 0, type = 0;
        uint8_t pad[2];
        if (!readExact(&ver, 1, 5000) || !readExact(&type, 1, 5000) || !readExact(pad, 2, 5000)) {
            nak("frame_hdr");
            break;
        }
        if (ver != kVersion) {
            nak("version");
            break;
        }
        if (type == static_cast<uint8_t>(FrameType::Abort)) {
            nak("aborted");
            break;
        }
        if (type == static_cast<uint8_t>(FrameType::End)) {
            if (received != fileSize) {
                nak("size_mismatch");
                break;
            }
            uint8_t got[32];
            hasher.final(got);
            if (std::memcmp(got, expectSha, 32) != 0) {
                nak("sha_mismatch");
                break;
            }
            out.close();
            free(buf);
            buf = nullptr;
            if (SD.exists(finalPath)) {
                SD.remove(finalPath);
            }
            if (!SD.rename(staging, finalPath)) {
                // Fallback: copy then remove staging
                File src = SD.open(staging, FILE_READ);
                File dst = SD.open(finalPath, FILE_WRITE);
                if (!src || !dst) {
                    if (src) {
                        src.close();
                    }
                    if (dst) {
                        dst.close();
                    }
                    nak("rename");
                    return false;
                }
                uint8_t cpy[512];
                while (src.available()) {
                    const int n = src.read(cpy, sizeof(cpy));
                    if (n <= 0) {
                        break;
                    }
                    dst.write(cpy, n);
                }
                src.close();
                dst.close();
                SD.remove(staging);
            }
            char hexGot[65];
            Sha256::toHex(got, hexGot);
            Serial.print("#ACK END path=");
            Serial.print(finalPath);
            Serial.print(" size=");
            Serial.print(static_cast<unsigned long>(received));
            Serial.print(" sha256=");
            Serial.println(hexGot);
            Serial.flush();
            ok = true;
            return true;
        }
        if (type != static_cast<uint8_t>(FrameType::Data)) {
            nak("frame_type");
            break;
        }
        uint32_t seq = 0, len = 0;
        if (!readU32(&seq, 5000) || !readU32(&len, 5000)) {
            nak("data_hdr");
            break;
        }
        if (seq != expectSeq || len == 0 || len > chunk || received + len > fileSize) {
            nak("data_seq");
            break;
        }
        // Prefer block read — more reliable on HW CDC than byte-at-a-time.
        {
            size_t bodyGot = 0;
            const uint32_t bodyT0 = millis();
            while (bodyGot < len) {
                const int n =
                    Serial.readBytes(reinterpret_cast<char *>(buf + bodyGot), len - bodyGot);
                if (n > 0) {
                    bodyGot += static_cast<size_t>(n);
                    continue;
                }
                if ((millis() - bodyT0) > 30000) {
                    nak("data_body");
                    bodyGot = 0;
                    break;
                }
                delay(1);
            }
            if (bodyGot != len) {
                break;
            }
        }
        const size_t wrote = out.write(buf, len);
        if (wrote != len) {
            nak("write");
            break;
        }
        hasher.update(buf, len);
        received += len;
        ++expectSeq;
        Serial.print("#ACK DATA seq=");
        Serial.println(seq);
        Serial.flush();
        delay(5);
    }

    free(buf);
    out.close();
    SD.remove(staging);
    return ok;
}

bool runBinarySession() {
    // Expect framed messages until Abort or transfer completes.
    const uint32_t sessionDeadline = millis() + 120000;
    while (millis() < sessionDeadline) {
        // Resync: discard until magic appears.
        uint8_t b0 = 0;
        if (!Serial.available()) {
            delay(1);
            continue;
        }
        if (!readExact(&b0, 1, 5000)) {
            continue;
        }
        if (b0 != static_cast<uint8_t>(kMagic & 0xFF)) {
            continue;
        }
        uint8_t rest[3];
        if (!readExact(rest, 3, 5000)) {
            continue;
        }
        const uint32_t magic = static_cast<uint32_t>(b0) | (static_cast<uint32_t>(rest[0]) << 8) |
                               (static_cast<uint32_t>(rest[1]) << 16) |
                               (static_cast<uint32_t>(rest[2]) << 24);
        if (magic != kMagic) {
            continue;
        }
        uint8_t ver = 0, type = 0, pad[2];
        if (!readExact(&ver, 1, 5000) || !readExact(&type, 1, 5000) || !readExact(pad, 2, 5000)) {
            nak("hdr");
            return false;
        }
        if (ver != kVersion) {
            nak("version");
            return false;
        }
        if (type == static_cast<uint8_t>(FrameType::Begin)) {
            return handleBeginSession();
        }
        if (type == static_cast<uint8_t>(FrameType::Verify)) {
            return handleVerify();
        }
        if (type == static_cast<uint8_t>(FrameType::Abort)) {
            ack("ABORT");
            return false;
        }
        nak("unexpected");
        return false;
    }
    nak("timeout");
    return false;
}

bool lineLooksLikeEnter(const char *line) {
    // Accept "#ESP2UPLOAD" with optional CR and whitespace.
    while (*line == ' ' || *line == '\t') {
        ++line;
    }
    return startsWith(line, "#ESP2UPLOAD");
}

} // namespace

bool sanitizeEsp2Path(const char *in, char *out, size_t outCap) {
    if (!in || !out || outCap < 8) {
        return false;
    }
    // Reject empty, relative, Windows, traversal, NUL injection.
    if (in[0] != '/') {
        return false;
    }
    if (std::strstr(in, "..") != nullptr) {
        return false;
    }
    if (std::strchr(in, '\\') != nullptr) {
        return false;
    }
    if (!startsWith(in, "/esp2/") && std::strcmp(in, "/esp2") != 0) {
        return false;
    }
    // Collapse duplicate slashes and copy.
    size_t o = 0;
    bool prevSlash = false;
    for (size_t i = 0; in[i]; ++i) {
        const char c = in[i];
        if (c < 0x20 || c == 0x7f) {
            return false;
        }
        if (c == '/') {
            if (prevSlash) {
                continue;
            }
            prevSlash = true;
        } else {
            prevSlash = false;
        }
        if (o + 1 >= outCap) {
            return false;
        }
        out[o++] = c;
    }
    if (o == 0) {
        return false;
    }
    // Must be a file path under /esp2/ (not just the root).
    if (o <= 6) { // "/esp2/" length 6 — need at least /esp2/x
        return false;
    }
    if (out[o - 1] == '/') {
        return false;
    }
    out[o] = 0;
    return true;
}

bool makeStagingPath(const char *finalPath, char *out, size_t outCap) {
    if (!finalPath || !out) {
        return false;
    }
    const int n = std::snprintf(out, outCap, "%s.upload", finalPath);
    return n > 0 && static_cast<size_t>(n) < outCap;
}

bool pollAndRunSession(uint32_t listenMs) {
    // Standalone USB-power (no CDC host): do not block — Serial.flush() can hang forever
    // without a host, leaving the AMOLED uninitialized if called before display bring-up.
    if (!Serial) {
        return false;
    }
    Serial.println("#ESP2UPLOAD WAIT");
    Serial.flush();
    const uint32_t t0 = millis();
    char line[96];
    size_t len = 0;
    uint32_t lastPing = 0;
    bool any = false;
    while ((millis() - t0) < listenMs) {
        if (!Serial) {
            // Host detached mid-wait — abort upload window, continue firmware boot.
            return any;
        }
        if ((millis() - lastPing) > 1000) {
            lastPing = millis();
            Serial.println("#ESP2UPLOAD WAIT");
            Serial.flush();
        }
        while (Serial.available()) {
            const int b = Serial.read();
            if (b < 0) {
                break;
            }
            if (b == '\n' || b == '\r') {
                if (len == 0) {
                    continue;
                }
                line[len] = 0;
                len = 0;
                if (lineLooksLikeEnter(line)) {
                    // SPI3 — must not claim SPI2 (CO5300 QSPI host).
                    if (!esp2SdBusBegin()) {
                        nak("sd");
                        return any;
                    }
                    // Boot-window upload runs before prepareEsp2Tree — seed roots.
                    if (!SD.exists("/esp2")) {
                        (void)SD.mkdir("/esp2");
                    }
                    if (!SD.exists("/esp2/roms")) {
                        (void)SD.mkdir("/esp2/roms");
                    }
                    if (!SD.exists("/esp2/disks")) {
                        (void)SD.mkdir("/esp2/disks");
                    }
                    if (!SD.exists("/esp2/tmp")) {
                        (void)SD.mkdir("/esp2/tmp");
                    }
                    if (!SD.exists("/esp2")) {
                        nak("mkdir_esp2");
                        return any;
                    }
                    // Stay in WAIT window so host can chain multiple files.
                    (void)runSessionNow();
                    any = true;
                    lastPing = 0; // reprint WAIT soon
                }
                continue;
            }
            if (len + 1 < sizeof(line)) {
                line[len++] = static_cast<char>(b);
            } else {
                len = 0;
            }
        }
        delay(1);
    }
    return any;
}

bool runSessionNow() {
    g_sessionActive = true;
    Serial.println("#ESP2UPLOAD READY v1");
    Serial.flush();
    const bool ok = runBinarySession();
    Serial.print("#ESP2UPLOAD DONE result=");
    Serial.println(ok ? "OK" : "FAIL");
    Serial.flush();
    g_sessionActive = false;
    return true;
}

bool isSessionActive() {
    return g_sessionActive;
}

} // namespace esp2_upload
