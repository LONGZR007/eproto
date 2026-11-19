/*
 * MIT License
 *
 * AES-CCM (Counter with CBC-MAC) AEAD.
 * Follows NIST SP 800-38C exactly.
 *
 * CCM combines CBC-MAC authentication with CTR-mode encryption in one pass.
 * It is mandatory in IEEE 802.15.4 (ZigBee / Thread / BLE) security.
 *
 * This is a self-contained, correct (no timing tricks though) implementation.
 */

#ifndef AES_CCM_H
#define AES_CCM_H

#include "aes_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Tag length options (even values 4..16). */
#define AES_CCM_MIN_TAG_LEN 4
#define AES_CCM_MAX_TAG_LEN 16

/* Nonce length must satisfy: 7 <= nonce_len <= 13 (SP 800-38C constraints).
 * And L = 15 - nonce_len, used for encoding message length field.
 */
#define AES_CCM_MIN_NONCE_LEN 7
#define AES_CCM_MAX_NONCE_LEN 13

/* ─────────────────────────── One-shot API ─────────────────────────── */
/*
 * Encrypt + authenticate with AES-CCM.
 *
 *   nonce_len: 7..13 bytes
 *   tag_len : 4..16 (even number of bytes)
 *   aad_len + pt_len must each be <= (1 << (8*L)) - 1 where L = 15 - nonce_len.
 *
 * Returns AES_OK + writes ciphertext into ct[] and tag into tag[].
 */
aes_err_t aes_ccm_encrypt(const aes_ctx_t* ctx,
                          const uint8_t* nonce, size_t nonce_len,
                          const uint8_t* aad,   size_t aad_len,
                          const uint8_t* pt,    size_t pt_len,
                          uint8_t* ct,
                          uint8_t* tag,         size_t tag_len);

/* Decrypt + verify. On tag mismatch returns AES_ERR_CRC_CHECK_FAILED. */
aes_err_t aes_ccm_decrypt(const aes_ctx_t* ctx,
                          const uint8_t* nonce, size_t nonce_len,
                          const uint8_t* aad,   size_t aad_len,
                          const uint8_t* ct,    size_t ct_len,
                          uint8_t* pt,
                          const uint8_t* tag,   size_t tag_len);

#ifdef __cplusplus
}
#endif

#endif /* AES_CCM_H */
