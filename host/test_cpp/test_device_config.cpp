/**
 * Host unit test for esp2_system_config + esp2_macro_engine parsers.
 */
#include "esp2_macro_engine.hpp"
#include "esp2_system_config.hpp"

#include <cstdio>
#include <cstring>

static int g_fails = 0;
static uint8_t g_lastKey = 0;

#define CHECK(cond)                                                                            \
    do {                                                                                       \
        if (!(cond)) {                                                                         \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                         \
            ++g_fails;                                                                         \
        }                                                                                      \
    } while (0)

static void inject(uint8_t k) {
    g_lastKey = k;
}

int main() {
    const char *sys = R"JSON({
  "schemaVersion": 1,
  "machine": { "rom": "/esp2/roms/system.rom" },
  "media": { "drive1": "/esp2/disks/Galaxian.dsk", "drive2": null },
  "startup": { "bootFromDisk": true, "macro": "galaxian-start" },
  "presentation": { "orientation": "landscape", "color": "artifact" },
  "display": { "screensaverSeconds": 300 }
})JSON";
    char err[64]{};
    auto cfg = esp2_config::parseSystemConfigJson(sys, std::strlen(sys), err, sizeof(err));
    CHECK(cfg.valid);
    CHECK(cfg.bootFromDisk);
    CHECK(cfg.orientation == esp2_config::Orient::Landscape);
    CHECK(cfg.color == esp2_config::ColorMode::Artifact);
    CHECK(cfg.screensaverSeconds == 300);
    CHECK(std::strcmp(cfg.startupMacro, "galaxian-start") == 0);

    auto badCfg =
        esp2_config::parseSystemConfigJson("{\"schemaVersion\":99}", 20, err, sizeof(err));
    CHECK(!badCfg.valid);

    const char *mac = R"JSON({
  "schemaVersion": 1,
  "macros": [{
    "id": "galaxian-start",
    "actions": [
      { "op": "waitHires", "ms": 90000 },
      { "op": "wait", "ms": 100 },
      { "op": "key", "code": "A" }
    ]
  }]
})JSON";
    esp2_macro::MacroBank bank{};
    CHECK(esp2_macro::parseMacrosJson(mac, std::strlen(mac), &bank, err, sizeof(err)));
    CHECK(bank.count == 1);
    CHECK(bank.macros[0].actions[2].key7 == 0x41);

    esp2_macro::Runner r{};
    CHECK(esp2_macro::startMacro(&r, bank, "galaxian-start", 0, err, sizeof(err)));
    // waitHires needs hold debounce
    CHECK(esp2_macro::tickMacro(&r, 1, true, inject));
    CHECK(esp2_macro::tickMacro(&r, 500, true, inject));
    // wait 100ms
    CHECK(esp2_macro::tickMacro(&r, 500, true, inject));
    CHECK(esp2_macro::tickMacro(&r, 700, true, inject));
    // key
    esp2_macro::tickMacro(&r, 701, true, inject);
    CHECK(g_lastKey == 0x41);
    CHECK(r.state == esp2_macro::RunState::Done);

    if (g_fails) {
        std::printf("device config host tests FAILED (%d)\n", g_fails);
        return 1;
    }
    std::printf("All device config host tests passed\n");
    return 0;
}
