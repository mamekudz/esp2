#include "esp_bracket/game_io.hpp"

namespace esp_bracket {

void GameIo::reset() {
    for (int i = 0; i < 4; ++i) {
        ann_[i] = false;
    }
    for (int i = 0; i < 3; ++i) {
        button_[i] = false;
    }
    cassetteOut_ = false;
    cassetteIn_ = false;
    utilityStrobeCount_ = 0;
    for (int i = 0; i < kPaddleCount; ++i) {
        paddlePos_[i] = 128; // center
    }
    paddleStartCycle_ = 0;
    paddleTriggered_ = false;
}

bool GameIo::accessAnnunciator(uint16_t address) {
    if (address < 0xC058u || address > 0xC05Fu) {
        return false;
    }
    const int idx = static_cast<int>((address - 0xC058u) >> 1);
    const bool on = (address & 1u) != 0;
    if (idx >= 0 && idx < 4) {
        ann_[idx] = on;
    }
    return true;
}

bool GameIo::annunciator(int index) const {
    if (index < 0 || index >= 4) {
        return false;
    }
    return ann_[index];
}

void GameIo::setButton(int index, bool pressed) {
    if (index >= 0 && index < 3) {
        button_[index] = pressed;
    }
}

bool GameIo::button(int index) const {
    if (index < 0 || index >= 3) {
        return false;
    }
    return button_[index];
}

void GameIo::cassetteOutToggle() {
    cassetteOut_ = !cassetteOut_;
}

void GameIo::utilityStrobe() {
    ++utilityStrobeCount_;
}

void GameIo::setPaddle(int index, uint8_t position) {
    if (index >= 0 && index < kPaddleCount) {
        paddlePos_[index] = position;
    }
}

uint8_t GameIo::paddle(int index) const {
    if (index < 0 || index >= kPaddleCount) {
        return 0;
    }
    return paddlePos_[index];
}

void GameIo::triggerPaddles(uint32_t cycle) {
    paddleStartCycle_ = cycle;
    paddleTriggered_ = true;
}

uint8_t GameIo::readPaddleSense(int index, uint32_t cycle) const {
    if (index < 0 || index >= kPaddleCount || !paddleTriggered_) {
        return 0x00; // bit7 clear
    }
    const uint32_t duration =
        static_cast<uint32_t>(paddlePos_[index]) * kPaddleScale;
    const uint32_t elapsed = cycle - paddleStartCycle_;
    return (elapsed < duration) ? 0x80u : 0x00u;
}

uint8_t GameIo::readGameInput(uint16_t address, uint32_t cycle) const {
    // Decode low 3 bits; A3 selects mirror ($C068–$C06F).
    const uint8_t sel = static_cast<uint8_t>(address & 0x07u);
    switch (sel) {
    case 0: // cassette in / PB3 — bit7
        return cassetteIn_ ? 0x80u : 0x00u;
    case 1:
        return button_[0] ? 0x80u : 0x00u;
    case 2:
        return button_[1] ? 0x80u : 0x00u;
    case 3:
        return button_[2] ? 0x80u : 0x00u;
    case 4:
        return readPaddleSense(0, cycle);
    case 5:
        return readPaddleSense(1, cycle);
    case 6:
        return readPaddleSense(2, cycle);
    case 7:
        return readPaddleSense(3, cycle);
    default:
        return 0x00;
    }
}

} // namespace esp_bracket
