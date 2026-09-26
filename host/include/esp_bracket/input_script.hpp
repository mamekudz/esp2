#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_bracket/apple2_machine_host.hpp"
#include "esp_bracket/text_screen.hpp"

namespace esp_bracket {

enum class ScriptStatus : uint8_t { Ok = 0, Timeout, Failed, BadStep };

enum class ScriptOp : uint8_t { WaitText = 0, Type, KeyReturn, KeyEscape, RunCycles, Reset };

struct ScriptStep {
    ScriptOp op = ScriptOp::RunCycles;
    const char *text = nullptr; // WaitText needle or Type string
    uint32_t cycles = 0;        // RunCycles amount or WaitText budget
};

struct ScriptResult {
    ScriptStatus status = ScriptStatus::Ok;
    int stepIndex = -1;
    uint32_t cyclesUsed = 0;
    const char *reason = "";
};

/**
 * Deterministic scripted input through normalized keyDown path.
 * Every WaitText requires a cycle budget (no infinite wait).
 */
class InputScript {
  public:
    static ScriptResult run(HostAppleIIMachine &machine, const ScriptStep *steps, int count,
                            int flashPhase = 0);
};

} // namespace esp_bracket
