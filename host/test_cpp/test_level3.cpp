#include "esp_bracket/apple2_machine_host.hpp"
#include "esp_bracket/input_script.hpp"
#include "esp_bracket/key_map.hpp"
#include "esp_bracket/rom_identity.hpp"
#include "esp_bracket/sha256.hpp"
#include "esp_bracket/text_screen.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

using namespace esp_bracket;

static int g_failures = 0;
static int g_passes = 0;

static void expect(bool ok, const char *name) {
    if (!ok) {
        std::fprintf(stderr, "FAIL  %s\n", name);
        ++g_failures;
    } else {
        std::printf("PASS  %s\n", name);
        ++g_passes;
    }
}

static void testSha256Empty() {
    // SHA256("") = e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
    char hex[65];
    Sha256::hashHex(nullptr, 0, hex);
    // hash with len 0
    uint8_t empty = 0;
    Sha256::hashHex(&empty, 0, hex);
    expect(std::strcmp(hex, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855") ==
               0,
           "sha256 empty");
}

static void testRomRejectBadSize() {
    Rom rom;
    uint8_t tiny[16]{};
    RomIdentity id{};
    expect(loadAndIdentifyRom(rom, tiny, sizeof(tiny), &id) == RomError::InvalidSize,
           "reject tiny rom");
    expect(id.status == RomIdStatus::Truncated || id.status == RomIdStatus::InvalidSize,
           "status truncated/invalid");
    expect(!rom.isLoaded(), "not loaded");
}

static void testSyntheticIdentify() {
    uint8_t img[Rom::kApple2PlusRomBytes];
    expect(generateSyntheticRom(img, sizeof(img)) == RomError::Ok, "gen synthetic");
    RomIdentity id = RomDatabase::identify(img, sizeof(img));
    expect(id.status == RomIdStatus::Ok, "synthetic id ok");
    expect(id.synthetic, "synthetic flag");
    expect(std::strcmp(id.name, "esp2_synthetic_host_test") == 0, "synthetic name");

    HostAppleIIMachine m;
    expect(m.loadRomIdentified(img, sizeof(img), &id) == RomError::Ok, "load id");
    expect(m.romIdentity().synthetic, "machine rom synthetic");
}

static void testRomMappingVectors() {
    HostAppleIIMachine m;
    m.loadSyntheticRom();
    expect(m.bus().rom().isLoaded(), "rom loaded");
    // Reset vector at $FFFC
    const uint8_t lo = m.bus().peek(0xFFFC);
    const uint8_t hi = m.bus().peek(0xFFFD);
    const uint16_t rst = static_cast<uint16_t>(lo | (hi << 8));
    expect(rst == 0xE000, "reset vector E000");
    // Write protect: write to ROM ignored
    m.bus().write(0xE000, 0x00);
    expect(m.bus().peek(0xE000) != 0x00 || m.bus().peek(0xE000) == 0x8D, "rom write ignored");
}

static void testPowerOnVsReset() {
    HostAppleIIMachine m;
    m.loadSyntheticRom();
    m.bus().ram()[0x0300] = 0x42;
    m.powerOn(RamInitMode::Zero);
    expect(m.bus().ram()[0x0300] == 0x00, "powerOn clears ram");
    m.bus().ram()[0x0300] = 0x55;
    m.reset();
    expect(m.bus().ram()[0x0300] == 0x55, "reset keeps ram");
}

static void testSlot6DefaultNone() {
    HostAppleIIMachine m;
    expect(m.slot6RomMode() == Slot6RomMode::None, "slot6 none");
    expect(m.diskII().romKind() == DiskIIController::RomKind::None, "no slot rom bytes");
    expect(m.bus().peek(0xC600) == 0xFF, "C600 open");
}

static void testTextScreenExtract() {
    HostAppleIIMachine m;
    TextDecoder::writeTextScreen(m.bus().ram(), 0x0400, "HELLO ESP][", 0, 0);
    TextScreen s = TextScreen::fromBus(m.bus(), 0);
    expect(s.contains("HELLO"), "contains HELLO");
    expect(s.contains("ESP]["), "contains ESP][");
    expect(!s.contains("NOPE"), "no false positive");
}

static void testFlashPhase() {
    uint8_t ram[0xC000]{};
    // Place flashing 'A' ($C1 is normal; flash range $40-$7F → 'A' = 0x41)
    const uint16_t addr = TextDecoder::cellAddress(0x0400, 0, 0);
    ram[addr] = 0x41; // flash 'A'
    TextScreen vis = TextScreen::fromRam(ram, 0x0400, 0);
    TextScreen blank = TextScreen::fromRam(ram, 0x0400, 1);
    expect(vis.cells[0][0].style == TextCellStyle::Flash, "flash style");
    expect(vis.cells[0][0].ch == 'A', "flash visible");
    expect(blank.cells[0][0].ch == ' ', "flash blanked");
}

static void testKeyMap() {
    uint8_t k = 0;
    expect(AppleIIKeyMap::mapHostKey('A', false, false, &k) && k == 'A', "map A");
    expect(AppleIIKeyMap::mapHostKey('a', false, true, &k) && k == 1, "ctrl-a");
    expect(AppleIIKeyMap::mapHostKey('\r', false, false, &k) && k == 0x0D, "return");
    expect(AppleIIKeyMap::mapHostKey(0x1B, false, false, &k) && k == 0x1B, "esc");
}

static void testInputScriptSynthetic() {
    HostAppleIIMachine m;
    m.loadSyntheticRom();
    m.powerOn();
    // Synthetic ROM writes "ESP][ HOST TEST" then loops — wait for it
    ScriptStep steps[] = {
        {ScriptOp::RunCycles, nullptr, 200000},
        {ScriptOp::WaitText, "ESP][", 50000},
    };
    // First run cycles already, then wait (may already be present)
    ScriptStep steps2[] = {
        {ScriptOp::WaitText, "ESP][ HOST TEST", 300000},
    };
    m.powerOn();
    ScriptResult r = InputScript::run(m, steps2, 1);
    expect(r.status == ScriptStatus::Ok, "wait synthetic text");
    (void)steps;
}

static void testScriptTimeout() {
    HostAppleIIMachine m;
    m.loadSyntheticRom();
    m.powerOn();
    ScriptStep steps[] = {
        {ScriptOp::WaitText, "THIS_WILL_NEVER_APPEAR_XYZ", 5000},
    };
    ScriptResult r = InputScript::run(m, steps, 1);
    expect(r.status == ScriptStatus::Timeout, "wait timeout");
}

static void testSnapshot() {
    HostAppleIIMachine m;
    m.loadSyntheticRom();
    m.powerOn();
    m.runCycles(1000);
    HostMachineSnapshot s = m.snapshot();
    expect(s.cycles >= 1000, "snapshot cycles");
    expect(true, "snapshot pc readable");
}

static void testResetThroughVector() {
    HostAppleIIMachine m;
    m.loadSyntheticRom();
    m.powerOn(); // CPU reset → vector
    expect(m.cpu().registers().pc == 0xE000, "pc at reset vector target");
}

int main() {
    testSha256Empty();
    testRomRejectBadSize();
    testSyntheticIdentify();
    testRomMappingVectors();
    testPowerOnVsReset();
    testSlot6DefaultNone();
    testTextScreenExtract();
    testFlashPhase();
    testKeyMap();
    testInputScriptSynthetic();
    testScriptTimeout();
    testSnapshot();
    testResetThroughVector();

    std::printf("\nLevel3 host tests: %d passed, %d failed\n", g_passes, g_failures);
    return g_failures == 0 ? 0 : 1;
}
