#include "esp_bracket/rom_identity.hpp"

#include "esp_bracket/sha256.hpp"

#include <cstdio>
#include <cstring>

namespace esp_bracket {

namespace {

struct KnownRom {
    const char *sha256Hex;
    MachineProfile profile;
    const char *name;
    const char *provenance;
    bool synthetic;
};

// Only entries with project-owned or clearly documented hash evidence.
// Do NOT invent commercial Apple ROM hashes without provenance.
// Users may extend via local config (Node tooling); this table is compile-time.
const KnownRom kKnown[] = {
    // Filled at runtime for synthetic via identifySynthetic helper — placeholder
    // none for Apple ROMs in-repo.
};

bool hexEq(const char *a, const char *b) {
    if (!a || !b) {
        return false;
    }
    for (int i = 0; i < 64; ++i) {
        char ca = a[i];
        char cb = b[i];
        if (ca >= 'A' && ca <= 'F') {
            ca = static_cast<char>(ca - 'A' + 'a');
        }
        if (cb >= 'A' && cb <= 'F') {
            cb = static_cast<char>(cb - 'A' + 'a');
        }
        if (ca != cb) {
            return false;
        }
    }
    return a[64] == '\0' || a[64] == 0;
}

} // namespace

const char *machineProfileName(MachineProfile p) {
    switch (p) {
    case MachineProfile::AppleII:
        return "AppleII";
    case MachineProfile::AppleIIPlus:
        return "AppleIIPlus";
    default:
        return "Unknown";
    }
}

const char *romIdStatusName(RomIdStatus s) {
    switch (s) {
    case RomIdStatus::Ok:
        return "OK";
    case RomIdStatus::InvalidSize:
        return "INVALID_SIZE";
    case RomIdStatus::Truncated:
        return "TRUNCATED";
    case RomIdStatus::UnknownHash:
        return "UNKNOWN_ROM";
    case RomIdStatus::Unsupported:
        return "UNSUPPORTED_ROM";
    case RomIdStatus::IoError:
        return "IO_ERROR";
    default:
        return "UNKNOWN";
    }
}

RomIdentity RomDatabase::lookupSha256Hex(const char *sha256Hex) {
    RomIdentity id{};
    id.status = RomIdStatus::UnknownHash;
    if (!sha256Hex) {
        return id;
    }
    std::snprintf(id.sha256Hex, sizeof(id.sha256Hex), "%s", sha256Hex);
    for (const KnownRom &k : kKnown) {
        if (hexEq(k.sha256Hex, sha256Hex)) {
            id.status = RomIdStatus::Ok;
            id.profile = k.profile;
            id.name = k.name;
            id.provenance = k.provenance;
            id.synthetic = k.synthetic;
            id.sizeBytes = Rom::kApple2PlusRomBytes;
            return id;
        }
    }
    return id;
}

RomIdentity RomDatabase::identify(const uint8_t *data, size_t size) {
    RomIdentity id{};
    if (!data) {
        id.status = RomIdStatus::IoError;
        return id;
    }
    id.sizeBytes = size;
    if (size == 0) {
        id.status = RomIdStatus::Truncated;
        return id;
    }
    if (size != Rom::kApple2PlusRomBytes) {
        // Common wrong sizes
        if (size < Rom::kApple2PlusRomBytes) {
            id.status = RomIdStatus::Truncated;
        } else {
            id.status = RomIdStatus::InvalidSize;
        }
        Sha256::hashHex(data, size > 256 ? 256 : size, id.sha256Hex); // partial note
        // Prefer hashing full buffer only when size is manageable; for wrong size
        // still hash what we have up to size.
        Sha256::hashHex(data, size, id.sha256Hex);
        return id;
    }
    Sha256::hashHex(data, size, id.sha256Hex);

    // ROM image is $D000–$FFFF → offset of $E000 is 0x1000; $FFFC is 0x2FFC.
    if (data[0x2FFC] == 0x00 && data[0x2FFD] == 0xE0) { // reset → $E000
        if (data[0x1000] == 0x8D && data[0x1001] == 0x51 && data[0x1002] == 0xC0) {
            id.status = RomIdStatus::Ok;
            id.profile = MachineProfile::AppleIIPlus; // map compatible
            id.name = "esp2_synthetic_host_test";
            id.provenance = "project-owned synthetic; not Apple firmware";
            id.synthetic = true;
            return id;
        }
    }

    RomIdentity known = lookupSha256Hex(id.sha256Hex);
    if (known.status == RomIdStatus::Ok) {
        known.sizeBytes = size;
        std::memcpy(known.sha256Hex, id.sha256Hex, sizeof(known.sha256Hex));
        return known;
    }
    id.status = RomIdStatus::UnknownHash;
    id.profile = MachineProfile::Unknown;
    id.name = "unrecognized";
    id.provenance = "hash not in project metadata DB";
    return id;
}

RomError loadAndIdentifyRom(Rom &rom, const uint8_t *data, size_t size, RomIdentity *outIdentity) {
    RomIdentity id = RomDatabase::identify(data, size);
    if (outIdentity) {
        *outIdentity = id;
    }
    if (id.status == RomIdStatus::IoError) {
        return RomError::IoError;
    }
    if (id.status == RomIdStatus::InvalidSize || id.status == RomIdStatus::Truncated ||
        id.status == RomIdStatus::Unsupported) {
        return RomError::InvalidSize;
    }
    // Unknown hash is still loadable if size is correct (user-supplied).
    if (size != Rom::kApple2PlusRomBytes) {
        return RomError::InvalidSize;
    }
    return rom.load(data, size);
}

Slot6RomIdentity Slot6RomDatabase::lookupSha256Hex(const char *sha256Hex) {
    Slot6RomIdentity id{};
    id.status = RomIdStatus::UnknownHash;
    if (!sha256Hex) {
        return id;
    }
    std::snprintf(id.sha256Hex, sizeof(id.sha256Hex), "%s", sha256Hex);
    // No compile-time Apple Disk II hashes until provenance is recorded in
    // host/data/slot6_rom_database.json and mirrored here.
    return id;
}

Slot6RomIdentity Slot6RomDatabase::identify(const uint8_t *data, size_t size) {
    Slot6RomIdentity id{};
    if (!data) {
        id.status = RomIdStatus::IoError;
        return id;
    }
    id.sizeBytes = size;
    if (size == 0) {
        id.status = RomIdStatus::Truncated;
        return id;
    }
    if (size != kExpectedSize) {
        id.status = (size < kExpectedSize) ? RomIdStatus::Truncated : RomIdStatus::InvalidSize;
        Sha256::hashHex(data, size, id.sha256Hex);
        return id;
    }
    Sha256::hashHex(data, size, id.sha256Hex);
    Slot6RomIdentity known = lookupSha256Hex(id.sha256Hex);
    if (known.status == RomIdStatus::Ok) {
        known.sizeBytes = size;
        std::memcpy(known.sha256Hex, id.sha256Hex, sizeof(known.sha256Hex));
        return known;
    }
    id.status = RomIdStatus::UnknownHash;
    id.name = "unrecognized_slot6";
    id.provenance = "hash not in slot6 metadata DB; still loadable if size OK";
    return id;
}

} // namespace esp_bracket
