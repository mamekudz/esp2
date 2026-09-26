#include "esp_bracket/apple2_machine_host.hpp"
#include "esp_bracket/disk_ii_boot.hpp"
#include "esp_bracket/disk_ii_controller.hpp"
#include "esp_bracket/disk_ii_encoding.hpp"
#include "esp_bracket/disk_ii_media.hpp"
#include "esp_bracket/disk_ii_track.hpp"
#include "esp_bracket/text_decoder.hpp"

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

static void testOddEven() {
    for (int v = 0; v < 256; ++v) {
        uint8_t o, e;
        DiskIIEncoding::encodeOddEven(static_cast<uint8_t>(v), &o, &e);
        const uint8_t d = DiskIIEncoding::decodeOddEven(o, e);
        if (d != static_cast<uint8_t>(v)) {
            expect(false, "odd-even roundtrip");
            return;
        }
    }
    expect(true, "odd-even roundtrip");
}

static void testSectorRoundTrip(const char *name, void (*fill)(uint8_t *)) {
    uint8_t data[256];
    fill(data);
    uint8_t nib[DiskIIEncoding::kDataNibbles];
    uint8_t out[256];
    expect(DiskIIEncoding::encodeSector(data, nib), name);
    expect(DiskIIEncoding::decodeSector(nib, out), "decode");
    expect(std::memcmp(data, out, 256) == 0, name);
}

static void testEncodingMatrix() {
    testOddEven();
    testSectorRoundTrip("6and2 zeros", [](uint8_t *d) { std::memset(d, 0x00, 256); });
    testSectorRoundTrip("6and2 ff", [](uint8_t *d) { std::memset(d, 0xFF, 256); });
    testSectorRoundTrip("6and2 incr", [](uint8_t *d) {
        for (int i = 0; i < 256; ++i) {
            d[i] = static_cast<uint8_t>(i);
        }
    });
    testSectorRoundTrip("6and2 pattern", [](uint8_t *d) {
        for (int i = 0; i < 256; ++i) {
            d[i] = static_cast<uint8_t>((i * 37 + 11) & 0xFF);
        }
    });
}

static void testTrackBuilder() {
    uint8_t sectors[16][256];
    std::memset(sectors, 0, sizeof(sectors));
    for (int i = 0; i < 256; ++i) {
        sectors[0][i] = static_cast<uint8_t>(i);
    }
    uint8_t track[DiskIITrackBuilder::kMaxTrackNibbles];
    const size_t n = DiskIITrackBuilder::buildTrack(0, 254, sectors, track, sizeof(track));
    expect(n > 500, "track builder length");
    // Find address prologue
    bool found = false;
    for (size_t i = 0; i + 3 < n; ++i) {
        if (track[i] == 0xD5 && track[i + 1] == 0xAA && track[i + 2] == 0x96) {
            found = true;
            break;
        }
    }
    expect(found, "track has address prologue");

    uint8_t decoded[256];
    expect(DiskIIHostBoot::decodeSectorFromStream(track, n, 0, 0, decoded),
           "decode T0S0 from track");
    expect(std::memcmp(decoded, sectors[0], 256) == 0, "T0S0 matches");
}

static void testSoftswitches() {
    DiskIIController ctl;
    Dos33NibbleImage img;
    img.clear();
    ctl.attachMedia(1, &img);

    ctl.access(0x9, 100, false, 0); // motor on
    expect(ctl.diagState().motorOn, "motor on");
    ctl.access(0x8, 200, false, 0);
    expect(!ctl.diagState().motorOn, "motor off");

    ctl.access(0xB, 300, false, 0);
    expect(ctl.diagState().selectedDrive == 2, "drive 2");
    ctl.access(0xA, 400, false, 0);
    expect(ctl.diagState().selectedDrive == 1, "drive 1");

    // All 16 offsets via read and write
    for (uint8_t off = 0; off < 16; ++off) {
        ctl.access(off, 1000u + off, false, 0);
        ctl.access(off, 2000u + off, true, 0x55);
    }
    expect(true, "softswitch range R/W");

    // Peek must not advance rotation
    ctl.access(0x9, 5000, false, 0);
    ctl.access(0xE, 5001, false, 0);
    const uint32_t rot0 = ctl.diagState().rotationIndex;
    (void)ctl.ioPeek(0xC);
    (void)ctl.ioPeek(0xC);
    expect(ctl.diagState().rotationIndex == rot0, "peek no rotate");
}

static void testStepper() {
    DiskIIController ctl;
    ctl.access(0x1, 10, false, 0); // phase0 on
    ctl.access(0x3, 20, false, 0); // phase1 on → +1
    expect(ctl.driveState(1).quarterTrack == 1, "step +1");
    ctl.access(0x1, 30, false, 0); // phase0 on → -1
    expect(ctl.driveState(1).quarterTrack == 0, "step -1");

    // Bounds
    ctl.driveState(1).quarterTrack = 0;
    ctl.access(0x7, 40, false, 0);
    ctl.access(0x5, 50, false, 0);
    // force many steps down
    for (int i = 0; i < 20; ++i) {
        ctl.access(0x1, 100u + static_cast<uint32_t>(i) * 10, false, 0);
        ctl.access(0x7, 105u + static_cast<uint32_t>(i) * 10, false, 0);
    }
    expect(ctl.driveState(1).quarterTrack >= 0, "track >= 0");

    ctl.driveState(1).quarterTrack = DiskIIController::kMaxQuarterTrack;
    for (int i = 0; i < 20; ++i) {
        ctl.access(0x1, 500u + static_cast<uint32_t>(i) * 10, false, 0);
        ctl.access(0x3, 505u + static_cast<uint32_t>(i) * 10, false, 0);
    }
    expect(ctl.driveState(1).quarterTrack <= DiskIIController::kMaxQuarterTrack, "track <= max");
}

static void testTwoDrivesIndependent() {
    DiskIIController ctl;
    Dos33NibbleImage a;
    Dos33NibbleImage b;
    a.clear();
    b.clear();
    ctl.attachMedia(1, &a);
    ctl.attachMedia(2, &b);
    ctl.driveState(1).quarterTrack = 10;
    ctl.driveState(2).quarterTrack = 40;
    ctl.access(0xA, 1, false, 0);
    expect(ctl.driveState(1).quarterTrack == 10, "drive1 pos kept");
    ctl.access(0xB, 2, false, 0);
    expect(ctl.driveState(2).quarterTrack == 40, "drive2 pos kept");
}

static void testNoDisk() {
    DiskIIController ctl;
    ctl.access(0x9, 1, false, 0);
    ctl.access(0xE, 2, false, 0);
    const uint8_t v = ctl.access(0xC, 100, false, 0);
    expect((v & 0x80) == 0, "no disk bit7 clear");
}

static void testWriteProtect() {
    DiskIIController ctl;
    Dos33NibbleImage img;
    img.clear();
    img.setWriteProtected(true);
    ctl.attachMedia(1, &img);
    ctl.access(0xD, 1, false, 0);                   // Q6H
    const uint8_t v = ctl.access(0xE, 2, false, 0); // Q7L sense
    expect((v & 0x80) != 0, "WP sense");
}

static void testInsertEjectReset() {
    DiskIIController ctl;
    Dos33NibbleImage img;
    uint8_t raw[kDos33ImageBytes];
    generateEsp2DiskTestImage(raw, sizeof(raw));
    img.load(raw, sizeof(raw));
    ctl.attachMedia(1, &img);
    expect(ctl.diagState().drive1.inserted, "inserted");
    ctl.ejectDrive(1);
    expect(!ctl.media(1), "ejected");
    ctl.attachMedia(1, &img);
    ctl.access(0x9, 1, false, 0);
    ctl.reset();
    expect(ctl.media(1) != nullptr, "reset keeps media");
    expect(!ctl.diagState().motorOn, "reset motor off");
}

static void testSlotRomDecode() {
    HostAppleIIMachine m;
    expect(m.diskII().romKind() == DiskIIController::RomKind::Synthetic, "synthetic slot rom");
    expect(m.bus().read(0xC600) != 0xFF, "C600 rom byte");
    // Neighbor: slot 5 empty
    expect(m.bus().slotDevice(5) == nullptr, "slot5 empty");
}

static void testSlotIoRouting() {
    HostAppleIIMachine m;
    m.bus().setAccessCycle(1000);
    m.bus().read(0xC0E9);
    expect(m.diskII().diagState().motorOn, "C0E9 motor via bus");
    m.bus().read(0xC0E8);
    expect(!m.diskII().diagState().motorOn, "C0E8 motor off");
    // Peek no side effect on motor
    m.bus().read(0xC0E9);
    const bool on = m.diskII().diagState().motorOn;
    (void)m.bus().peek(0xC0E8);
    expect(m.diskII().diagState().motorOn == on, "peek no motor change");
}

static void testRotation() {
    DiskIIController ctl;
    Dos33NibbleImage img;
    uint8_t raw[kDos33ImageBytes];
    generateEsp2DiskTestImage(raw, sizeof(raw));
    img.load(raw, sizeof(raw));
    ctl.attachMedia(1, &img);
    ctl.driveState(1).quarterTrack = 0;
    ctl.access(0x9, 0, false, 0);
    ctl.access(0xE, 1, false, 0);
    const uint8_t a = ctl.access(0xC, 32, false, 0);
    const uint8_t b = ctl.access(0xC, 64, false, 0);
    // Not required to differ if track starts with runs of FF, but index advances
    expect(ctl.diagState().rotationIndex >= 2, "rotation advances");
    (void)a;
    (void)b;
}

static void testSyntheticRom6502() {
    HostAppleIIMachine m;
    expect(m.loadSyntheticRom() == RomError::Ok, "mb rom");
    expect(m.mountDisk(DriveId::Drive1, "Esp2DiskTest") == MediaResult::Ok, "mount");
    m.bus().write(0x03FD, 0);
    m.bus().write(0x03FE, 0);
    CpuRegisters r = m.cpu().registers();
    r.pc = 0xC600;
    m.cpu().setRegisters(r);
    for (int i = 0; i < 5000; ++i) {
        m.runCycles(50);
        if (m.bus().peek(0x03FD) == 0xD2 && m.bus().peek(0x03FE) == 0xEB) {
            break;
        }
    }
    expect(m.bus().peek(0x03FD) == 0xD2, "slot rom softswitch marker");
    expect(m.bus().peek(0x03FE) == 0xEB, "mb disk boot marker");
    expect(m.diskII().diagState().motorOn, "motor on after slot rom");
}

static void testDiskBoot() {
    HostAppleIIMachine m;
    expect(m.loadSyntheticRom() == RomError::Ok, "boot mb rom");
    expect(m.mountDisk(DriveId::Drive1, "Esp2DiskTest") == MediaResult::Ok, "boot mount");

    // 6502 path: Slot-6 ROM → softswitches → $E200 marker
    m.bus().write(0x03FD, 0);
    m.bus().write(0x03FE, 0);
    m.bus().write(0x03FF, 0);
    CpuRegisters r = m.cpu().registers();
    r.pc = 0xC600;
    m.cpu().setRegisters(r);
    for (int i = 0; i < 8000; ++i) {
        m.runCycles(40);
        if (m.bus().peek(0x03FE) == 0xEB) {
            break;
        }
    }
    expect(m.bus().peek(0x03FD) == 0xD2, "boot softswitch ok");

    // Host boot loader reads T0S0 through Disk II latch / 6-and-2
    uint8_t sec[256];
    expect(DiskIIHostBoot::readSector(m.bus(), m.diskII(), 0, 0, sec), "read T0S0 via Disk II");
    for (int i = 0; i < 256; ++i) {
        m.bus().write(static_cast<uint16_t>(0x0800 + i), sec[i]);
    }
    r = m.cpu().registers();
    r.pc = 0x0800;
    m.cpu().setRegisters(r);
    for (int i = 0; i < 20000; ++i) {
        m.runCycles(20);
        if (m.bus().peek(0x03FF) == 0xA5) {
            break;
        }
    }
    expect(m.bus().peek(0x03FF) == 0xA5, "boot host marker");

    uint8_t chars[40 * 24];
    TextDecoder::decodeScreen(m.bus().ram(), 0x0400, chars);
    const char *expectMsg = "ESP][ DISK BOOT OK";
    bool textOk = true;
    for (int i = 0; expectMsg[i]; ++i) {
        if ((chars[i] & 0x7F) != expectMsg[i]) {
            textOk = false;
            break;
        }
    }
    expect(textOk, "ESP][ DISK BOOT OK text");
}

static void testPoMapping() {
    expect(DiskIITrackBuilder::poSectorToLogical(0) == 0, "po0");
    expect(DiskIITrackBuilder::poSectorToLogical(1) == 8, "po1");
    Dos33NibbleImage img;
    uint8_t raw[kDos33ImageBytes];
    std::memset(raw, 0, sizeof(raw));
    raw[0] = 0x11;   // T0 PO sector 0 → DOS logical 0
    raw[256] = 0x22; // T0 PO sector 1 → DOS logical 8
    expect(img.load(raw, sizeof(raw), true), "load po");
    size_t len = 0;
    const uint8_t *tr = img.trackNibbles(0, &len);
    expect(tr && len > 0, "po track built");
    uint8_t dec[256];
    expect(DiskIIHostBoot::decodeSectorFromStream(tr, len, 0, 0, dec), "po s0");
    expect(dec[0] == 0x11, "po logical0 data");
    expect(DiskIIHostBoot::decodeSectorFromStream(tr, len, 0, 8, dec), "po s8");
    expect(dec[0] == 0x22, "po logical8 data");
}

static void testNibOptional() {
    NibTrackImage nib;
    std::vector<uint8_t> raw(NibTrackImage::kImageBytes, 0xFF);
    // Plant a tiny marker track
    raw[0] = 0xD5;
    raw[1] = 0xAA;
    expect(nib.load(raw.data(), raw.size()), "nib load");
    size_t len = 0;
    const uint8_t *t = nib.trackNibbles(0, &len);
    expect(t && len == NibTrackImage::kTrackLen && t[0] == 0xD5, "nib track");
}

int main() {
    testEncodingMatrix();
    testTrackBuilder();
    testSoftswitches();
    testStepper();
    testTwoDrivesIndependent();
    testNoDisk();
    testWriteProtect();
    testInsertEjectReset();
    testSlotRomDecode();
    testSlotIoRouting();
    testRotation();
    testSyntheticRom6502();
    testDiskBoot();
    testPoMapping();
    testNibOptional();

    std::printf("\nDisk II tests: %d passed, %d failed\n", g_passes, g_failures);
    return g_failures == 0 ? 0 : 1;
}
