#include "esp2_macro_engine.hpp"

#include <cctype>
#include <cstdio>
#include <cstring>

namespace esp2_macro {
namespace {

void setErr(char *err, size_t errLen, const char *msg) {
    if (!err || errLen == 0) {
        return;
    }
    strncpy(err, msg ? msg : "error", errLen - 1);
    err[errLen - 1] = 0;
}

const char *skipWs(const char *p, const char *end) {
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) {
        ++p;
    }
    return p;
}

bool parseQuoted(const char **pp, const char *end, char *out, size_t outLen) {
    const char *p = skipWs(*pp, end);
    if (p >= end || *p != '"' || outLen == 0) {
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
    if (p >= end || *p != '"') {
        return false;
    }
    out[i] = 0;
    *pp = p + 1;
    return true;
}

bool parseUInt(const char **pp, const char *end, uint32_t *out) {
    const char *p = skipWs(*pp, end);
    if (p >= end || !isdigit(static_cast<unsigned char>(*p))) {
        return false;
    }
    uint32_t v = 0;
    while (p < end && isdigit(static_cast<unsigned char>(*p))) {
        v = v * 10u + static_cast<uint32_t>(*p - '0');
        ++p;
    }
    *out = v;
    *pp = p;
    return true;
}

uint8_t keyFromToken(const char *tok) {
    if (!tok || !tok[0]) {
        return 0;
    }
    if (tok[1] == 0) {
        char c = tok[0];
        if (c >= 'a' && c <= 'z') {
            c = static_cast<char>(c - 'a' + 'A');
        }
        return static_cast<uint8_t>(c & 0x7f);
    }
    if (strcmp(tok, "RETURN") == 0 || strcmp(tok, "CR") == 0) {
        return 0x0d;
    }
    if (strcmp(tok, "ESC") == 0 || strcmp(tok, "ESCAPE") == 0) {
        return 0x1b;
    }
    if (strcmp(tok, "SPACE") == 0) {
        return 0x20;
    }
    // hex 2-digit
    if (strlen(tok) == 2 && isxdigit(static_cast<unsigned char>(tok[0])) &&
        isxdigit(static_cast<unsigned char>(tok[1]))) {
        unsigned v = 0;
        sscanf(tok, "%2x", &v);
        return static_cast<uint8_t>(v & 0x7f);
    }
    return 0;
}

bool parseActionObject(const char **pp, const char *end, Action *act) {
    const char *p = skipWs(*pp, end);
    if (p >= end || *p != '{') {
        return false;
    }
    ++p;
    char op[24]{};
    uint32_t ms = 0;
    char code[16]{};
    bool haveOp = false;
    while (p < end && *p != '}') {
        p = skipWs(p, end);
        if (p >= end) {
            return false;
        }
        if (*p == ',' || *p == '}') {
            if (*p == ',') {
                ++p;
            }
            continue;
        }
        char key[24]{};
        if (!parseQuoted(&p, end, key, sizeof(key))) {
            return false;
        }
        p = skipWs(p, end);
        if (p >= end || *p != ':') {
            return false;
        }
        ++p;
        if (strcmp(key, "op") == 0) {
            if (!parseQuoted(&p, end, op, sizeof(op))) {
                return false;
            }
            haveOp = true;
        } else if (strcmp(key, "ms") == 0 || strcmp(key, "wait") == 0) {
            if (!parseUInt(&p, end, &ms)) {
                return false;
            }
        } else if (strcmp(key, "code") == 0 || strcmp(key, "key") == 0) {
            // "A" or bare wait shorthand handled elsewhere
            p = skipWs(p, end);
            if (p < end && *p == '"') {
                if (!parseQuoted(&p, end, code, sizeof(code))) {
                    return false;
                }
            } else {
                return false;
            }
        } else {
            // skip unknown value (string/number/bool/null)
            p = skipWs(p, end);
            if (p < end && *p == '"') {
                char junk[64]{};
                if (!parseQuoted(&p, end, junk, sizeof(junk))) {
                    return false;
                }
            } else if (p < end && (isdigit(static_cast<unsigned char>(*p)) || *p == '-')) {
                while (p < end && (isdigit(static_cast<unsigned char>(*p)) || *p == '-' ||
                                   *p == '+' || *p == '.')) {
                    ++p;
                }
            } else if (p + 4 <= end && strncmp(p, "true", 4) == 0) {
                p += 4;
            } else if (p + 5 <= end && strncmp(p, "false", 5) == 0) {
                p += 5;
            } else if (p + 4 <= end && strncmp(p, "null", 4) == 0) {
                p += 4;
            } else {
                return false;
            }
        }
        p = skipWs(p, end);
        if (p < end && *p == ',') {
            ++p;
        }
    }
    if (p >= end || *p != '}') {
        return false;
    }
    *pp = p + 1;
    if (!haveOp) {
        return false;
    }
    if (strcmp(op, "wait") == 0) {
        act->op = Op::Wait;
        act->ms = ms > kMaxWaitMs ? kMaxWaitMs : ms;
        return act->ms > 0;
    }
    if (strcmp(op, "waitHires") == 0 || strcmp(op, "wait_hires") == 0) {
        act->op = Op::WaitHires;
        act->ms = ms == 0 ? kMaxWaitMs : (ms > kMaxWaitMs ? kMaxWaitMs : ms);
        return true;
    }
    if (strcmp(op, "key") == 0) {
        act->op = Op::Key;
        act->key7 = keyFromToken(code);
        return act->key7 != 0;
    }
    return false;
}

bool parseMacroObject(const char **pp, const char *end, Macro *m) {
    const char *p = skipWs(*pp, end);
    if (p >= end || *p != '{') {
        return false;
    }
    ++p;
    m->id[0] = 0;
    m->actionCount = 0;
    while (p < end && *p != '}') {
        p = skipWs(p, end);
        if (p < end && *p == ',') {
            ++p;
            continue;
        }
        if (p < end && *p == '}') {
            break;
        }
        char key[24]{};
        if (!parseQuoted(&p, end, key, sizeof(key))) {
            return false;
        }
        p = skipWs(p, end);
        if (p >= end || *p != ':') {
            return false;
        }
        ++p;
        if (strcmp(key, "id") == 0) {
            if (!parseQuoted(&p, end, m->id, sizeof(m->id))) {
                return false;
            }
        } else if (strcmp(key, "actions") == 0) {
            p = skipWs(p, end);
            if (p >= end || *p != '[') {
                return false;
            }
            ++p;
            while (p < end && *p != ']') {
                p = skipWs(p, end);
                if (p < end && *p == ',') {
                    ++p;
                    continue;
                }
                if (p < end && *p == ']') {
                    break;
                }
                if (m->actionCount >= kMaxActions) {
                    return false;
                }
                Action a{};
                if (!parseActionObject(&p, end, &a)) {
                    return false;
                }
                m->actions[m->actionCount++] = a;
            }
            if (p >= end || *p != ']') {
                return false;
            }
            ++p;
        } else {
            // skip
            p = skipWs(p, end);
            if (p < end && *p == '"') {
                char junk[64]{};
                parseQuoted(&p, end, junk, sizeof(junk));
            } else if (p < end && *p == '[') {
                int depth = 1;
                ++p;
                while (p < end && depth > 0) {
                    if (*p == '[') {
                        ++depth;
                    } else if (*p == ']') {
                        --depth;
                    }
                    ++p;
                }
            } else if (p < end && *p == '{') {
                int depth = 1;
                ++p;
                while (p < end && depth > 0) {
                    if (*p == '{') {
                        ++depth;
                    } else if (*p == '}') {
                        --depth;
                    }
                    ++p;
                }
            } else {
                while (p < end && *p != ',' && *p != '}') {
                    ++p;
                }
            }
        }
        p = skipWs(p, end);
        if (p < end && *p == ',') {
            ++p;
        }
    }
    if (p >= end || *p != '}') {
        return false;
    }
    *pp = p + 1;
    return m->id[0] != 0 && m->actionCount > 0;
}

} // namespace

bool parseMacrosJson(const char *json, size_t len, MacroBank *out, char *err, size_t errLen) {
    if (!out) {
        setErr(err, errLen, "null");
        return false;
    }
    *out = MacroBank{};
    if (!json || len == 0) {
        setErr(err, errLen, "empty");
        return false;
    }
    const char *end = json + len;
    const char *macrosKey = strstr(json, "\"macros\"");
    if (!macrosKey) {
        setErr(err, errLen, "no_macros");
        return false;
    }
    const char *p = macrosKey + 8;
    p = skipWs(p, end);
    if (p >= end || *p != ':') {
        setErr(err, errLen, "macros_colon");
        return false;
    }
    ++p;
    p = skipWs(p, end);
    if (p >= end || *p != '[') {
        setErr(err, errLen, "macros_array");
        return false;
    }
    ++p;
    while (p < end && *p != ']') {
        p = skipWs(p, end);
        if (p < end && *p == ',') {
            ++p;
            continue;
        }
        if (p < end && *p == ']') {
            break;
        }
        if (out->count >= kMaxMacros) {
            setErr(err, errLen, "too_many_macros");
            return false;
        }
        Macro m{};
        if (!parseMacroObject(&p, end, &m)) {
            setErr(err, errLen, "bad_macro");
            return false;
        }
        out->macros[out->count++] = m;
    }
    out->loaded = out->count > 0;
    if (!out->loaded) {
        setErr(err, errLen, "empty_macros");
        return false;
    }
    if (err && errLen) {
        err[0] = 0;
    }
    return true;
}

const Macro *findMacro(const MacroBank &bank, const char *id) {
    if (!id || !id[0]) {
        return nullptr;
    }
    for (uint8_t i = 0; i < bank.count; ++i) {
        if (strcmp(bank.macros[i].id, id) == 0) {
            return &bank.macros[i];
        }
    }
    return nullptr;
}

bool startMacro(Runner *r, const MacroBank &bank, const char *id, uint32_t nowMs, char *err,
                size_t errLen) {
    (void)nowMs;
    if (!r) {
        setErr(err, errLen, "null");
        return false;
    }
    const Macro *m = findMacro(bank, id);
    if (!m) {
        setErr(err, errLen, "not_found");
        return false;
    }
    *r = Runner{};
    r->state = RunState::Running;
    r->macro = m;
    r->index = 0;
    strncpy(r->activeId, m->id, sizeof(r->activeId) - 1);
    if (err && errLen) {
        err[0] = 0;
    }
    return true;
}

void stopMacro(Runner *r, const char *reason) {
    if (!r) {
        return;
    }
    r->state = RunState::Failed;
    r->macro = nullptr;
    strncpy(r->failReason, reason ? reason : "stop", sizeof(r->failReason) - 1);
}

bool tickMacro(Runner *r, uint32_t nowMs, bool hiresNow, void (*injectKey)(uint8_t)) {
    if (!r || r->state != RunState::Running || !r->macro) {
        return false;
    }
    if (r->index >= r->macro->actionCount) {
        r->state = RunState::Done;
        r->macro = nullptr;
        return false;
    }
    const Action a = r->macro->actions[r->index];
    switch (a.op) {
    case Op::Wait:
        if (r->waitDeadlineMs == 0) {
            if (r->totalWaitMs + a.ms > kMaxTotalWaitMs) {
                stopMacro(r, "total_wait");
                return false;
            }
            r->waitDeadlineMs = nowMs + a.ms;
            r->totalWaitMs += a.ms;
        }
        if (static_cast<int32_t>(nowMs - r->waitDeadlineMs) >= 0) {
            r->waitDeadlineMs = 0;
            ++r->index;
        }
        break;
    case Op::WaitHires:
        if (r->waitDeadlineMs == 0) {
            if (r->totalWaitMs + a.ms > kMaxTotalWaitMs) {
                stopMacro(r, "total_wait");
                return false;
            }
            r->waitDeadlineMs = nowMs + a.ms;
            r->totalWaitMs += a.ms;
            r->hiresSinceMs = 0;
        }
        if (hiresNow) {
            if (r->hiresSinceMs == 0) {
                r->hiresSinceMs = nowMs;
            } else if (static_cast<int32_t>(nowMs - r->hiresSinceMs) >=
                       static_cast<int32_t>(kHiresHoldMs)) {
                r->waitDeadlineMs = 0;
                r->hiresSinceMs = 0;
                ++r->index;
            }
        } else {
            r->hiresSinceMs = 0;
            if (static_cast<int32_t>(nowMs - r->waitDeadlineMs) >= 0) {
                stopMacro(r, "waitHires_timeout");
                return false;
            }
        }
        break;
    case Op::Key:
        if (injectKey) {
            injectKey(a.key7);
        }
        ++r->index;
        r->waitDeadlineMs = 0;
        break;
    }
    if (r->state == RunState::Running && r->index >= r->macro->actionCount) {
        r->state = RunState::Done;
        r->macro = nullptr;
        return false;
    }
    return r->state == RunState::Running;
}

} // namespace esp2_macro
