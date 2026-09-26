#include "usb_storage_mode.hpp"

#include "board_pins.h"

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <cstring>

#include "sd_diskio.h"

#if !ARDUINO_USB_MODE
#include "USB.h"
#include "USBMSC.h"
#endif

namespace esp_bracket {
namespace {

UsbStorageMode *g_mscInstance = nullptr;

#if !ARDUINO_USB_MODE
USBMSC g_msc;
#endif

uint8_t discoverPdrv() {
    for (uint8_t i = 0; i < 4; ++i) {
        if (sdcard_num_sectors(i) > 0 && sdcard_sector_size(i) > 0) {
            return i;
        }
    }
    return 0xFF;
}

} // namespace

int32_t usbMscOnRead(uint32_t lba, uint32_t /*offset*/, void *buffer, uint32_t bufsize) {
    if (!g_mscInstance || !g_mscInstance->ownsBlockDevice()) {
        return -1;
    }
    const uint16_t sec = g_mscInstance->sectorSize();
    if (!sec || (bufsize % sec) != 0) {
        return -1;
    }
    const uint32_t n = bufsize / sec;
    auto *dst = static_cast<uint8_t *>(buffer);
    for (uint32_t i = 0; i < n; ++i) {
        if (!sd_read_raw(g_mscInstance->sdPdrv(), dst + (i * sec), lba + i)) {
            return -1;
        }
    }
    return static_cast<int32_t>(bufsize);
}

int32_t usbMscOnWrite(uint32_t lba, uint32_t /*offset*/, uint8_t *buffer, uint32_t bufsize) {
    if (!g_mscInstance || !g_mscInstance->ownsBlockDevice()) {
        return -1;
    }
    const uint16_t sec = g_mscInstance->sectorSize();
    if (!sec || (bufsize % sec) != 0) {
        return -1;
    }
    const uint32_t n = bufsize / sec;
    for (uint32_t i = 0; i < n; ++i) {
        if (!sd_write_raw(g_mscInstance->sdPdrv(), buffer + (i * sec), lba + i)) {
            return -1;
        }
    }
    return static_cast<int32_t>(bufsize);
}

bool usbMscOnStartStop(uint8_t /*power_condition*/, bool start, bool load_eject) {
    if (!g_mscInstance) {
        return false;
    }
    if (load_eject && !start) {
        g_mscInstance->requestEject();
    }
    return true;
}

bool UsbStorageMode::isSupported() {
#if ARDUINO_USB_MODE
    return false; // Hardware CDC/JTAG — TinyUSB MSC unavailable
#else
    return true;
#endif
}

bool UsbStorageMode::enter() {
    if (!isSupported()) {
        owner_.setError("usb_mode_hw_cdc_no_msc");
        return false;
    }
    if (active_) {
        return true;
    }
    if (owner_.state() != SdOwnerState::Esp2OwnsSd) {
        owner_.setError("bad_owner_state");
        return false;
    }

    owner_.setState(SdOwnerState::TransitionToUsb);
    g_mscInstance = this;
    ejectRequested_ = false;
    unsafe_ = false;

    sdPdrv_ = discoverPdrv();
    if (sdPdrv_ == 0xFF) {
        owner_.setError("sd_pdrv");
        g_mscInstance = nullptr;
        return false;
    }
    sectorCount_ = sdcard_num_sectors(sdPdrv_);
    sectorSize_ = static_cast<uint16_t>(sdcard_sector_size(sdPdrv_));
    if (sectorCount_ == 0 || sectorSize_ == 0) {
        owner_.setError("sd_geometry");
        g_mscInstance = nullptr;
        return false;
    }

    // Drop FAT so Windows is the sole filesystem client.
    if (sdcard_unmount(sdPdrv_) != 0) {
        // Some builds return non-zero even when unmounted; continue if RAW works.
    }

#if !ARDUINO_USB_MODE
    g_msc.vendorID("ESP][");
    g_msc.productID("microSD");
    g_msc.productRevision("1.0");
    g_msc.onRead(usbMscOnRead);
    g_msc.onWrite(usbMscOnWrite);
    g_msc.onStartStop(usbMscOnStartStop);
    g_msc.mediaPresent(true);
    if (!g_msc.begin(sectorCount_, sectorSize_)) {
        owner_.setError("msc_begin");
        // Try remount FAT for recovery
        sdcard_mount(sdPdrv_, "/sd", 5, false);
        owner_.setState(SdOwnerState::Esp2OwnsSd);
        g_mscInstance = nullptr;
        return false;
    }
    // Ensure USB stack is running (CDC_ON_BOOT usually already called USB.begin).
    USB.begin();
#endif

    active_ = true;
    owner_.setState(SdOwnerState::UsbOwnsSd);
    return true;
}

bool UsbStorageMode::leave(bool suspectUnsafeDisconnect) {
    if (!active_ && owner_.state() != SdOwnerState::UsbOwnsSd &&
        owner_.state() != SdOwnerState::Error) {
        return owner_.esp2MayUseFat();
    }

    owner_.setState(SdOwnerState::TransitionToEsp2);
    unsafe_ = suspectUnsafeDisconnect;

#if !ARDUINO_USB_MODE
    g_msc.mediaPresent(false);
    g_msc.end();
#endif

    active_ = false;
    g_mscInstance = nullptr;

    // Remount FAT onto the still-initialized card.
    bool mounted = false;
    if (sdPdrv_ != 0xFF) {
        mounted = sdcard_mount(sdPdrv_, "/sd", 5, false);
    }
    if (!mounted) {
        // Full re-init path
        SPI.begin(PIN_SD_SCLK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
        if (SD.begin(PIN_SD_CS)) {
            sdPdrv_ = discoverPdrv();
            mounted = true;
        }
    }

    if (!mounted) {
        owner_.setError(unsafe_ ? "remount_after_unsafe" : "remount_fail");
        return false;
    }

    if (unsafe_) {
        // Mounted but mark recovery — caller should validate FS / rescan.
        owner_.setState(SdOwnerState::Esp2OwnsSd);
        return true;
    }

    owner_.setState(SdOwnerState::Esp2OwnsSd);
    ejectRequested_ = false;
    return true;
}

} // namespace esp_bracket
