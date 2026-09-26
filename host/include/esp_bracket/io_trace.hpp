#pragma once

#include <cstdint>

namespace esp_bracket {

/** Optional host I/O trace sink (disabled by default — no hot-path cost). */
struct IoTraceEvent {
    uint32_t cycle;
    bool isWrite;
    uint16_t address;
    uint8_t value;
    const char* tag; // language-neutral token, not UI text
};

using IoTraceFn = void (*)(void* ctx, const IoTraceEvent& ev);

} // namespace esp_bracket
