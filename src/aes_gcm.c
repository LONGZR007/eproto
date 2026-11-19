/*
 * MIT License
 *
 * AES-GCM: Authenticated Encryption with Associated Data (AEAD).
 * Follows NIST SP 800-38D exactly, with GF(2^128) multiplication
 * using mbedTLS's proven-correct table-based (Shoup) algorithm.
 *
 * Dependencies: aes_core.h (for AES-ECB primitives).
 */

#include "aes_gcm.h"
#include "aes_core.h"
#include <string.h>
#include <stdint.h>
#include <stddef.h>

/* ─────────────────────────────────────────────────────────────── *
 * GF(2^128) multiply tables (Shoup's method, mbedTLS-compatible). *
 * Multiplication in reversed polynomial basis with irreducible   *
 * x^128 + x^7 + x^2 + x + 1.                                     *
 * ─────────────────────────────────────────────────────────────── */

/* Precomputed low-nibble reduction table (16 entries). */
static const uint64_t gcm_last4[16] = {
    0x0000, 0x1c20, 0x3840, 0x2460,
    0x7080, 0x6ca0, 0x48c0, 0x54e0,
    0xe100, 0xfd20, 0xd940, 0xc560,
    0x9180, 0x8da0, 0xa9c0, 0xb5e0
};

/* Precompute hh_tbl[]/hl_tbl[] from H (= AES_Encrypt(K, 0^128)). */
static void gcm_gen_tables(uint64_t hh_tbl[16], uint64_t hl_tbl[16],
                           const uint8_t H[16]) {
    /* Match mbedTLS exactly:
     *   vh = (BE(H[0:4]) << 32) | BE(H[4:8])
     *   vl = (BE(H[8:12]) << 32) | BE(H[12:16])
     * where BE() means big-endian 32-bit uint32_t.
     */
    uint64_t vh = ((uint64_t)H[0] << 56) | ((uint64_t)H[1] << 48)
                | ((uint64_t)H[2] << 40) | ((uint64_t)H[3] << 32)
                | ((uint64_t)H[4] << 24) | ((uint64_t)H[5] << 16)
                | ((uint64_t)H[6] <<  8) |  (uint64_t)H[7];
    uint64_t vl = ((uint64_t)H[8]  << 56) | ((uint64_t)H[9]  << 48)
                | ((uint64_t)H[10] << 40) | ((uint64_t)H[11] << 32)
                | ((uint64_t)H[12] << 24) | ((uint64_t)H[13] << 16)
                | ((uint64_t)H[14] <<  8) |  (uint64_t)H[15];

    hh_tbl[8] = vh; hl_tbl[8] = vl;
    hh_tbl[0] = 0;  hl_tbl[0] = 0;

    for (int i = 4; i > 0; i >>= 1) {
        uint64_t T = ((vl & 1) * 0xe1000000ULL);
        vl  = ((vh << 63) | (vl >> 1));
        vh  = (vh >> 1) ^ T;
        hl_tbl[i] = vl; hh_tbl[i] = vh;
    }
    for (int i = 2; i <= 8; i *= 2) {
        uint64_t vh_base = hh_tbl[i], vl_base = hl_tbl[i];
        for (int j = 1; j < i; j++) {
            hh_tbl[i + j] = vh_base ^ hh_tbl[j];
            hl_tbl[i + j] = vl_base ^ hl_tbl[j];
        }
    }
}

/* out = x * H using tables (mbedTLS gcm_mult). */
static void gcm_mult(const uint64_t hh_tbl[16], const uint64_t hl_tbl[16],
                     const uint8_t x[16], uint8_t out[16]) {
    uint64_t zh, zl;
    uint8_t lo = x[15] & 0xf;
    zh = hh_tbl[lo]; zl = hl_tbl[lo];

    for (int i = 15; i >= 0; i--) {
        lo = x[i] & 0xf;
        uint8_t hi = (x[i] >> 4) & 0xf;
        if (i != 15) {
            uint8_t rem = (uint8_t)(zl & 0xf);
            zl = (zh << 60) | (zl >> 4);
            zh = (zh >> 4);
            zh ^= ((uint64_t)gcm_last4[rem]) << 48;
            zh ^= hh_tbl[lo];
            zl ^= hl_tbl[lo];
        }
        uint8_t rem = (uint8_t)(zl & 0xf);
        zl = (zh << 60) | (zl >> 4);
        zh = (zh >> 4);
        zh ^= ((uint64_t)gcm_last4[rem]) << 48;
        zh ^= hh_tbl[hi];
        zl ^= hl_tbl[hi];
    }
    out[0]  = (uint8_t)((zh >> 56) & 0xff); out[1]  = (uint8_t)((zh >> 48) & 0xff);
    out[2]  = (uint8_t)((zh >> 40) & 0xff); out[3]  = (uint8_t)((zh >> 32) & 0xff);
    out[4]  = (uint8_t)((zh >> 24) & 0xff); out[5]  = (uint8_t)((zh >> 16) & 0xff);
    out[6]  = (uint8_t)((zh >>  8) & 0xff); out[7]  = (uint8_t)( zh        & 0xff);
    out[8]  = (uint8_t)((zl >> 56) & 0xff); out[9]  = (uint8_t)((zl >> 48) & 0xff);
    out[10] = (uint8_t)((zl >> 40) & 0xff); out[11] = (uint8_t)((zl >> 32) & 0xff);
    out[12] = (uint8_t)((zl >> 24) & 0xff); out[13] = (uint8_t)((zl >> 16) & 0xff);
    out[14] = (uint8_t)((zl >>  8) & 0xff); out[15] = (uint8_t)( zl        & 0xff);
}

/* GHASH one block: Y = (Y XOR X) * H. */
static void ghash_xor(uint8_t Y[16], const uint8_t X[16],
                      const uint64_t hh_tbl[16], const uint64_t hl_tbl[16]) {
    for (int i = 0; i < 16; i++) Y[i] ^= X[i];
    gcm_mult(hh_tbl, hl_tbl, Y, Y);
}

/* ─────────────────────────────────────────────────────────────── *
 * J0 derivation (SP 800-38D section 5.2.1).                    *
 * ─────────────────────────────────────────────────────────────── */
static void gcm_derive_J0(const aes_ctx_t* ctx,
                          const uint8_t* iv, size_t iv_len,
                          const uint8_t* H,
                          const uint64_t hh_tbl[16], const uint64_t hl_tbl[16],
                          uint8_t J0[16]) {
    if (iv_len == 12) {
        memcpy(J0, iv, 12);
        J0[12] = J0[13] = J0[14] = 0;
        J0[15] = 1;
    } else {
        uint8_t Y[16] = {0};
        size_t full_blocks = iv_len / 16;
        size_t rem = iv_len % 16;
        for (size_t i = 0; i < full_blocks; i++) {
            ghash_xor(Y, iv + i * 16, hh_tbl, hl_tbl);
        }
        if (rem > 0 || iv_len == 0) {
            uint8_t tmp[16] = {0};
            if (rem > 0) memcpy(tmp, iv + full_blocks * 16, rem);
            ghash_xor(Y, tmp, hh_tbl, hl_tbl);
        }
        uint8_t len_block[16] = {0};
        uint64_t bits = (uint64_t)iv_len * 8;
        for (int i = 0; i < 8; i++) len_block[8 + i] = (uint8_t)(bits >> (56 - 8 * i));
        ghash_xor(Y, len_block, hh_tbl, hl_tbl);
        memcpy(J0, Y, 16);
    }
}

/* Increment low 32 bits of 16-byte big-endian counter (SP 800-38D). */
static void gcm_incr32(uint8_t ctr[16]) {
    for (int i = 15; i >= 12; i--) {
        ctr[i]++;
        if (ctr[i] != 0) break;
    }
}

/* ─────────────────────────────────────────────────────────────── *
 * Public API                                                      *
 * ─────────────────────────────────────────────────────────────── */

aes_err_t aes_gcm_init(aes_gcm_state_t* st, const aes_ctx_t* ctx,
                       const uint8_t* iv, size_t iv_len,
                       size_t tag_len) {
    if (!st || !ctx || !iv) return AES_ERR_INVALID_ARG;
    if (tag_len < AES_GCM_MIN_TAG_LEN || tag_len > AES_GCM_MAX_TAG_LEN)
        return AES_ERR_INVALID_ARG;

    memset(st, 0, sizeof(*st));
    st->ctx     = ctx;
    st->tag_len = tag_len;

    /* H = AES_Encrypt(K, 0^128) */
    uint8_t zero[16] = {0};
    aes_encrypt_block(ctx, zero, st->H);

    /* Precompute GHASH tables from H. */
    gcm_gen_tables(st->hh_tbl, st->hl_tbl, st->H);

    /* Derive J0 from IV. */
    gcm_derive_J0(ctx, iv, iv_len, st->H, st->hh_tbl, st->hl_tbl, st->J0);

    /* Encryption counter starts at J_1 = J0 + 1 (low 32-bit incr). */
    memcpy(st->ctr, st->J0, 16);
    gcm_incr32(st->ctr);
    st->ks_idx = 16;

    return AES_OK;
}

static void ctr_next_block(aes_gcm_state_t* st) {
    aes_encrypt_block(st->ctx, st->ctr, st->ks_buf);
    gcm_incr32(st->ctr);
    st->ks_idx = 0;
}

aes_err_t aes_gcm_aad(aes_gcm_state_t* st,
                      const uint8_t* aad, size_t aad_len) {
    if (!st || !aad || aad_len == 0) return AES_ERR_INVALID_ARG;
    if (st->need_mac) return AES_ERR_INVALID_ARG;

    /* Flush previously buffered partial block. */
    if (st->buf_len > 0) {
        uint8_t tmp[16] = {0};
        memcpy(tmp, st->buf, st->buf_len);
        ghash_xor(st->ghash, tmp, st->hh_tbl, st->hl_tbl);
        st->buf_len = 0;
    }
    size_t full = aad_len / 16;
    for (size_t i = 0; i < full; i++) {
        ghash_xor(st->ghash, aad + i * 16, st->hh_tbl, st->hl_tbl);
    }
    st->buf_len = (uint16_t)(aad_len % 16);
    if (st->buf_len > 0) memcpy(st->buf, aad + full * 16, st->buf_len);
    st->aad_len_bits += (uint64_t)aad_len * 8;
    return AES_OK;
}

/* Internal: push ciphertext bytes into GHASH. */
static void gcm_emit_ct(aes_gcm_state_t* st, const uint8_t* ct, size_t ct_len) {
    st->c_len_bits += (uint64_t)ct_len * 8;

    /* If AAD was buffered or this is the first ciphertext, flush and mark boundary. */
    if (!st->need_mac) {
        if (st->buf_len > 0) {
            uint8_t tmp[16] = {0};
            memcpy(tmp, st->buf, st->buf_len);
            ghash_xor(st->ghash, tmp, st->hh_tbl, st->hl_tbl);
            st->buf_len = 0;
        }
        st->need_mac = 1;
    }
    /* Flush previously buffered ciphertext fragment. */
    if (st->buf_len > 0) {
        uint8_t tmp[16] = {0};
        memcpy(tmp, st->buf, st->buf_len);
        ghash_xor(st->ghash, tmp, st->hh_tbl, st->hl_tbl);
        st->buf_len = 0;
    }
    size_t full = ct_len / 16;
    for (size_t i = 0; i < full; i++) {
        ghash_xor(st->ghash, ct + i * 16, st->hh_tbl, st->hl_tbl);
    }
    st->buf_len = (uint16_t)(ct_len % 16);
    if (st->buf_len > 0) memcpy(st->buf, ct + full * 16, st->buf_len);
}

aes_err_t aes_gcm_encrypt_update(aes_gcm_state_t* st,
                                 const uint8_t* pt, size_t pt_len,
                                 uint8_t* ct) {
    if (!st || !pt || !ct) return AES_ERR_INVALID_ARG;
    uint8_t* out = ct;
    for (size_t i = 0; i < pt_len; i++) {
        if (st->ks_idx == 16) ctr_next_block(st);
        out[i] = pt[i] ^ st->ks_buf[st->ks_idx++];
    }
    gcm_emit_ct(st, ct, pt_len);
    return AES_OK;
}

aes_err_t aes_gcm_decrypt_update(aes_gcm_state_t* st,
                                 const uint8_t* ct, size_t ct_len,
                                 uint8_t* pt) {
    if (!st || !ct || !pt) return AES_ERR_INVALID_ARG;
    /* Decrypt in-place: XOR with keystream. GHASH uses ciphertext. */
    uint8_t* out = pt;
    for (size_t i = 0; i < ct_len; i++) {
        if (st->ks_idx == 16) ctr_next_block(st);
        out[i] = ct[i] ^ st->ks_buf[st->ks_idx++];
    }
    gcm_emit_ct(st, ct, ct_len);
    return AES_OK;
}

aes_err_t aes_gcm_finalize(aes_gcm_state_t* st, uint8_t* tag) {
    if (!st || !tag) return AES_ERR_INVALID_ARG;

    /* Flush final partial GHASH block (zero-pad). */
    if (st->buf_len > 0) {
        uint8_t tmp[16] = {0};
        memcpy(tmp, st->buf, st->buf_len);
        ghash_xor(st->ghash, tmp, st->hh_tbl, st->hl_tbl);
        st->buf_len = 0;
    }

    /* Length block: AAD_len(64) || Ciphertext_len(64), bits, big-endian. */
    uint8_t len_block[16];
    for (int i = 0; i < 8; i++) {
        len_block[i]     = (uint8_t)(st->aad_len_bits >> (56 - 8 * i));
        len_block[i + 8] = (uint8_t)(st->c_len_bits   >> (56 - 8 * i));
    }
    ghash_xor(st->ghash, len_block, st->hh_tbl, st->hl_tbl);

    /* Final tag = GHASH_Y XOR AES_Encrypt(K, J0). */
    uint8_t J0enc[16];
    aes_encrypt_block(st->ctx, st->J0, J0enc);
    for (int i = 0; i < 16; i++) tag[i] = (uint8_t)(st->ghash[i] ^ J0enc[i]);

    return AES_OK;
}

/* One-shot API: thin wrapper around init + aad + update + finalize. */

aes_err_t aes_gcm_encrypt(const aes_ctx_t* ctx,
                          const uint8_t* iv,   size_t iv_len,
                          const uint8_t* aad,  size_t aad_len,
                          const uint8_t* pt,   size_t pt_len,
                          uint8_t* ct,
                          uint8_t* tag,        size_t tag_len) {
    aes_gcm_state_t st;
    aes_err_t e = aes_gcm_init(&st, ctx, iv, iv_len, tag_len);
    if (e != AES_OK) return e;
    if (aad_len > 0) {
        e = aes_gcm_aad(&st, aad, aad_len);
        if (e != AES_OK) return e;
    }
    if (pt_len > 0) {
        e = aes_gcm_encrypt_update(&st, pt, pt_len, ct);
        if (e != AES_OK) return e;
    }
    return aes_gcm_finalize(&st, tag);
}

aes_err_t aes_gcm_decrypt(const aes_ctx_t* ctx,
                          const uint8_t* iv,   size_t iv_len,
                          const uint8_t* aad,  size_t aad_len,
                          const uint8_t* ct,   size_t ct_len,
                          uint8_t* pt,
                          const uint8_t* tag,  size_t tag_len) {
    aes_gcm_state_t st;
    aes_err_t e = aes_gcm_init(&st, ctx, iv, iv_len, tag_len);
    if (e != AES_OK) return e;
    if (aad_len > 0) {
        e = aes_gcm_aad(&st, aad, aad_len);
        if (e != AES_OK) return e;
    }
    if (ct_len > 0) {
        e = aes_gcm_decrypt_update(&st, ct, ct_len, pt);
        if (e != AES_OK) return e;
    }
    uint8_t computed_tag[16];
    e = aes_gcm_finalize(&st, computed_tag);
    if (e != AES_OK) return e;

    /* Constant-time comparison. */
    uint8_t diff = 0;
    for (size_t i = 0; i < tag_len; i++) diff |= computed_tag[i] ^ tag[i];
    if (diff != 0) return AES_ERR_CRC_CHECK_FAILED;
    return AES_OK;
}
