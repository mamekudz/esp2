#include "esp_bracket/input_script.hpp"

#include "esp_bracket/key_map.hpp"

#include <cstring>

namespace esp_bracket {

namespace {

void typeString(HostAppleIIMachine &m, const char *s) {
    if (!s) {
        return;
    }
    for (const char *p = s; *p; ++p) {
        uint8_t apple = 0;
        if (!AppleIIKeyMap::mapHostKey(static_cast<unsigned char>(*p), false, false, &apple)) {
            apple = AppleIIKeyMap::fromAscii(*p);
        }
        m.keyDown(apple);
        // Allow ROM to poll/clear strobe
        m.runCycles(500);
        m.bus().keyboard().clearStrobe();
        m.runCycles(200);
    }
}

} // namespace

ScriptResult InputScript::run(HostAppleIIMachine &machine, const ScriptStep *steps, int count,
                              int flashPhase) {
    ScriptResult r{};
    if (!steps || count <= 0) {
        r.status = ScriptStatus::BadStep;
        r.reason = "empty_script";
        return r;
    }
    for (int i = 0; i < count; ++i) {
        const ScriptStep &st = steps[i];
        r.stepIndex = i;
        switch (st.op) {
        case ScriptOp::RunCycles:
            machine.runCycles(st.cycles);
            r.cyclesUsed += st.cycles;
            break;
        case ScriptOp::Reset:
            machine.reset();
            break;
        case ScriptOp::Type:
            typeString(machine, st.text);
            break;
        case ScriptOp::KeyReturn: {
            uint8_t k = 0x0D;
            machine.keyDown(k);
            machine.runCycles(500);
            machine.bus().keyboard().clearStrobe();
            machine.runCycles(200);
            break;
        }
        case ScriptOp::KeyEscape: {
            machine.keyDown(0x1B);
            machine.runCycles(500);
            machine.bus().keyboard().clearStrobe();
            machine.runCycles(200);
            break;
        }
        case ScriptOp::WaitText: {
            if (!st.text || st.cycles == 0) {
                r.status = ScriptStatus::BadStep;
                r.reason = "wait_requires_text_and_budget";
                return r;
            }
            uint32_t left = st.cycles;
            const uint32_t slice = 1000;
            bool found = false;
            while (left > 0) {
                const uint32_t step = left > slice ? slice : left;
                machine.runCycles(step);
                r.cyclesUsed += step;
                left -= step;
                TextScreen scr = TextScreen::fromBus(machine.bus(), flashPhase);
                if (scr.contains(st.text)) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                r.status = ScriptStatus::Timeout;
                r.reason = "wait_text_timeout";
                return r;
            }
            break;
        }
        default:
            r.status = ScriptStatus::BadStep;
            r.reason = "unknown_op";
            return r;
        }
    }
    r.status = ScriptStatus::Ok;
    r.reason = "ok";
    return r;
}

} // namespace esp_bracket
