/*
 * MIT License
 *
 * AES Block & Stream Modes:
 *   ECB, CBC (with PKCS#7 padding helpers), CTR, CFB128, OFB.
 *
 * All functions support streaming (caller loops over blocks),
 * which lets them handle arbitrary-length messages.
 */

#ifndef AES_MODES_H
#define AES_MODES_H

#include "aes_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ────────────────────────────── ECB ────────────────────────────── */
/* ECB is just repeated aes_encrypt_block / aes_decrypt_block.
 * Provided here as a convenience that loops correctly.
 * No padding handled — caller must ensure in_len is multiple of 16,
 * or use aes_pkcs7_apply / aes_pkcs7_strip below.
 */
aes_err_t aes_ecb_encrypt(const aes_ctx_t* ctx,
                          const uint8_t* in, size_t in_len,
                          uint8_t* out, size_t* out_len);

aes_err_t aes_ecb_decrypt(const aes_ctx_t* ctx,
                          const uint8_t* in, size_t in_len,
                          uint8_t* out, size_t* out_len);

/* ────────────────────────────── CBC ────────────────────────────── */
/* iv must be exactly 16 bytes.
 * Same note on padding: caller manages via PKCS#7 helpers.
 */
aes_err_t aes_cbc_encrypt(const aes_ctx_t* ctx,
                          const uint8_t* iv,
                          const uint8_t* in, size_t in_len,
                          uint8_t* out, size_t* out_len);

aes_err_t aes_cbc_decrypt(const aes_ctx_t* ctx,
                          const uint8_t* iv,
                          const uint8_t* in, size_t in_len,
                          uint8_t* out, size_t* out_len);

/* Streaming CBC: call init -> update_blocks (loop) -> final.
 * Useful when decrypting an unknown-length stream incrementally.
 */
typedef struct {
    aes_ctx_t* ctx;           /* borrows caller's ctx */
    uint8_t    prev[16];      /* previous ciphertext block (= IV for first block) */
} aes_cbc_state_t;

aes_err_t aes_cbc_init(aes_cbc_state_t* st, aes_ctx_t* ctx, const uint8_t* iv);
void      aes_cbc_reset(aes_cbc_state_t* st, const uint8_t* new_iv);

/* ────────────────────────────── CTR ────────────────────────────── */
/* CTR turns AES into a stream cipher.
 * counter is 16 bytes; conventionally:
 *   counter = [nonce (1-15B) | counter (big-endian, low 1..15B)]
 * This library treats the full 16-byte counter as user-managed
 * and increments it on every block.
 *
 * stream_out receives keystream blocks (caller XORs with plaintext/ciphertext).
 * Or use aes_ctr_crypt() which does the XOR internally for convenience.
 */
typedef struct {
    const aes_ctx_t* ctx;
    uint8_t  counter[16];
    uint8_t  ks_buf[16];      /* Current keystream block. */
    uint8_t  ks_idx;          /* Next unused byte in ks_buf (0..15). */
} aes_ctr_state_t;

aes_err_t aes_ctr_init(aes_ctr_state_t* st, const aes_ctx_t* ctx,
                       const uint8_t* counter);
aes_err_t aes_ctr_crypt(aes_ctr_state_t* st,
                        const uint8_t* in, size_t in_len,
                        uint8_t* out);
void aes_ctr_keystream(aes_ctr_state_t* st, size_t n, uint8_t* out);

/* ────────────────────────────── CFB128 ────────────────────────────── */
/* Cipher Feedback, segment size = 128-bit (full block).
 * Same streaming structure as CTR.
 */
typedef struct {
    const aes_ctx_t* ctx;
    uint8_t  shift_reg[16];    /* Initially the IV. */
    uint8_t  buf[16];          /* Current keystream block. */
    uint8_t  idx;
} aes_cfb_state_t;

aes_err_t aes_cfb_init(aes_cfb_state_t* st, const aes_ctx_t* ctx,
                       const uint8_t* iv);
aes_err_t aes_cfb_encrypt(aes_cfb_state_t* st,
                          const uint8_t* in, size_t in_len,
                          uint8_t* out);
aes_err_t aes_cfb_decrypt(aes_cfb_state_t* st,
                          const uint8_t* in, size_t in_len,
                          uint8_t* out);

/* ────────────────────────────── OFB ────────────────────────────── */
/* Output Feedback.  Identical keystream for encrypt/decrypt.
 */
typedef struct {
    const aes_ctx_t* ctx;
    uint8_t  s[16];           /* Feedback state. */
    uint8_t  buf[16];
    uint8_t  idx;
} aes_ofb_state_t;

aes_err_t aes_ofb_init(aes_ofb_state_t* st, const aes_ctx_t* ctx,
                       const uint8_t* iv);
aes_err_t aes_ofb_crypt(aes_ofb_state_t* st,
                        const uint8_t* in, size_t in_len,
                        uint8_t* out);

/* ────────────────────────────── PKCS#7 Padding ───────────────────── */
/* Apply PKCS#7 padding to plaintext so it becomes a multiple of 16.
 * Output buffer must hold up to in_len + 16 bytes.
 * on return *out_len = actual padded length (in_len + pad_len).
 */
aes_err_t aes_pkcs7_apply(const uint8_t* in, size_t in_len,
                          uint8_t* out, size_t* out_len);

/* Strip PKCS#7 padding from *out in-place.
 * Returns AES_OK and sets *out_len to the unpadded length.
 * Returns AES_ERR_INVALID_ARG if padding is malformed.
 */
aes_err_t aes_pkcs7_strip(uint8_t* data, size_t* data_len);

#ifdef __cplusplus
}
#endif

#endif /* AES_MODES_H */
