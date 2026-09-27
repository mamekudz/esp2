#include "esp_bracket/disk_ii_controller.hpp"

#include <cstdio>
#include <cstring>

namespace esp_bracket {

namespace {

void syncDriveInserted(DiskIIDriveState &st, NibbleTrackMedia *m) {
    if (!m || !m->inserted()) {
        st.inserted = false;
        return;
    }
    st.inserted = true;
    st.writeProtected = m->writeProtected();
}

} // namespace

DiskIIController::DiskIIController() {
    reset();
}

void DiskIIController::reset() {
    motorOn_ = false;
    selectedDrive_ = 1;
    q6_ = false;
    q7_ = false;
    phases_ = 0;
    lastPhase_ = 0; // matches apple2js DriveState.phase initial value
    latch_ = 0;
    lastCycle_ = 0;
    rotationIndex_ = 0;
    lastActivity_ = DiskIIActivity::None;
    // Drive positions retained across Apple II reset (media stays).
    // Quarter-track positions kept; motor off.
}

uint8_t DiskIIController::ioRead(uint8_t offset, uint32_t cycle) {
    return access(offset, cycle, false, 0);
}

void DiskIIController::ioWrite(uint8_t offset, uint8_t value, uint32_t cycle) {
    (void)access(offset, cycle, true, value);
}

uint8_t DiskIIController::ioPeek(uint8_t offset) const {
    // Observe switch mirrors / latch without advancing rotation or changing Q.
    switch (offset & 0x0Fu) {
    case 0x0:
    case 0x1:
    case 0x2:
    case 0x3:
    case 0x4:
    case 0x5:
    case 0x6:
    case 0x7:
        return phases_;
    case 0x8:
    case 0x9:
        return motorOn_ ? 0x01 : 0x00;
    case 0xA:
    case 0xB:
        return static_cast<uint8_t>(selectedDrive_);
    case 0xC:
        return latch_;
    case 0xD:
        return q6_ ? 0x01 : 0x00;
    case 0xE:
        return q7_ ? 0x00 : 0x01; // Q7L sense-ish
    case 0xF:
        return q7_ ? 0x01 : 0x00;
    default:
        return 0xFF;
    }
}

uint8_t DiskIIController::romRead(uint16_t offset) {
    if (romKind_ == RomKind::None) {
        return 0xFF;
    }
    return rom_[offset & 0xFFu];
}

uint8_t DiskIIController::expansionRomRead(uint16_t offset) const {
    if (!expansionLoaded_ || offset >= kExpansionRomSize) {
        return 0xFF;
    }
    return expansion_[offset];
}

void DiskIIController::clearBootTrace() {
    bootTraceCount_ = 0;
}

void DiskIIController::pushBootTrace(DiskIIBootTraceEvent ev, uint32_t cycle, uint16_t detail) {
    if (bootTraceCount_ >= kBootTraceCap) {
        return;
    }
    bootTrace_[bootTraceCount_++] = DiskIIBootTraceEntry{ev, cycle, detail};
}

uint8_t DiskIIController::access(uint8_t offset, uint32_t cycle, bool isWrite, uint8_t writeValue) {
    (void)isWrite;
    (void)writeValue;
    applySwitch(static_cast<uint8_t>(offset & 0x0Fu), cycle);
    // Data register: Q6L ($C) while in read mode returns latch.
    if ((offset & 0x0Fu) == 0x0C && !q7_) {
        return readLatch(cycle);
    }
    if ((offset & 0x0Fu) == 0x0C && q7_) {
        // Write mode: reading Q6L often still shifts; return sense WP when Q6H path
        return latch_;
    }
    if ((offset & 0x0Fu) == 0x0E) {
        // Q7L — write-protect sense when Q6=1 (common sequence Q6H then Q7L)
        if (q6_) {
            return senseWriteProtect();
        }
    }
    return latch_;
}

void DiskIIController::applySwitch(uint8_t offset, uint32_t cycle) {
    advanceRotation(cycle);
    switch (offset) {
    case 0x0:
        updateStepper(0, false);
        break;
    case 0x1:
        updateStepper(0, true);
        break;
    case 0x2:
        updateStepper(1, false);
        break;
    case 0x3:
        updateStepper(1, true);
        break;
    case 0x4:
        updateStepper(2, false);
        break;
    case 0x5:
        updateStepper(2, true);
        break;
    case 0x6:
        updateStepper(3, false);
        break;
    case 0x7:
        updateStepper(3, true);
        break;
    case 0x8:
        if (motorOn_) {
            emitActivity(DiskIIActivity::MotorOff);
        }
        motorOn_ = false;
        traceLine(cycle, "motor", 0);
        break;
    case 0x9:
        if (!motorOn_) {
            emitActivity(DiskIIActivity::MotorOn);
            motorStartCycle_ = cycle;
            rotationIndex_ = 0;
            lastServedNibbleIndex_ = UINT32_MAX;
        }
        motorOn_ = true;
        lastCycle_ = cycle;
        traceLine(cycle, "motor", 1);
        break;
    case 0xA:
        selectedDrive_ = 1;
        emitActivity(DiskIIActivity::DriveSelect);
        traceLine(cycle, "drive", 1);
        break;
    case 0xB:
        selectedDrive_ = 2;
        emitActivity(DiskIIActivity::DriveSelect);
        traceLine(cycle, "drive", 2);
        break;
    case 0xC:
        q6_ = false;
        break;
    case 0xD:
        q6_ = true;
        break;
    case 0xE:
        q7_ = false;
        break;
    case 0xF:
        q7_ = true;
        if (!writesEnabled_ || senseWriteProtect() & 0x80) {
            emitActivity(DiskIIActivity::WriteAttempt);
        }
        break;
    default:
        break;
    }
}

void DiskIIController::updateStepper(uint8_t phase, bool on) {
    if (phase > 3) {
        return;
    }

    // Sather / apple2js: head positioning is enabled only while the drive motor
    // is on. Phase pulses with motor off must not move the arm.
    if (!motorOn_) {
        return;
    }

    if (!on) {
        // Phase OFF updates the magnet mask only. Movement is modeled on phase
        // ON transitions (wave / adjacent overlap simplified like apple2js).
        phases_ = static_cast<uint8_t>(phases_ & ~(1u << phase));
        return;
    }

    phases_ = static_cast<uint8_t>(phases_ | (1u << phase));

    // Quarter-track delta when phase Y is turned on while latched phase is X.
    // Derived from apple2js PHASE_DELTA (half-track units) × 2:
    //   adjacent ±1 half-track → ±2 quarter-tracks
    //   skip-one  ±1 full track → ±4 quarter-tracks
    // Sources: UtA2e p.9-12; apple2js js/cards/disk2.ts; Big Mess o' Wires Disk II.
    static constexpr int kPhaseDeltaQt[4][4] = {
        {0, 2, 4, -2},
        {-2, 0, 2, 4},
        {-4, -2, 0, 2},
        {2, -4, -2, 0},
    };

    const int from = (lastPhase_ >= 0 && lastPhase_ <= 3) ? lastPhase_ : 0;
    const int step = kPhaseDeltaQt[from][phase];
    if (step != 0) {
        DiskIIDriveState &st = activeDriveState();
        int qt = st.quarterTrack + step;
        if (qt < 0) {
            qt = 0;
        }
        if (qt > kMaxQuarterTrack) {
            qt = kMaxQuarterTrack;
        }
        if (qt != st.quarterTrack) {
            st.quarterTrack = qt;
            emitActivity(DiskIIActivity::Step);
            if (trace_) {
                traceLine(lastCycle_, "track", st.quarterTrack);
            }
        }
    }
    lastPhase_ = static_cast<int>(phase);
}

void DiskIIController::advanceRotation(uint32_t cycle) {
    lastCycle_ = cycle;
    if (!motorOn_) {
        return;
    }
    if (cycle < motorStartCycle_) {
        rotationIndex_ = rotationPhaseOffset_;
        return;
    }
    rotationIndex_ = rotationPhaseOffset_ + (cycle - motorStartCycle_) / kCyclesPerNibble;
}

uint8_t DiskIIController::readLatch(uint32_t cycle) {
    advanceRotation(cycle);
    emitActivity(DiskIIActivity::Read);

    if (!motorOn_) {
        latch_ = 0x00;
        return latch_;
    }
    NibbleTrackMedia *m = activeMedia();
    syncDriveInserted(activeDriveState(), m);
    if (!m || !m->inserted()) {
        latch_ = 0x7F; // bit7 clear — no valid data
        return latch_;
    }

    size_t len = 0;
    const int whole = activeDriveState().wholeTrack();
    const uint8_t *tr = m->trackNibbles(whole, &len);
    if (!tr || len == 0) {
        latch_ = 0x7F;
        return latch_;
    }
    const uint32_t idx = rotationIndex_ % static_cast<uint32_t>(len);
    // Authentic shift-register: first read in a nibble window returns bit7 set;
    // subsequent reads until the window advances return bit7 clear (BPL wait).
    if (idx != lastServedNibbleIndex_) {
        latch_ = tr[idx];
        lastServedNibbleIndex_ = idx;
        if (trace_) {
            traceLine(cycle, "read", latch_);
        }
        return latch_;
    }
    latch_ = static_cast<uint8_t>(tr[idx] & 0x7Fu);
    return latch_;
}

uint8_t DiskIIController::senseWriteProtect() const {
    NibbleTrackMedia *m = activeMedia();
    if (!m || !m->inserted()) {
        return 0xFF; // no disk → often appears protected
    }
    return m->writeProtected() ? 0xFF : 0x7F;
}

NibbleTrackMedia *DiskIIController::activeMedia() const {
    return selectedDrive_ == 2 ? media2_ : media1_;
}

DiskIIDriveState &DiskIIController::activeDriveState() {
    return selectedDrive_ == 2 ? drive2_ : drive1_;
}

const DiskIIDriveState &DiskIIController::activeDriveState() const {
    return selectedDrive_ == 2 ? drive2_ : drive1_;
}

DiskIIDriveState &DiskIIController::driveState(int drive) {
    return drive == 2 ? drive2_ : drive1_;
}

const DiskIIDriveState &DiskIIController::driveState(int drive) const {
    return drive == 2 ? drive2_ : drive1_;
}

void DiskIIController::emitActivity(DiskIIActivity a) {
    lastActivity_ = a;
}

void DiskIIController::traceLine(uint32_t cycle, const char *what, int value) const {
    if (!trace_) {
        return;
    }
    std::printf("[DISKII] cycle=%u %s=%d\n", cycle, what, value);
}

bool DiskIIController::loadSyntheticRom(const uint8_t *data, size_t size) {
    if (!data || size != kSlotRomSize) {
        return false;
    }
    std::memcpy(rom_, data, kSlotRomSize);
    expansionLoaded_ = false;
    romKind_ = RomKind::Synthetic;
    return true;
}

bool DiskIIController::loadCleanRoomRom(const uint8_t *prom256, const uint8_t *expansion2048) {
    if (!prom256 || !expansion2048) {
        return false;
    }
    std::memcpy(rom_, prom256, kSlotRomSize);
    std::memcpy(expansion_, expansion2048, kExpansionRomSize);
    expansionLoaded_ = true;
    romKind_ = RomKind::CleanRoom;
    return true;
}

bool DiskIIController::loadUserRom(const uint8_t *data, size_t size) {
    if (!data || size != kSlotRomSize) {
        return false;
    }
    std::memcpy(rom_, data, kSlotRomSize);
    expansionLoaded_ = false;
    romKind_ = RomKind::UserSupplied;
    return true;
}

void DiskIIController::clearRom() {
    std::memset(rom_, 0xFF, sizeof(rom_));
    std::memset(expansion_, 0xFF, sizeof(expansion_));
    expansionLoaded_ = false;
    romKind_ = RomKind::None;
}

uint32_t DiskIIController::romHash() const {
    if (romKind_ == RomKind::None) {
        return 0;
    }
    uint32_t h = 2166136261u;
    for (int i = 0; i < kSlotRomSize; ++i) {
        h ^= rom_[i];
        h *= 16777619u;
    }
    return h;
}

void DiskIIController::attachMedia(int drive, NibbleTrackMedia *media) {
    if (drive == 2) {
        media2_ = media;
        syncDriveInserted(drive2_, media);
    } else {
        media1_ = media;
        syncDriveInserted(drive1_, media);
    }
    emitActivity(DiskIIActivity::Insert);
}

NibbleTrackMedia *DiskIIController::media(int drive) const {
    return drive == 2 ? media2_ : media1_;
}

void DiskIIController::ejectDrive(int drive) {
    if (drive == 2) {
        media2_ = nullptr;
        drive2_.inserted = false;
        drive2_.dirty = false;
    } else {
        media1_ = nullptr;
        drive1_.inserted = false;
        drive1_.dirty = false;
    }
    emitActivity(DiskIIActivity::Eject);
}

DiskIIDiagState DiskIIController::diagState() const {
    DiskIIDiagState d{};
    d.romPresent = romKind_ != RomKind::None;
    d.romSynthetic = romKind_ == RomKind::Synthetic;
    d.romCleanRoom = romKind_ == RomKind::CleanRoom;
    d.motorOn = motorOn_;
    d.selectedDrive = selectedDrive_;
    d.q6 = q6_;
    d.q7 = q7_;
    d.phases = phases_;
    d.latch = latch_;
    d.rotationIndex = rotationIndex_;
    d.drive1 = drive1_;
    d.drive2 = drive2_;
    syncDriveInserted(const_cast<DiskIIDriveState &>(d.drive1), media1_);
    syncDriveInserted(const_cast<DiskIIDriveState &>(d.drive2), media2_);
    return d;
}

// ----- Synthetic Slot-6 ROM (project-owned, NOT Apple Disk II ROM) -----

size_t generateSyntheticDiskIISlotRom(uint8_t *dst, size_t dstCap) {
    if (!dst || dstCap < DiskIIController::kSlotRomSize) {
        return 0;
    }
    std::memset(dst, 0x60, DiskIIController::kSlotRomSize); // RTS fill

    // Entry $C600: exercise Disk II softswitches, then JMP $E200 (motherboard boot).
    // Bytes are placed at offset 0 of the 256-byte PROM.
    size_t i = 0;
    auto emit = [&](uint8_t b) {
        if (i < DiskIIController::kSlotRomSize) {
            dst[i++] = b;
        }
    };

    // LDA $C0E9 (motor on) — absolute
    emit(0xAD);
    emit(0xE9);
    emit(0xC0);
    // LDA $C0EA (drive 1)
    emit(0xAD);
    emit(0xEA);
    emit(0xC0);
    // LDA $C0EE (Q7L read mode)
    emit(0xAD);
    emit(0xEE);
    emit(0xC0);
    // LDA $C0E1 (phase0 on) — start recalibrate pulse
    emit(0xAD);
    emit(0xE1);
    emit(0xC0);
    // LDA $C0E0 (phase0 off)
    emit(0xAD);
    emit(0xE0);
    emit(0xC0);
    // Marker for host softswitch test: STA $03FD = $D2
    emit(0xA9);
    emit(0xD2);
    emit(0x8D);
    emit(0xFD);
    emit(0x03);
    // JMP $E200 — motherboard disk boot / loader
    emit(0x4C);
    emit(0x00);
    emit(0xE2);

    // Secondary entry at offset $40: motor off + RTS (for unit tests)
    i = 0x40;
    emit(0xAD);
    emit(0xE8);
    emit(0xC0); // motor off
    emit(0xA9);
    emit(0x01);
    emit(0x8D);
    emit(0xFC);
    emit(0x03);
    emit(0x60);

    return DiskIIController::kSlotRomSize;
}

// ----- Esp2DiskTest.dsk generator -----

bool generateEsp2DiskTestImage(uint8_t *dst, size_t dstCap) {
    if (!dst || dstCap < kDos33ImageBytes) {
        return false;
    }
    std::memset(dst, 0, kDos33ImageBytes);

    // Boot sector (track 0, DOS sector 0): write message + host marker.
    // Loaded to $0800 by motherboard boot routine.
    uint8_t *sec = dst; // T0S0
    size_t j = 0;
    auto b = [&](uint8_t v) { sec[j++] = v; };

    // STA $C051 / STA $C054
    b(0x8D);
    b(0x51);
    b(0xC0);
    b(0x8D);
    b(0x54);
    b(0xC0);

    const char *msg = "ESP][ DISK BOOT OK";
    uint16_t col = 0;
    for (const char *p = msg; *p; ++p, ++col) {
        const uint8_t ch = static_cast<uint8_t>(0x80u | static_cast<uint8_t>(*p));
        const uint16_t addr = static_cast<uint16_t>(0x0400 + col);
        b(0xA9);
        b(ch);
        b(0x8D);
        b(static_cast<uint8_t>(addr & 0xFF));
        b(static_cast<uint8_t>((addr >> 8) & 0xFF));
    }
    // LDA #$A5 / STA $03FF
    b(0xA9);
    b(0xA5);
    b(0x8D);
    b(0xFF);
    b(0x03);
    // JMP *
    const uint16_t loop = static_cast<uint16_t>(0x0800 + j);
    b(0x4C);
    b(static_cast<uint8_t>(loop & 0xFF));
    b(static_cast<uint8_t>((loop >> 8) & 0xFF));

    // Pattern sectors for encoding tests: T1S0 incremental, T1S1 zeros, T1S2 FFs
    for (int i = 0; i < 256; ++i) {
        dst[16 * 256 + i] = static_cast<uint8_t>(i); // T1S0
        dst[16 * 256 + 256 + i] = 0x00;              // T1S1
        dst[16 * 256 + 512 + i] = 0xFF;              // T1S2
    }
    return true;
}

} // namespace esp_bracket
