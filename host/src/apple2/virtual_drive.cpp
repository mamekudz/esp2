#include "esp_bracket/virtual_drive.hpp"

#include <cstdio>
#include <cstring>

namespace esp_bracket {

void VirtualDrive::reset() {
    state_ = {};
    state_.track = -1;
}

MediaResult VirtualDrive::mount(const char* imageId, DiskFormat format) {
    if (!imageId || !imageId[0]) {
        return MediaResult::InvalidImage;
    }
    state_.inserted = true;
    state_.writeProtected = false;
    state_.dirty = false;
    state_.activity = false;
    state_.format = format;
    state_.track = 0;
    std::snprintf(state_.imageId, sizeof(state_.imageId), "%s", imageId);
    return MediaResult::Ok;
}

MediaResult VirtualDrive::unmount() {
    const bool was = state_.inserted;
    reset();
    return was ? MediaResult::Ok : MediaResult::NotFound;
}

} // namespace esp_bracket
