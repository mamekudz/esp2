#include "esp_bracket/disk_ii_media.hpp"

#include <cstring>

namespace esp_bracket {

bool Dos33NibbleImage::load(const uint8_t *data, size_t size, bool poOrder) {
    if (!data || size != kImageBytes) {
        return false;
    }
    std::memcpy(image_, data, kImageBytes);
    poOrder_ = poOrder;
    loaded_ = true;
    invalidateCache();
    return true;
}

void Dos33NibbleImage::clear() {
    std::memset(image_, 0, sizeof(image_));
    poOrder_ = false;
    loaded_ = true;
    writeProtected_ = true;
    invalidateCache();
}

void Dos33NibbleImage::invalidateCache() {
    cache_[0].track = -1;
    cache_[1].track = -1;
    cache_[0].length = 0;
    cache_[1].length = 0;
}

bool Dos33NibbleImage::writeSector(uint16_t track, uint16_t sector, const uint8_t src[256]) {
    if (!loaded_ || track >= kTracks || sector >= kSectors || !src) {
        return false;
    }
    const size_t off = (static_cast<size_t>(track) * kSectors + sector) * 256u;
    std::memcpy(image_ + off, src, 256);
    invalidateCache();
    return true;
}

bool Dos33NibbleImage::readSector(uint16_t track, uint16_t sector, uint8_t *dst256) {
    if (!loaded_ || !dst256 || track >= kTracks || sector >= kSectors) {
        return false;
    }
    const size_t off = (static_cast<size_t>(track) * kSectors + sector) * 256u;
    std::memcpy(dst256, image_ + off, 256);
    return true;
}

bool Dos33NibbleImage::buildTrackCached(int wholeTrack) {
    if (wholeTrack < 0 || wholeTrack >= kTracks) {
        return false;
    }
    for (CacheSlot &s : cache_) {
        if (s.track == wholeTrack && s.length > 0) {
            return true;
        }
    }
    // Evict slot 1, promote 0→1, build into 0 (simple 2-slot LRU).
    cache_[1] = cache_[0];
    CacheSlot &slot = cache_[0];
    slot.track = wholeTrack;

    uint8_t sectors[16][256];
    for (int sec = 0; sec < 16; ++sec) {
        int logical = sec;
        if (poOrder_) {
            // File stores ProDOS order; convert file sector index → DOS logical.
            logical = DiskIITrackBuilder::poSectorToLogical(sec);
            const size_t off =
                (static_cast<size_t>(wholeTrack) * 16u + static_cast<size_t>(sec)) * 256u;
            std::memcpy(sectors[logical], image_ + off, 256);
        } else {
            const size_t off =
                (static_cast<size_t>(wholeTrack) * 16u + static_cast<size_t>(sec)) * 256u;
            std::memcpy(sectors[sec], image_ + off, 256);
        }
    }
    // When poOrder, sectors[] filled by logical index above; when dsk, by DOS order.
    if (poOrder_) {
        // Ensure any unfilled — already written by logical.
    }

    slot.length = DiskIITrackBuilder::buildTrack(static_cast<uint8_t>(wholeTrack), volume_, sectors,
                                                 slot.nib, sizeof(slot.nib));
    return slot.length > 0;
}

const uint8_t *Dos33NibbleImage::trackNibbles(int wholeTrack, size_t *outLength) {
    if (!loaded_ || !outLength) {
        if (outLength) {
            *outLength = 0;
        }
        return nullptr;
    }
    if (!buildTrackCached(wholeTrack)) {
        *outLength = 0;
        return nullptr;
    }
    for (CacheSlot &s : cache_) {
        if (s.track == wholeTrack && s.length > 0) {
            *outLength = s.length;
            return s.nib;
        }
    }
    *outLength = 0;
    return nullptr;
}

bool NibTrackImage::load(const uint8_t *data, size_t size) {
    if (!data || size != kImageBytes) {
        return false;
    }
    std::memcpy(image_, data, kImageBytes);
    loaded_ = true;
    return true;
}

const uint8_t *NibTrackImage::trackNibbles(int wholeTrack, size_t *outLength) {
    if (!loaded_ || !outLength || wholeTrack < 0 || wholeTrack >= kTracks) {
        if (outLength) {
            *outLength = 0;
        }
        return nullptr;
    }
    *outLength = kTrackLen;
    return image_ + static_cast<size_t>(wholeTrack) * kTrackLen;
}

} // namespace esp_bracket
