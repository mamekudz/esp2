#pragma once

#include <cstdint>
#include <cstddef>

namespace esp_bracket {

enum class DiskFormat : uint8_t { Unknown = 0, Dsk, Po, Nib, Woz };

struct DiskGeometry {
    uint16_t tracks;
    uint16_t sectorsPerTrack;
    uint16_t sectorSize;
};

inline constexpr DiskGeometry kDos33Geometry{35, 16, 256};
inline constexpr size_t kDos33ImageBytes =
    size_t(kDos33Geometry.tracks) * kDos33Geometry.sectorsPerTrack *
    kDos33Geometry.sectorSize; // 143360

struct VirtualDriveState {
    bool inserted;
    bool writeProtected;
    bool dirty;
    bool activity;
    char imageId[64];
    DiskFormat format;
    int16_t track; // -1 unknown
};

class StorageBackend {
public:
    virtual ~StorageBackend() = default;
    virtual bool readAll(const char* path, uint8_t* dst, size_t dstSize,
                         size_t* outSize) = 0;
};

class DiskImage {
public:
    virtual ~DiskImage() = default;
    virtual DiskFormat format() const = 0;
    virtual size_t sizeBytes() const = 0;
    virtual bool readSector(uint16_t track, uint16_t sector, uint8_t* dst256) = 0;
};

} // namespace esp_bracket
