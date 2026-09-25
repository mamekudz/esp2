#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_bracket/errors.hpp"

namespace esp_bracket {

/**
 * Apple II+ ROM window: 12 KiB mapped at $D000–$FFFF.
 * NEVER commit copyrighted Apple ROMs — user-supplied or synthetic only.
 */
class Rom {
  public:
    static constexpr size_t kApple2PlusRomBytes = 12 * 1024; // 12288
    static constexpr uint16_t kMapBase = 0xD000;
    static constexpr uint16_t kMapEnd = 0xFFFF;

    Rom() = default;

    void clear();

    /** Load external image; must be exactly 12 KiB. */
    RomError load(const uint8_t *data, size_t size);

    /** Fill with project synthetic ROM (see synthetic_rom.cpp). */
    RomError loadSynthetic();

    bool isLoaded() const { return loaded_; }
    size_t size() const { return loaded_ ? kApple2PlusRomBytes : 0; }
    const uint8_t *data() const { return loaded_ ? data_ : nullptr; }

    /** Read byte at absolute Apple address in ROM window. */
    uint8_t readAbsolute(uint16_t address) const;

    /** Read by offset from $D000 (0 .. 12287). */
    uint8_t readOffset(uint16_t offset) const;

    RomError lastError() const { return lastError_; }

  private:
    uint8_t data_[kApple2PlusRomBytes]{};
    bool loaded_ = false;
    RomError lastError_ = RomError::NotLoaded;
};

/** Build synthetic 12K ROM image into dst[12288]. Returns RomError::Ok. */
RomError generateSyntheticRom(uint8_t *dst, size_t dstSize);

} // namespace esp_bracket
