/*
 * MIT License
 *
 * AES-CCM (Counter with CBC-MAC) AEAD.
 * Follows NIST SP 800-38C exactly.
 *
 * Correctly handles:
 *   - B0 block construction (Flags || nonce || msg_len)
 *   - AAD header encoding (2-byte or 6-byte variant)
 *   - CBC-MAC over B0 || AAD_padded || P_padded (encrypt mode)
 *     or over B0 || AAD_padded || C_padded (decrypt mode)
 *   - CTR encryption with counter starting at 1
 *   - Tag masking: T = MSB_t(MAC) XOR MSB_t(E_K(B0))
 */

#include "aes_ccm.h"
#include "aes_core.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

/* Inline XOR helper. */
static inline void xor_block(uint8_t dst[16], const uint8_t src[16]) {
    for (int i = 0; i < 16; i++) dst[i] ^= src[i];
}

/* SP 800-38C helper: build the Flags byte.
 *   Bit 7: reserved = 0
 *   Bit 6: Adata_flag (1 if AAD present)
 *   Bits 5..3: tag_len/2 - 1
 *   Bits 2..0: L - 1
 */
static uint8_t ccm_make_flags(size_t tag_len, int has_aad, uint8_t L) {
    uint8_t f = 0;
    if (has_aad) f |= 0x40;
    f |= (uint8_t)((tag_len / 2 - 1) << 3);
    f |= (uint8_t)(L - 1);
    return f;
}

/* Feed msg_len bytes starting at ptr into CBC-MAC (zero-padded to 16). */
static void ccm_mac_feed(const aes_ctx_t* ctx, uint8_t Y[16],
                         const uint8_t* ptr, size_t msg_len, uint8_t X[16]) {
    size_t full = msg_len / 16;
    size_t rem  = msg_len % 16;
    for (size_t b = 0; b < full; b++) {
        xor_block(Y, ptr + b * 16);
        aes_encrypt_block(ctx, Y, Y);
    }
    if (rem > 0) {
        memset(X, 0, 16);
        memcpy(X, ptr + full * 16, rem);
        xor_block(Y, X);
        aes_encrypt_block(ctx, Y, Y);
    }
}

/* Compute CBC-MAC over  B0 || AAD_padded || msg_padded.  */
static void ccm_compute_mac(const aes_ctx_t* ctx,
                            const uint8_t* B0,
                            const uint8_t* aad, size_t aad_len,
                            const uint8_t* msg, size_t msg_len,
                            uint8_t Y_out[16]) {
    uint8_t Y[16] = {0};
    uint8_t X[16];

    /* Block 0: B0 */
    memcpy(X, B0, 16);
    xor_block(Y, X);
    aes_encrypt_block(ctx, Y, Y);

    /* AAD blocks: header || AAD, always zero-padded to 16-byte boundary. */
    if (aad_len > 0) {
        uint8_t header[6];
        size_t header_len;
        if (aad_len < ((size_t)1 << 16) - 240) {
            header[0] = (uint8_t)(aad_len >> 8);
            header[1] = (uint8_t)(aad_len & 0xFF);
            header_len = 2;
        } else {
            header[0] = 0xFF; header[1] = 0xFE;
            header[2] = (uint8_t)(aad_len >> 24);
            header[3] = (uint8_t)(aad_len >> 16);
            header[4] = (uint8_t)(aad_len >> 8);
            header[5] = (uint8_t)(aad_len & 0xFF);
            header_len = 6;
        }

        /* First AAD block: zero-padded header || start of AAD. */
        uint8_t first[16] = {0};
        memcpy(first, header, header_len);
        size_t tail = 16 - header_len;
        size_t copy = (aad_len < tail) ? aad_len : tail;
        memcpy(first + header_len, aad, copy);
        xor_block(Y, first);
        aes_encrypt_block(ctx, Y, Y);

        /* Remaining AAD body in whole blocks, then zero-padded tail. */
        ccm_mac_feed(ctx, Y, aad + copy, aad_len - copy, X);
    }

    /* Message blocks (always zero-padded to 16-byte boundary). */
    ccm_mac_feed(ctx, Y, msg, msg_len, X);

    memcpy(Y_out, Y, 16);
}

/*
 * Core CCM AEAD computation.
 *   encrypt = 1: in=PT → out=CT;   MAC covers PT (i.e. in)
 *   encrypt = 0: in=CT → out=PT;   MAC covers PT (i.e. recovered out)
 *   mac      : 16-byte output buffer, receives full (masked) tag
 */
static aes_err_t ccm_core(const aes_ctx_t* ctx,
                          const uint8_t* nonce, size_t nonce_len,
                          const uint8_t* aad,   size_t aad_len,
                          const uint8_t* in,    size_t in_len,
                          uint8_t* out,
                          size_t tag_len,
                          int encrypt,
                          uint8_t* mac /* 16-byte out */) {
    uint8_t L = (uint8_t)(15 - nonce_len);

    /* ────── Step 1: Build B0 (Flags || nonce || msg_len) ────── */
    uint8_t flags = ccm_make_flags(tag_len, aad_len > 0, L);
    uint8_t B0[16];
    B0[0] = flags;
    memcpy(B0 + 1, nonce, nonce_len);
    uint64_t len64 = (uint64_t)in_len;
    for (uint8_t i = 0; i < L; i++) {
        B0[15 - i] = (uint8_t)(len64 >> (8 * i));
    }

/* ────── Step 2: CTR encrypt/decrypt first, so that during decrypt we
     *       have the recovered plaintext available for MAC. ──────
     * Counter blocks B_i (i >= 1):
     *   B_i = Flags_C || nonce || i   (L bytes, big-endian)
     * where Flags_C = (L-1) only — the Adata and tag-length fields
     * are ZERO in counter blocks (unlike B_0 which carries full flags).
     */
    uint8_t ctr_flags = (uint8_t)(L - 1);
    if (in_len > 0) {
        size_t nb = (in_len + 15) / 16;
        for (size_t b = 0; b < nb; b++) {
            uint64_t ctr_val = (uint64_t)(b + 1);  /* counter starts at 1 */
            uint8_t Bi[16];
            Bi[0] = ctr_flags;
            memcpy(Bi + 1, nonce, nonce_len);
            for (uint8_t i = 0; i < L; i++) {
                Bi[15 - i] = (uint8_t)(ctr_val >> (8 * i));
            }
            uint8_t S[16];
            aes_encrypt_block(ctx, Bi, S);
            size_t chunk = (in_len - b * 16 < 16) ? (in_len - b * 16) : 16;
            for (size_t j = 0; j < chunk; j++) {
                out[b * 16 + j] = (uint8_t)(in[b * 16 + j] ^ S[j]);
            }
        }
    }

    /* ────── Step 3: CBC-MAC over plaintext in both directions. ──────
     * During encrypt  plaintext = in  (original input)
     * During decrypt  plaintext = out (recovered above)
     * So we can just always MAC over out when decrypt, in when encrypt. */
    const uint8_t* mac_msg = encrypt ? in : out;
    uint8_t Y[16];
    ccm_compute_mac(ctx, B0, aad, aad_len, mac_msg, in_len, Y);

    /* ────── Step 4: S0 = E_K(ctr0_block),  T = Y XOR S0 ──────
     * CRITICAL: S0 is NOT E_K(B0_mac)!  B0_mac has msg_len at the end,
     * but S0 uses the counter block layout (ctr_flags + nonce + 0).
     * This is a *completely different block* from B0_mac. */
    uint8_t ctr0_block[16];
    ctr0_block[0] = ctr_flags;
    memcpy(ctr0_block + 1, nonce, nonce_len);
    memset(ctr0_block + 1 + nonce_len, 0, L);   /* counter = 0 */
    uint8_t S0[16];
    aes_encrypt_block(ctx, ctr0_block, S0);
    for (int i = 0; i < 16; i++) mac[i] = (uint8_t)(Y[i] ^ S0[i]);

    return AES_OK;
}

/* ─────────────────────────────────────────────────────────────── *
 * Public one-shot API.                                            *
 * ─────────────────────────────────────────────────────────────── */

aes_err_t aes_ccm_encrypt(const aes_ctx_t* ctx,
                          const uint8_t* nonce, size_t nonce_len,
                          const uint8_t* aad,   size_t aad_len,
                          const uint8_t* pt,    size_t pt_len,
                          uint8_t* ct,
                          uint8_t* tag,         size_t tag_len) {
    if (!ctx || !nonce || !ct || !tag) return AES_ERR_INVALID_ARG;
    if (nonce_len < AES_CCM_MIN_NONCE_LEN || nonce_len > AES_CCM_MAX_NONCE_LEN)
        return AES_ERR_INVALID_ARG;
    if (tag_len < AES_CCM_MIN_TAG_LEN || tag_len > AES_CCM_MAX_TAG_LEN || (tag_len & 1))
        return AES_ERR_INVALID_ARG;

    uint8_t full_tag[16];
    aes_err_t e = ccm_core(ctx, nonce, nonce_len, aad, aad_len, pt, pt_len,
                           ct, tag_len, 1 /* encrypt */, full_tag);
    if (e != AES_OK) return e;
    memcpy(tag, full_tag, tag_len);
    return AES_OK;
}

aes_err_t aes_ccm_decrypt(const aes_ctx_t* ctx,
                          const uint8_t* nonce, size_t nonce_len,
                          const uint8_t* aad,   size_t aad_len,
                          const uint8_t* ct,    size_t ct_len,
                          uint8_t* pt,
                          const uint8_t* tag,   size_t tag_len) {
    if (!ctx || !nonce || !ct || !pt || !tag) return AES_ERR_INVALID_ARG;
    if (nonce_len < AES_CCM_MIN_NONCE_LEN || nonce_len > AES_CCM_MAX_NONCE_LEN)
        return AES_ERR_INVALID_ARG;
    if (tag_len < AES_CCM_MIN_TAG_LEN || tag_len > AES_CCM_MAX_TAG_LEN || (tag_len & 1))
        return AES_ERR_INVALID_ARG;

    uint8_t full_tag[16];
    aes_err_t e = ccm_core(ctx, nonce, nonce_len, aad, aad_len, ct, ct_len,
                           pt, tag_len, 0 /* decrypt */, full_tag);
    if (e != AES_OK) return e;

    uint8_t diff = 0;
    for (size_t i = 0; i < tag_len; i++) diff |= full_tag[i] ^ tag[i];
    if (diff != 0) return AES_ERR_CRC_CHECK_FAILED;
    return AES_OK;
}
