#pragma once

/**
 * Small cooperative Apple II input macro engine (wait / key / waitHires).
 * Runs on the same keyboard latch path as live input — no RAM patches.
 */
#include <cstddef>
#include <cstdint>

namespace esp2_macro {

static constexpr int kSchemaVersion = 1;
static constexpr size_t kMaxMacros = 16;
static constexpr size_t kMaxActions = 32;
static constexpr size_t kIdMax = 48;
static constexpr uint32_t kMaxWaitMs = 180000;
static constexpr uint32_t kMaxTotalWaitMs = 300000;

enum class Op : uint8_t { Wait = 0, Key = 1, WaitHires = 2 };

struct Action {
    Op op = Op::Wait;
    uint32_t ms = 0;
    uint8_t key7 = 0;
};

struct Macro {
    char id[kIdMax]{};
    Action actions[kMaxActions]{};
    uint8_t actionCount = 0;
};

struct MacroBank {
    Macro macros[kMaxMacros]{};
    uint8_t count = 0;
    bool loaded = false;
};

enum class RunState : uint8_t { Idle = 0, Running = 1, Done = 2, Failed = 3 };

struct Runner {
    RunState state = RunState::Idle;
    const Macro *macro = nullptr;
    uint8_t index = 0;
    uint32_t waitDeadlineMs = 0;
    uint32_t totalWaitMs = 0;
    /** waitHires: first time condition was true (0 = not yet). */
    uint32_t hiresSinceMs = 0;
    char failReason[48]{};
    char activeId[kIdMax]{};
};

/** HGR must hold this long before waitHires succeeds (debounce). */
static constexpr uint32_t kHiresHoldMs = 400;

/** Parse macros.json (array of {id,actions}). */
bool parseMacrosJson(const char *json, size_t len, MacroBank *out, char *err, size_t errLen);

const Macro *findMacro(const MacroBank &bank, const char *id);

bool startMacro(Runner *r, const MacroBank &bank, const char *id, uint32_t nowMs, char *err,
                size_t errLen);

void stopMacro(Runner *r, const char *reason);

/**
 * Advance one cooperative step.
 * @param hiresNow current Apple II soft-switch HGR state
 * @param injectKey callback for 7-bit Apple key (same path as live input)
 * @return true if still running
 */
bool tickMacro(Runner *r, uint32_t nowMs, bool hiresNow, void (*injectKey)(uint8_t));

} // namespace esp2_macro
