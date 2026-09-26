#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_bracket/errors.hpp"
#include "esp_bracket/rom.hpp"

namespace esp_bracket {

enum class MachineProfile : uint8_t { Unknown = 0, AppleII, AppleIIPlus };

enum class RomIdStatus : uint8_t {
    Ok = 0,
    InvalidSize,
    Truncated,
    UnknownHash,
    Unsupported,
    IoError
};

enum class Slot6RomMode : uint8_t { None = 0, Synthetic, UserSupplied };

struct RomIdentity {
    RomIdStatus status = RomIdStatus::UnknownHash;
    MachineProfile profile = MachineProfile::Unknown;
    size_t sizeBytes = 0;
    char sha256Hex[65]{};
    const char *name = "";       // stable id, not localized
    const char *provenance = ""; // evidence note
    bool synthetic = false;
};

/**
 * Metadata-only known ROM table (no ROM bytes).
 * Hashes included only with documented provenance; otherwise UNKNOWN.
 */
class RomDatabase {
  public:
    static RomIdentity lookupSha256Hex(const char *sha256Hex);
    static RomIdentity identify(const uint8_t *data, size_t size);
};

/** Validate and identify a motherboard ROM image in memory. */
RomError loadAndIdentifyRom(Rom &rom, const uint8_t *data, size_t size, RomIdentity *outIdentity);

const char *machineProfileName(MachineProfile p);
const char *romIdStatusName(RomIdStatus s);

} // namespace esp_bracket
