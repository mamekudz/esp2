#include "esp32_sd_storage.hpp"

#include <Arduino.h>
#include <SD.h>

namespace esp_bracket {

bool Esp32SdStorageBackend::ensureDiskRoot() {
    if (!mounted_) {
        return false;
    }
    if (SD.exists(kDiskRoot)) {
        return true;
    }
    return SD.mkdir("/esp2") && SD.mkdir(kDiskRoot);
}

bool Esp32SdStorageBackend::ensureRomRoot() {
    if (!mounted_) {
        return false;
    }
    if (SD.exists(kRomRoot)) {
        return true;
    }
    return SD.mkdir("/esp2") && SD.mkdir(kRomRoot);
}

bool Esp32SdStorageBackend::exists(const char *path) const {
    return mounted_ && path && SD.exists(path);
}

size_t Esp32SdStorageBackend::fileSize(const char *path) const {
    if (!exists(path)) {
        return 0;
    }
    File f = SD.open(path, FILE_READ);
    if (!f) {
        return 0;
    }
    const size_t n = f.size();
    f.close();
    return n;
}

bool Esp32SdStorageBackend::readAll(const char *path, uint8_t *dst, size_t dstSize,
                                    size_t *outSize) {
    lastReadUs_ = 0;
    if (outSize) {
        *outSize = 0;
    }
    if (!mounted_ || !path || !dst || dstSize == 0) {
        ++readErrors_;
        return false;
    }
    const uint32_t t0 = micros();
    File f = SD.open(path, FILE_READ);
    if (!f) {
        lastReadUs_ = micros() - t0;
        ++readErrors_;
        return false;
    }
    const size_t sz = f.size();
    if (sz > dstSize) {
        f.close();
        lastReadUs_ = micros() - t0;
        ++readErrors_;
        return false;
    }
    size_t got = 0;
    while (got < sz) {
        const int n = f.read(dst + got, sz - got);
        if (n <= 0) {
            f.close();
            lastReadUs_ = micros() - t0;
            ++readErrors_;
            return false;
        }
        got += static_cast<size_t>(n);
    }
    f.close();
    lastReadUs_ = micros() - t0;
    if (outSize) {
        *outSize = got;
    }
    return got == sz;
}

bool Esp32SdStorageBackend::writeAll(const char *path, const uint8_t *src, size_t size) {
    lastWriteUs_ = 0;
    if (!mounted_ || !path || !src || size == 0) {
        return false;
    }
    if (!ensureDiskRoot()) {
        return false;
    }
    const uint32_t t0 = micros();
    if (SD.exists(path)) {
        SD.remove(path);
    }
    File f = SD.open(path, FILE_WRITE);
    if (!f) {
        lastWriteUs_ = micros() - t0;
        return false;
    }
    const size_t wrote = f.write(src, size);
    f.close();
    lastWriteUs_ = micros() - t0;
    return wrote == size;
}

} // namespace esp_bracket
