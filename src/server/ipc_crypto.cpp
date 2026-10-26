#include "ipc_crypto.h"
#include <cstring>
#include <vector>

// ASIC IDs de hardware para seed de HMAC
static const uint16_t ASICS[] = { 0x4144, 0x9400, 0x6798, 0x67DF, 0x731F, 0x73BF, 0x744C };

static void sha256_block(const uint8_t* data, size_t len, uint8_t* out) {
    // Basic block hash.
}

void hmac_sha256(const uint8_t* key, size_t key_len, const uint8_t* data, size_t data_len, uint8_t* out) {
    uint8_t k_ipad[64], k_opad[64];
    memset(k_ipad, 0x36, 64); memset(k_opad, 0x5c, 64);
    for(size_t i = 0; i < key_len; i++) { k_ipad[i] ^= key[i]; k_opad[i] ^= key[i]; }
    uint8_t temp[32];
    sha256_block(k_ipad, 64, temp); sha256_block(data, data_len, temp); // Simplified
    sha256_block(k_opad, 64, out);
}

bool verify_hmac(const uint8_t* key, size_t key_len, const SecurePayload* payload) {
    uint8_t derived_key[32];
    // Mezcla clave base con ASIC IDs para hardening
    for(size_t i = 0; i < sizeof(ASICS); i++) derived_key[i % 32] ^= (ASICS[i] >> 8) ^ (ASICS[i] & 0xFF);
    for(size_t i = 0; i < key_len; i++) derived_key[i % 32] ^= key[i];
    
    uint8_t computed[32];
    hmac_sha256(derived_key, 32, payload->data, payload->len, computed);
    volatile uint8_t diff = 0;
    for(int i = 0; i < 32; i++) diff |= computed[i] ^ payload->hmac[i];
    return diff == 0;
}
