/**
 * Host unit test: local control hotkey/chord engine + bluetooth.json parser.
 */
#include "esp2_bluetooth_config.hpp"
#include "esp_bracket/local_control.hpp"

#include <cstdio>
#include <cstring>

static int g_fails = 0;

#define CHECK(cond)                                                                                \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                            \
            ++g_fails;                                                                             \
        }                                                                                          \
    } while (0)

int main() {
    using namespace esp_bracket;

    LocalControlEngine eng;
    eng.setConfig(defaultLocalControlConfig());

    // Keyboard Ctrl+Alt+R → Reset
    KeyboardEvent ev{};
    ev.pressed = true;
    ev.ctrl = true;
    ev.alt = true;
    ev.hidUsage = 0x15; // R
    CHECK(eng.evaluateKeyboard(ev) == LocalAction::ResetAppleIi);

    // Partial modifiers must not fire
    ev.alt = false;
    CHECK(eng.evaluateKeyboard(ev) == LocalAction::None);

    // Ctrl+Alt+S → Settings
    ev.alt = true;
    ev.hidUsage = 0x16;
    CHECK(eng.evaluateKeyboard(ev) == LocalAction::OpenSettings);

    // Gamepad chord hold
    GamepadState pad{};
    pad.connected = true;
    pad.btnStart = true;
    pad.btnSelect = true;
    CHECK(eng.evaluateGamepad(pad, 1000) == LocalAction::None); // armed
    CHECK(eng.chordConsumingGamepad(pad));
    CHECK(eng.evaluateGamepad(pad, 2000) == LocalAction::None); // not yet 1.5s
    CHECK(eng.evaluateGamepad(pad, 2600) == LocalAction::OpenSettings);
    CHECK(eng.evaluateGamepad(pad, 3000) == LocalAction::None); // already fired

    // Release + Reset chord Start+B
    pad.btnSelect = false;
    pad.btnB = true;
    eng.resetChord();
    CHECK(eng.evaluateGamepad(pad, 4000) == LocalAction::None);
    CHECK(eng.evaluateGamepad(pad, 5600) == LocalAction::ResetAppleIi);

    // Malformed config falls back to usable defaults
    LocalControlConfig bad{};
    eng.setConfig(bad);
    ev.alt = true;
    ev.ctrl = true;
    ev.hidUsage = 0x15;
    CHECK(eng.evaluateKeyboard(ev) == LocalAction::ResetAppleIi);

    // bluetooth.json parse + known device upsert/forget
    const char *json = R"JSON({
  "schemaVersion": 1,
  "knownDevices": [
    {"address":"AA:BB:CC:DD:EE:FF","addrType":1,"name":"8BitDo SN30 Pro (BlueShift)",
     "deviceType":"gamepad","autoReconnect":true,"bonded":true}
  ],
  "control": {
    "resetAppleIi": {
      "keyboard": {"ctrl":true,"alt":true,"shift":false,"meta":false,"key":"R"},
      "gamepad": {"buttons":["start","b"],"holdMs":1500}
    },
    "openSettings": {
      "keyboard": {"ctrl":true,"alt":true,"shift":false,"meta":false,"key":"S"},
      "gamepad": {"buttons":["start","select"],"holdMs":1500}
    }
  }
})JSON";
    char err[64]{};
    auto cfg = esp2_ble_config::parseBluetoothConfigJson(json, std::strlen(json), err, sizeof(err));
    CHECK(cfg.valid);
    CHECK(cfg.knownCount == 1);
    CHECK(std::strstr(cfg.known[0].name, "(BlueShift)") != nullptr);
    CHECK(cfg.known[0].type == esp2_ble_config::DeviceType::Gamepad);
    CHECK(cfg.control.resetAppleIi.keyboard.hidUsage == 0x15);

    esp2_ble_config::KnownDevice kb{};
    std::strncpy(kb.address, "11:22:33:44:55:66", sizeof(kb.address) - 1);
    std::strncpy(kb.name, "Bluetooth Keyboard", sizeof(kb.name) - 1);
    kb.type = esp2_ble_config::DeviceType::Keyboard;
    CHECK(esp2_ble_config::upsertKnownDevice(&cfg, kb));
    CHECK(cfg.knownCount == 2);
    CHECK(esp2_ble_config::forgetKnownDevice(&cfg, "AA:BB:CC:DD:EE:FF"));
    CHECK(cfg.knownCount == 1);
    CHECK(esp2_ble_config::findKnownDevice(cfg, "11:22:33:44:55:66") == 0);

    // Identity is address, not display name
    CHECK(esp2_ble_config::findKnownDevice(cfg, "Bluetooth Keyboard") < 0);

    char out[2048];
    const size_t n = esp2_ble_config::serializeBluetoothConfigJson(cfg, out, sizeof(out));
    CHECK(n > 0);
    CHECK(std::strstr(out, "11:22:33:44:55:66") != nullptr);

    // BlueShift transparency: no special protocol branch — name preserved
    CHECK(std::strstr(json, "(BlueShift)") != nullptr);

    if (g_fails) {
        std::printf("FAILED %d checks\n", g_fails);
        return 1;
    }
    std::printf("test_local_control: PASS\n");
    return 0;
}
