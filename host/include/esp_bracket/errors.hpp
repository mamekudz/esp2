#pragma once

#include <cstdint>
#include <cstddef>

namespace esp_bracket {

enum class CpuError : uint8_t {
    Ok = 0,
    Uninitialized,
    NoMemoryCallbacks,
    ExecLimitExceeded
};

enum class RomError : uint8_t {
    Ok = 0,
    NotLoaded,
    InvalidSize,
    WriteProtected,
    HashUnknown,
    IoError
};

enum class MachineError : uint8_t {
    Ok = 0,
    NotReady,
    RomMissing,
    InvalidState
};

enum class VideoError : uint8_t {
    Ok = 0,
    InvalidMode,
    BufferTooSmall
};

enum class DiagResult : uint8_t {
    Pass = 0,
    Fail,
    NotTested
};

struct DiagReport {
    DiagResult cpu;
    DiagResult ram;
    DiagResult rom;
    DiagResult softswitch;
    DiagResult keyboard;
    DiagResult speaker;
    DiagResult text;
    DiagResult lores;
    DiagResult hgr;
};

} // namespace esp_bracket
