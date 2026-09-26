#pragma once

#include "sd_ownership.hpp"

#include <cstdint>

namespace esp_bracket {

/**
 * USB Mass Storage (TinyUSB MSC) for the SPI microSD.
 *
 * Requires ARDUINO_USB_MODE=0 (OTG/TinyUSB). With ARDUINO_USB_CDC_ON_BOOT=1,
 * CDC serial and MSC form a composite device.
 *
 * While USB owns the card, ESP][ must not mount FAT or open files.
 */
class UsbStorageMode {
  public:
    explicit UsbStorageMode(SdOwnership &owner) : owner_(owner) {}

    /** True when toolchain/build enables TinyUSB MSC (USB_MODE==0). */
    static bool isSupported();

    /**
     * Stop FAT use, hand block device to Windows MSC.
     * Preconditions: no open application files; Disk II detached.
     */
    bool enter();

    /**
     * Tear down MSC and remount FAT for ESP][.
     * Call after Windows eject or explicit leave command.
     */
    bool leave(bool suspectUnsafeDisconnect = false);

    bool active() const { return active_; }
    bool ejectRequested() const { return ejectRequested_; }
    void clearEjectRequest() { ejectRequested_ = false; }
    void requestEject() { ejectRequested_ = true; }
    bool ownsBlockDevice() const { return owner_.usbOwnsBlock(); }

    uint32_t sectorCount() const { return sectorCount_; }
    uint16_t sectorSize() const { return sectorSize_; }
    uint8_t sdPdrv() const { return sdPdrv_; }

  private:
    friend int32_t usbMscOnRead(uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize);
    friend int32_t usbMscOnWrite(uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize);
    friend bool usbMscOnStartStop(uint8_t power_condition, bool start, bool load_eject);

    SdOwnership &owner_;
    bool active_ = false;
    bool ejectRequested_ = false;
    bool unsafe_ = false;
    uint8_t sdPdrv_ = 0xFF;
    uint32_t sectorCount_ = 0;
    uint16_t sectorSize_ = 512;
};

} // namespace esp_bracket
