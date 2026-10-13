#pragma once

/**
 * ESP][ Control Center — on-device touch frontend.
 * Business logic lives in display / Disk II / BLE / SD ownership services.
 */

#include <Arduino_GFX_Library.h>

#include "esp_bracket/control_center.hpp"

#include <cstdint>

namespace esp2_cc {

bool isOpen();
void open();
void close();

void draw(Arduino_GFX *gfx, int panelW, int panelH);
void tick(uint32_t nowMs);
/** Layout context (physical panel + visible system-UI viewport + present mode). */
void setLayoutContext(const esp_bracket::CcLayoutContext &ctx);
/** Full redraw needed (avoid AMOLED fillScreen every frame). */
bool uiDirty();

/**
 * Pointer sample in panel pixels (already mapped).
 * pressed/released are edges; contact is current finger down.
 */
bool onPointer(uint16_t x, uint16_t y, bool pressed, bool released, bool contact,
               bool wakeConsumed, int panelW, int panelH);

/** Physical-button navigation (RLCD etc. — no touch). */
enum class Button : uint8_t { Next = 0, Activate, Back };
void onButton(Button button);

} // namespace esp2_cc

namespace esp2_cc_svc {

enum class Monitor : uint8_t { White = 0, Green, Amber, Artifact };
enum class Effect : uint8_t { Clean = 0, Crt };
enum class Orient : uint8_t { Classic = 0, Landscape };
enum class SdOwnerUi : uint8_t { Esp2 = 0, Computer, Transition, Error, Unknown };

struct PresentSnap {
    Monitor monitor = Monitor::White;
    Effect effect = Effect::Clean;
    Orient orient = Orient::Classic;
};

struct DiskSnap {
    bool drive1Mounted = false;
    char drive1Name[40]{};
    bool drive1Motor = false;
    bool drive2Supported = false;
    bool sdUsable = false;
};

struct InputSnap {
    bool bleReady = false;
    bool keyboard = false;
    bool gamepad = false;
    bool pointer = false;
    uint8_t knownCount = 0;
    uint8_t connectedCount = 0;
};

struct StorageSnap {
    bool present = false;
    SdOwnerUi owner = SdOwnerUi::Unknown;
    bool mscSupported = false;
    bool fatOk = false;
    char capacityLine[40]{};
};

struct Services {
    const char *(*hardwareName)() = nullptr;
    void (*pauseEmu)() = nullptr;
    void (*resumeEmu)() = nullptr;
    PresentSnap (*present)() = nullptr;
    void (*setMonitor)(Monitor) = nullptr;
    void (*setEffect)(Effect) = nullptr;
    void (*setOrient)(Orient) = nullptr;
    DiskSnap (*disks)() = nullptr;
    int (*listDiskFiles)(char names[][32], int maxCount) = nullptr;
    bool (*mountDrive1)(const char *path) = nullptr;
    void (*ejectDrive1)() = nullptr;
    InputSnap (*input)() = nullptr;
    StorageSnap (*storage)() = nullptr;
    bool (*shareSd)() = nullptr;
    bool (*returnSd)() = nullptr;
    void (*resetApple)() = nullptr;
    void (*coldStart)() = nullptr;
    void (*restartEsp)() = nullptr;
    bool bleAvailable = false;
};

void setServices(const Services &s);
const Services &services();

} // namespace esp2_cc_svc
