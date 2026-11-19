/*
 * MIT License
 *
 * AES-GCM: Authenticated Encryption with Associated Data (AEAD).
 * Follows NIST SP 800-38D exactly.
 *
 * Components:
 *   1. AES-CTR encryption  (using aes_core for block encrypts)
 *   2. GHASH over AAD || Ciphertext
 *   3. Tag = GHASH(H, AAD || C) XOR AES_Encrypt(K, J0)
 *
 * IV (nonce) defaults to 96-bit; if other lengths are provided,
 * we compute J0 via GHASH as required by SP 800-38D.
 *
 * This module is intentionally self-contained; no dependency on aes_modes.
 */

#ifndef AES_GCM_H
#define AES_GCM_H

#include "aes_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Default tag length in bytes. GCM supports 4..16 but 16 is strongest. */
#define AES_GCM_DEFAULT_TAG_LEN 16
#define AES_GCM_MIN_TAG_LEN    4
#define AES_GCM_MAX_TAG_LEN    16

/* Context for incremental GCM. */
typedef struct {
    const aes_ctx_t* ctx;      /* AES key (borrowed). */
    uint8_t H[16];             /* GHASH subkey = AES_Encrypt(K, 0^128). */
    uint8_t J0[16];            /* Pre-counter block. */
    uint8_t ctr[16];           /* Current counter block J_i (J0 + i, starting at i=1). */
    uint8_t ks_buf[16];        /* Current keystream. */
    uint8_t ks_idx;            /* Next byte to consume in ks_buf. */
    uint8_t ghash[16];         /* Running GHASH accumulator. */
    uint8_t buf[16];           /* Buffer for partial GHASH blocks. */
    uint16_t buf_len;          /* Valid bytes in buf (0..15). */
    uint64_t aad_len_bits;     /* Total AAD length in bits. */
    uint64_t c_len_bits;       /* Total ciphertext length in bits. */
    uint8_t need_mac;          /* After aad, we must finalize the AAD section before ciphertext. */
    size_t  tag_len;           /* Desired tag length (4..16). */

    /* mbedTLS-style precomputed tables for GF(2^128) multiply-by-H. */
    uint64_t hh_tbl[16];
    uint64_t hl_tbl[16];
} aes_gcm_state_t;

/* ─────────────────────────── One-shot API ─────────────────────────── */
/*
 * Encrypt + authenticate:
 *   ciphertext out = AES-GCM-Encrypt(key, iv, aad, plaintext, tag_len)
 *   tag out        = 16 bytes or caller-specified
 *
 * iv_len can be anything; 96-bit is most efficient.
 * tag_len: 4..16, default 16.
 */
aes_err_t aes_gcm_encrypt(const aes_ctx_t* ctx,
                          const uint8_t* iv,   size_t iv_len,
                          const uint8_t* aad,  size_t aad_len,
                          const uint8_t* pt,   size_t pt_len,
                          uint8_t* ct,
                          uint8_t* tag,        size_t tag_len);

/* Decrypt + verify:
 * Returns AES_OK + writes plaintext out if tag matches.
 * Returns AES_ERR_INVALID_ARG (or AES_ERR_CRC_CHECK_FAILED if we add it)
 * on tag mismatch; caller MUST discard plaintext in that case.
 */
aes_err_t aes_gcm_decrypt(const aes_ctx_t* ctx,
                          const uint8_t* iv,   size_t iv_len,
                          const uint8_t* aad,  size_t aad_len,
                          const uint8_t* ct,   size_t ct_len,
                          uint8_t* pt,
                          const uint8_t* tag,  size_t tag_len);

/* ─────────────────────────── Incremental API ─────────────────────────── */
/* For streaming applications (ciphertext arrives in chunks). */

aes_err_t aes_gcm_init(aes_gcm_state_t* st, const aes_ctx_t* ctx,
                       const uint8_t* iv, size_t iv_len,
                       size_t tag_len);
aes_err_t aes_gcm_aad(aes_gcm_state_t* st,
                      const uint8_t* aad, size_t aad_len);
aes_err_t aes_gcm_encrypt_update(aes_gcm_state_t* st,
                                 const uint8_t* pt, size_t pt_len,
                                 uint8_t* ct);
aes_err_t aes_gcm_decrypt_update(aes_gcm_state_t* st,
                                 const uint8_t* ct, size_t ct_len,
                                 uint8_t* pt);
aes_err_t aes_gcm_finalize(aes_gcm_state_t* st, uint8_t* tag);

#ifdef __cplusplus
}
#endif

#endif /* AES_GCM_H */
