#pragma once

#include <cstdint>

namespace esp_bracket {

/**
 * Apple II+ soft-switch model (authoritative for host machine).
 *
 * Any access (read or write) to $C050–$C057 updates state.
 * Keyboard strobe ($C010) and speaker ($C030) are handled by the bus,
 * not by this class — but address constants live here for one place.
 */
class SoftSwitches {
  public:
    // Display soft switches (II+)
    static constexpr uint16_t kAddrGraphics = 0xC050; // TEXT off
    static constexpr uint16_t kAddrText = 0xC051;     // TEXT on
    static constexpr uint16_t kAddrFull = 0xC052;     // MIXED off
    static constexpr uint16_t kAddrMixed = 0xC053;    // MIXED on
    static constexpr uint16_t kAddrPage1 = 0xC054;
    static constexpr uint16_t kAddrPage2 = 0xC055;
    static constexpr uint16_t kAddrLores = 0xC056; // HIRES off
    static constexpr uint16_t kAddrHires = 0xC057; // HIRES on

    static constexpr uint16_t kAddrKeyboardData = 0xC000;
    static constexpr uint16_t kAddrKeyboardStrobe = 0xC010;
    static constexpr uint16_t kAddrSpeaker = 0xC030;

    SoftSwitches() { reset(); }

    void reset();

    /** Decode $C050–$C057. Returns true if address was a display soft switch. */
    bool access(uint16_t address);

    bool isText() const { return text_; }
    bool isGraphics() const { return !text_; }
    bool isMixed() const { return mixed_; }
    bool isPage2() const { return page2_; }
    bool isHires() const { return hires_; }
    bool isLores() const { return !hires_; }

    void setText(bool on) { text_ = on; }
    void setMixed(bool on) { mixed_ = on; }
    void setPage2(bool on) { page2_ = on; }
    void setHires(bool on) { hires_ = on; }

  private:
    bool text_ = true;
    bool mixed_ = false;
    bool page2_ = false;
    bool hires_ = false;
};

} // namespace esp_bracket
