#pragma once

#include <cstdint>
#include <cstddef>

namespace esp_bracket {

/**
 * Built-in game I/O + cassette + annunciators + paddle timers (II/II+).
 * Transport (BLE/ADC) stays outside — only normalized state here.
 */
class GameIo {
public:
    static constexpr int kPaddleCount = 4;

    GameIo() { reset(); }

    void reset();

    // --- Annunciators AN0–AN3 ---
    bool accessAnnunciator(uint16_t address); // $C058–$C05F any access
    bool annunciator(int index) const;

    // --- Pushbuttons PB0–PB2 (+ PB3/cassette-in sense on $C060) ---
    void setButton(int index, bool pressed); // 0..2
    bool button(int index) const;

    // --- Cassette (logical only) ---
    void cassetteOutToggle();
    bool cassetteOut() const { return cassetteOut_; }
    void setCassetteIn(bool high) { cassetteIn_ = high; }
    bool cassetteIn() const { return cassetteIn_; }

    // --- Utility strobe $C040–$C04F ---
    void utilityStrobe();
    uint32_t utilityStrobeCount() const { return utilityStrobeCount_; }

    // --- Paddles ---
    /** Position 0..255 → timer duration in cycles (scaled). */
    void setPaddle(int index, uint8_t position);
    uint8_t paddle(int index) const;
    void triggerPaddles(uint32_t cycle);
    /** bit7 = 1 while timer still running for that paddle. */
    uint8_t readPaddleSense(int index, uint32_t cycle) const;

    /** Read $C060–$C06F family (mirrored every 8). */
    uint8_t readGameInput(uint16_t address, uint32_t cycle) const;

private:
    bool ann_[4]{};
    bool button_[3]{};
    bool cassetteOut_ = false;
    bool cassetteIn_ = false;
    uint32_t utilityStrobeCount_ = 0;
    uint8_t paddlePos_[kPaddleCount]{};
    uint32_t paddleStartCycle_ = 0;
    bool paddleTriggered_ = false;
    static constexpr uint32_t kPaddleScale = 11; // ~0..2805 cycles for 0..255
};

} // namespace esp_bracket
