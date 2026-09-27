#pragma once

#include <cstddef>
#include <cstdint>

namespace esp2_upload {

/** Protocol magic 'ESPU' little-endian. */
static constexpr uint32_t kMagic = 0x55505345u;
static constexpr uint8_t kVersion = 1;
static constexpr uint32_t kDefaultChunk = 2048;
static constexpr uint32_t kMaxChunk = 4096;
static constexpr uint64_t kMaxFileBytes = 2ull * 1024ull * 1024ull; // 2 MiB
static constexpr size_t kMaxPathLen = 120;

enum class FrameType : uint8_t {
    Begin = 1,
    Data = 2,
    End = 3,
    Abort = 4,
    Verify = 5,
};

/**
 * Validate destination path: must be absolute under /esp2/, no traversal.
 * Writes normalized path (no trailing slash except root) into out, NUL-terminated.
 */
bool sanitizeEsp2Path(const char *in, char *out, size_t outCap);

/** Append ".upload" suffix for staging (outCap must fit). */
bool makeStagingPath(const char *finalPath, char *out, size_t outCap);

/**
 * Development-only serial media upload.
 * Call early after Serial.begin; returns true if a session ran.
 */
bool pollAndRunSession(uint32_t listenMs);

/**
 * Enter upload protocol immediately (SD must already be mounted by caller).
 * Prints READY and runs one framed transfer/verify session.
 */
bool runSessionNow();

/** True while a MEDIA binary session owns the CDC RX stream. */
bool isSessionActive();

} // namespace esp2_upload
