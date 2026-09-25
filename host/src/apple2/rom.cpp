#include "esp_bracket/rom.hpp"

#include <cstring>

namespace esp_bracket {

void Rom::clear() {
    std::memset(data_, 0, sizeof(data_));
    loaded_ = false;
    lastError_ = RomError::NotLoaded;
}

RomError Rom::load(const uint8_t *data, size_t size) {
    if (!data) {
        lastError_ = RomError::IoError;
        loaded_ = false;
        return lastError_;
    }
    if (size != kApple2PlusRomBytes) {
        lastError_ = RomError::InvalidSize;
        loaded_ = false;
        return lastError_;
    }
    std::memcpy(data_, data, kApple2PlusRomBytes);
    loaded_ = true;
    lastError_ = RomError::Ok;
    return lastError_;
}

RomError Rom::loadSynthetic() {
    const RomError err = generateSyntheticRom(data_, sizeof(data_));
    loaded_ = (err == RomError::Ok);
    lastError_ = err;
    return lastError_;
}

uint8_t Rom::readAbsolute(uint16_t address) const {
    if (!loaded_ || address < kMapBase) {
        return 0xFF;
    }
    const uint16_t offset = static_cast<uint16_t>(address - kMapBase);
    return readOffset(offset);
}

uint8_t Rom::readOffset(uint16_t offset) const {
    if (!loaded_ || offset >= kApple2PlusRomBytes) {
        return 0xFF;
    }
    return data_[offset];
}

} // namespace esp_bracket
