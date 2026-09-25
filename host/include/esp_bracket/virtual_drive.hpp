#pragma once

#include "esp_bracket/apple2_machine.hpp"
#include "esp_bracket/media_types.hpp"

namespace esp_bracket {

/**
 * Virtual drive slot — no Disk II controller yet.
 * Holds mount metadata only; machine must not know filesystem paths.
 */
class VirtualDrive {
public:
    VirtualDrive() { reset(); }

    void reset();

    MediaResult mount(const char* imageId, DiskFormat format = DiskFormat::Unknown);
    MediaResult unmount();

    const VirtualDriveState& state() const { return state_; }

private:
    VirtualDriveState state_{};
};

} // namespace esp_bracket
