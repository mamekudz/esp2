#pragma once

#include <cstdint>
#include <cstddef>

#include "esp_bracket/types.hpp"

namespace esp_bracket {

enum class DriveId : uint8_t { Drive1 = 0, Drive2 = 1 };

struct JoystickState {
    int16_t x;
    int16_t y;
    bool button0;
    bool button1;
};

struct PaddleState {
    uint16_t p0;
    uint16_t p1;
    bool button0;
    bool button1;
};

struct SpeakerEvent {
    uint32_t cycle;
    uint8_t level;
};

struct VideoFrameState {
    uint8_t textPage;
    bool hires;
    bool mixed;
    bool page2;
    VideoColorMode colorMode;
};

enum class MediaResult : uint8_t {
    Ok = 0,
    NotFound,
    InvalidImage,
    Unsupported,
    Busy
};

class AppleIIMachine {
public:
    virtual ~AppleIIMachine() = default;
    virtual void reset() = 0;
    virtual void runCycles(uint32_t cycles) = 0;
    virtual void keyDown(uint8_t normalizedKey) = 0;
    virtual void keyUp(uint8_t normalizedKey) = 0;
    virtual void setJoystick(const JoystickState& state) = 0;
    virtual void setPaddle(const PaddleState& state) = 0;
    virtual MediaResult mountDisk(DriveId id, const char* imageId) = 0;
    virtual MediaResult unmountDisk(DriveId id) = 0;
    virtual VideoFrameState getVideoState() const = 0;
    virtual size_t consumeAudioEvents(SpeakerEvent* out, size_t max) = 0;
};

} // namespace esp_bracket
