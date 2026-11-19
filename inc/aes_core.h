/*
 * MIT License
 *
 * AES Core: Key expansion + block encrypt/decrypt (ECB primitive).
 * Supports AES-128 / AES-192 / AES-256.
 * Pure C, zero dependency, constant-time friendly.
 */

#ifndef AES_CORE_H
#define AES_CORE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* AES key sizes (bytes). */
#define AES128_KEY_SIZE 16
#define AES192_KEY_SIZE 24
#define AES256_KEY_SIZE 32

/* AES block size (bytes) — always 16. */
#define AES_BLOCK_SIZE 16

/*
 * Number of round keys = 4 * (Nr + 1), where Nr = rounds.
 *  AES-128: 4 * 11  = 44 words
 *  AES-192: 4 * 13  = 52 words
 *  AES-256: 4 * 15  = 60 words
 * Max round-key buffer in uint32_t words: 60.
 */
#define AES_MAX_RK_WORDS 60

/* Rounds per key length. */
#define AES128_ROUNDS 10
#define AES192_ROUNDS 12
#define AES256_ROUNDS 14

/* Return codes. */
typedef enum {
    AES_OK = 0,
    AES_ERR_INVALID_KEY_SIZE = -1,
    AES_ERR_INVALID_ARG = -2,
    AES_ERR_BUFFER_OVERRUN = -3,
    AES_ERR_CRC_CHECK_FAILED = -4,  /* AEAD tag mismatch / corrupt ciphertext. */
    AES_ERR_MEMORY = -5
} aes_err_t;

/* Opaque-ish context. User should consider it read-only after aes_init(). */
typedef struct {
    uint32_t rk[AES_MAX_RK_WORDS];  /* Round keys (big-endian words). */
    uint8_t  rounds;                /* Nr (10, 12, 14). */
} aes_ctx_t;

/*
 * Initialize aes_ctx_t with a raw AES key.
 *   key_len: 16, 24, or 32 bytes
 * Returns AES_OK on success.
 */
aes_err_t aes_init(aes_ctx_t* ctx, const uint8_t* key, size_t key_len);

/*
 * Encrypt ONE 16-byte block.
 * in  -> 16 bytes of plaintext (const).
 * out -> 16 bytes of ciphertext (may alias in, but NOT recommended for clarity).
 */
void aes_encrypt_block(const aes_ctx_t* ctx, const uint8_t* in, uint8_t* out);

/*
 * Decrypt ONE 16-byte block.
 */
void aes_decrypt_block(const aes_ctx_t* ctx, const uint8_t* in, uint8_t* out);

/* Zeroize sensitive context (e.g. session close). */
void aes_zeroize_ctx(aes_ctx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* AES_CORE_H */
