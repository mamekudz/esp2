#pragma once

#include "esp_bracket/media_types.hpp"

#include <cstddef>
#include <cstdint>

namespace esp_bracket {

/**
 * microSD StorageBackend for ESP32 — wraps the verified Arduino SD API.
 * DiskIIController never sees FAT paths or SPI details.
 */
class Esp32SdStorageBackend : public StorageBackend {
  public:
    static constexpr const char *kDiskRoot = "/esp2/disks";
    static constexpr const char *kBootTestDsk = "/esp2/disks/Esp2BootTest.dsk";
    static constexpr const char *kBootTestPo = "/esp2/disks/Esp2BootTest.po";
    static constexpr const char *kBootTestNib = "/esp2/disks/Esp2BootTest.nib";
    static constexpr const char *kRomRoot = "/esp2/roms";
    /** Preferred deterministic system ROM filename (user-supplied, never in Git). */
    static constexpr const char *kSystemRom = "/esp2/roms/system.rom";
    static constexpr const char *kApple2PlusRom = "/esp2/roms/apple2plus.rom";
    static constexpr const char *kApple2Rom = "/esp2/roms/apple2.rom";
    /** User-supplied 256-byte Disk II Slot-6 PROM (Autostart path; never in Git). */
    static constexpr const char *kDiskIiProm = "/esp2/roms/diskii.prom";
    static constexpr const char *kDiskIiPromAlt = "/esp2/roms/diskii_apple2js_16.prom";
    static constexpr const char *kProfileFile = "/esp2/roms/profile.txt";

    bool beginMounted() { return mounted_; }
    void setMounted(bool m) { mounted_ = m; }

    bool exists(const char *path) const;
    size_t fileSize(const char *path) const;

    /** Read entire file into dst; fails if file larger than dstSize. */
    bool readAll(const char *path, uint8_t *dst, size_t dstSize, size_t *outSize) override;

    /** Create/overwrite file from buffer (used to seed project-owned fixtures). */
    bool writeAll(const char *path, const uint8_t *src, size_t size);

    /** Ensure /esp2/disks exists. */
    bool ensureDiskRoot();
    /** Ensure /esp2/roms exists. */
    bool ensureRomRoot();

    uint32_t lastReadUs() const { return lastReadUs_; }
    uint32_t lastWriteUs() const { return lastWriteUs_; }
    uint32_t readErrorCount() const { return readErrors_; }

  private:
    bool mounted_ = false;
    uint32_t lastReadUs_ = 0;
    uint32_t lastWriteUs_ = 0;
    uint32_t readErrors_ = 0;
};

} // namespace esp_bracket
