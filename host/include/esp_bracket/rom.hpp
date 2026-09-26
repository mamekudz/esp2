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

/**
 * Project-owned ESP32 text-port ROM (NOT Apple firmware).
 * Writes multi-line status into text page 1 and markers at $03FC–$03FF.
 * Signature: $03FC=$A2 $03FD=$54 $03FE=$58 $03FF=$31 ("A2TX1").
 */
RomError generateSyntheticEsp32TextPortRom(uint8_t *dst, size_t dstSize);

/**
 * PART C video-pipeline diagnostic ROM (NOT Apple firmware).
 * RESET → TEXT banner + idle. Subroutine $E800 clears HGR page 1 ($2000–$3FFF).
 * Marker $03F8–$03FB = A2VP1.
 */
RomError generateSyntheticVideoPipelineRom(uint8_t *dst, size_t dstSize);

/** Documented Esp32 text-port success marker bytes at $03FC. */
inline constexpr uint8_t kEsp32TextPortMarker0 = 0xA2;
inline constexpr uint8_t kEsp32TextPortMarker1 = 0x54;
inline constexpr uint8_t kEsp32TextPortMarker2 = 0x58;
inline constexpr uint8_t kEsp32TextPortMarker3 = 0x31;
inline constexpr uint16_t kEsp32TextPortMarkerAddr = 0x03FC;

inline constexpr uint8_t kEsp32VideoPipeMarker0 = 0xA2;
inline constexpr uint8_t kEsp32VideoPipeMarker1 = 0x56;
inline constexpr uint8_t kEsp32VideoPipeMarker2 = 0x50;
inline constexpr uint8_t kEsp32VideoPipeMarker3 = 0x31;
inline constexpr uint16_t kEsp32VideoPipeMarkerAddr = 0x03F8;
inline constexpr uint16_t kEsp32HgrClearRoutine = 0xE800;

} // namespace esp_bracket
