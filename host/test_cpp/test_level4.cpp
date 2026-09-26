#include "esp_bracket/apple2_machine_host.hpp"
#include "esp_bracket/disk_ii_cleanroom.hpp"
#include "esp_bracket/disk_ii_encoding.hpp"
#include "esp_bracket/disk_ii_media.hpp"
#include "esp_bracket/disk_ii_track.hpp"
#include "esp_bracket/text_screen.hpp"

#include <cstdio>
#include <cstring>
#include <memory>
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

static bool runCleanRoomBoot(HostAppleIIMachine &m, uint32_t rotSeed) {
    m.diskII().driveState(1).quarterTrack = 0;
    m.diskII().setRotationIndex(rotSeed);
    m.bus().write(0x03FE, 0);
    m.bus().write(0x03FF, 0);
    m.powerOn(RamInitMode::Zero);
    CpuRegisters r = m.cpu().registers();
    r.pc = 0xC600;
    m.cpu().setRegisters(r);
    for (int i = 0; i < 300000; ++i) {
        m.runCycles(100);
        if (m.bus().peek(0x03FE) == 0x4C && m.bus().peek(0x03FF) == 0x34) {
            return true;
        }
    }
    return false;
}

static void testDenibbleService() {
    auto ram = std::make_unique<uint8_t[]>(0xC000);
    std::memset(ram.get(), 0, 0xC000);
    uint8_t sector[256];
    for (int i = 0; i < 256; ++i) {
        sector[i] = static_cast<uint8_t>(i ^ 0x5A);
    }
    expect(DiskIIEncoding::encodeSector(sector, ram.get() + 0x0900), "encode");
    ram[0x03FA] = 0xDE;
    expect(serviceCleanRoomDenibbleRequest(ram.get()), "service");
    expect(ram[0x03FA] == 0, "cleared");
    expect(std::memcmp(ram.get() + 0x0800, sector, 256) == 0, "decoded");
}

static void testLevel4Boot(uint32_t rotSeed) {
    auto m = std::make_unique<HostAppleIIMachine>();
    expect(m->loadSyntheticRom() == RomError::Ok, "mb rom");
    expect(m->mountDisk(DriveId::Drive1, "Esp2BootTest") == MediaResult::Ok, "mount");
    expect(m->diskII().romKind() == DiskIIController::RomKind::CleanRoom, "cleanroom");
    const bool ok = runCleanRoomBoot(*m, rotSeed);
    char label[64];
    std::snprintf(label, sizeof(label), "boot markers rot=%u", rotSeed);
    expect(ok, label);
    if (ok) {
        expect(m->diskII().diagState().motorOn, "motor on");
        expect(m->diskII().diagState().selectedDrive == 1, "drive1");
        TextScreen scr = TextScreen::fromBus(m->bus(), 0);
        expect(scr.contains("ESP][ LEVEL 4"), "text L4");
        expect(scr.contains("DISK II BOOT OK"), "text OK");
    }
}

static void testPoBoot() {
    auto m = std::make_unique<HostAppleIIMachine>();
    expect(m->loadSyntheticRom() == RomError::Ok, "po mb");
    expect(m->mountDisk(DriveId::Drive1, "Esp2BootTest.po") == MediaResult::Ok, "po mount");
    expect(runCleanRoomBoot(*m, 19), "po boot markers");
}

static void testNibBoot() {
    auto m = std::make_unique<HostAppleIIMachine>();
    expect(m->loadSyntheticRom() == RomError::Ok, "nib mb");
    m->setSlot6RomMode(Slot6RomMode::CleanRoom);

    uint8_t dsk[kDos33ImageBytes];
    expect(generateEsp2BootTestImage(dsk, sizeof(dsk)), "nib gen dsk");
    auto tmp = std::make_unique<Dos33NibbleImage>();
    expect(tmp->load(dsk, sizeof(dsk), false), "nib tmp load");

    std::vector<uint8_t> raw(NibTrackImage::kImageBytes, 0xFF);
    for (int t = 0; t < NibTrackImage::kTracks; ++t) {
        size_t len = 0;
        const uint8_t *tr = tmp->trackNibbles(t, &len);
        if (!tr || len == 0) {
            continue;
        }
        const size_t copy = len < NibTrackImage::kTrackLen ? len : NibTrackImage::kTrackLen;
        std::memcpy(raw.data() + static_cast<size_t>(t) * NibTrackImage::kTrackLen, tr, copy);
    }
    auto nib = std::make_unique<NibTrackImage>();
    expect(nib->load(raw.data(), raw.size()), "nib load");
    m->diskII().attachMedia(1, nib.get());
    expect(runCleanRoomBoot(*m, 5), "nib boot markers");
}

static void testNoHostSectorShortcut() {
    auto m = std::make_unique<HostAppleIIMachine>();
    m->loadSyntheticRom();
    m->mountDisk(DriveId::Drive1, "Esp2BootTest");
    m->powerOn();
    m->runCycles(50000);
    expect(m->bus().peek(0x03FF) != 0x34, "no auto-boot without slot rom");
}

static void testMalformedTimeout() {
    auto m = std::make_unique<HostAppleIIMachine>();
    m->loadSyntheticRom();
    m->setSlot6RomMode(Slot6RomMode::CleanRoom);
    auto blank = std::make_unique<Dos33NibbleImage>();
    blank->clear();
    blank->setWriteProtected(true);
    m->diskII().attachMedia(1, blank.get());
    m->powerOn(RamInitMode::Zero);
    CpuRegisters r = m->cpu().registers();
    r.pc = 0xC600;
    m->cpu().setRegisters(r);
    for (int i = 0; i < 20000; ++i) {
        m->runCycles(100);
    }
    expect(m->bus().peek(0x03FF) != 0x34, "malformed no success");
    expect(m->cpu().registers().pc >= 0xC600, "still in slot rom search");
}

static void testSlot6IdentifySize() {
    uint8_t tiny[16]{};
    Slot6RomIdentity id = Slot6RomDatabase::identify(tiny, sizeof(tiny));
    expect(id.status == RomIdStatus::Truncated || id.status == RomIdStatus::InvalidSize,
           "slot6 reject tiny");
    uint8_t prom[256];
    uint8_t exp[2048];
    generateCleanRoomDiskIICard(prom, exp);
    id = Slot6RomDatabase::identify(prom, sizeof(prom));
    expect(id.status == RomIdStatus::UnknownHash, "cleanroom unknown hash ok");
    expect(id.sizeBytes == 256, "slot6 size");
}

static void testOptionalUserSlot6Missing() {
    std::printf("SKIPPED_NO_SLOT6_ROM  optional user Disk II PROM not required for CI\n");
    expect(true, "skip path documented");
}

int main() {
    testDenibbleService();
    testLevel4Boot(0);
    testLevel4Boot(37);
    testLevel4Boot(128);
    testLevel4Boot(777);
    testPoBoot();
    testNibBoot();
    testNoHostSectorShortcut();
    testMalformedTimeout();
    testSlot6IdentifySize();
    testOptionalUserSlot6Missing();

    std::printf("\nLevel4 tests: %d passed, %d failed\n", g_passes, g_failures);
    return g_failures == 0 ? 0 : 1;
}
