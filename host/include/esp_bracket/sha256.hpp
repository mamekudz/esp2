#pragma once

#include <cstddef>
#include <cstdint>

namespace esp_bracket {

/** SHA-256 (public-domain style compact implementation). */
class Sha256 {
  public:
    Sha256();
    void update(const uint8_t *data, size_t len);
    void final(uint8_t out[32]);

    /** Hex lowercase, needs 65 bytes (64 + NUL). */
    static void toHex(const uint8_t digest[32], char outHex65[65]);

    static void hash(const uint8_t *data, size_t len, uint8_t out[32]);
    static void hashHex(const uint8_t *data, size_t len, char outHex65[65]);

  private:
    void transform(const uint8_t block[64]);
    uint32_t state_[8]{};
    uint64_t bitLen_ = 0;
    uint8_t buffer_[64]{};
    size_t bufferLen_ = 0;
};

} // namespace esp_bracket
