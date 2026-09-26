#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_bracket/disk_ii_track.hpp"
#include "esp_bracket/media_types.hpp"

namespace esp_bracket {

/**
 * Abstract nibble-track media for DiskIIController.
 * No filesystem / ESP32 knowledge.
 */
class NibbleTrackMedia {
  public:
    virtual ~NibbleTrackMedia() = default;
    virtual bool inserted() const = 0;
    virtual bool writeProtected() const = 0;
    virtual DiskFormat format() const = 0;
    /**
     * Return nibble stream for whole track 0..34.
     * Pointer valid until next call on this object (or eject).
     */
    virtual const uint8_t *trackNibbles(int wholeTrack, size_t *outLength) = 0;
    virtual void setWriteProtected(bool wp) = 0;
};

/**
 * Standard 16-sector DOS 3.3 /.dsk (and remapped /.po) image → nibble tracks.
 * Lazy per-track build with a 2-slot cache.
 */
class Dos33NibbleImage : public NibbleTrackMedia, public DiskImage {
  public:
    static constexpr int kTracks = 35;
    static constexpr int kSectors = 16;
    static constexpr size_t kImageBytes = kDos33ImageBytes;

    Dos33NibbleImage() = default;

    /** Copy image bytes. poOrder=true remaps ProDOS sector order per track. */
    bool load(const uint8_t *data, size_t size, bool poOrder = false);

    /** Zero-fill empty image (for builders). */
    void clear();

    uint8_t *raw() { return image_; }
    const uint8_t *raw() const { return image_; }

    bool writeSector(uint16_t track, uint16_t sector, const uint8_t src[256]);

    // NibbleTrackMedia
    bool inserted() const override { return loaded_; }
    bool writeProtected() const override { return writeProtected_; }
    DiskFormat format() const override { return poOrder_ ? DiskFormat::Po : DiskFormat::Dsk; }
    const uint8_t *trackNibbles(int wholeTrack, size_t *outLength) override;
    void setWriteProtected(bool wp) override { writeProtected_ = wp; }

    // DiskImage
    size_t sizeBytes() const override { return loaded_ ? kImageBytes : 0; }
    bool readSector(uint16_t track, uint16_t sector, uint8_t *dst256) override;

    void eject() {
        loaded_ = false;
        invalidateCache();
    }

    /** Lazy track-cache diagnostics (host + ESP32). */
    uint32_t cacheHits() const { return cacheHits_; }
    uint32_t cacheMisses() const { return cacheMisses_; }
    uint32_t trackBuildCount() const { return trackBuildCount_; }
    void clearCacheStats() {
        cacheHits_ = 0;
        cacheMisses_ = 0;
        trackBuildCount_ = 0;
    }
    /** Approximate bytes held in the 2-slot nibble cache. */
    size_t cacheBytesUsed() const;

  private:
    void invalidateCache();
    bool buildTrackCached(int wholeTrack);

    uint8_t image_[kImageBytes]{};
    bool loaded_ = false;
    bool poOrder_ = false;
    bool writeProtected_ = true; // V1 default read-only
    uint8_t volume_ = 254;
    uint32_t cacheHits_ = 0;
    uint32_t cacheMisses_ = 0;
    uint32_t trackBuildCount_ = 0;

    struct CacheSlot {
        int track = -1;
        size_t length = 0;
        uint8_t nib[DiskIITrackBuilder::kMaxTrackNibbles]{};
    };
    CacheSlot cache_[2]{};
};

/**
 * Raw .nib media (35 × fixed nibble track). Optional host path.
 * Track length 0x1A00 (6656) — common NIB layout.
 */
class NibTrackImage : public NibbleTrackMedia {
  public:
    static constexpr int kTracks = 35;
    static constexpr size_t kTrackLen = 0x1A00;
    static constexpr size_t kImageBytes = kTracks * kTrackLen;

    bool load(const uint8_t *data, size_t size);
    bool inserted() const override { return loaded_; }
    bool writeProtected() const override { return writeProtected_; }
    DiskFormat format() const override { return DiskFormat::Nib; }
    const uint8_t *trackNibbles(int wholeTrack, size_t *outLength) override;
    void setWriteProtected(bool wp) override { writeProtected_ = wp; }
    void eject() { loaded_ = false; }

  private:
    uint8_t image_[kImageBytes]{};
    bool loaded_ = false;
    bool writeProtected_ = true;
};

} // namespace esp_bracket
