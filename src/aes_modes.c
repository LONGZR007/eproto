/*
 * MIT License
 *
 * AES modes: ECB, CBC, CTR, CFB128, OFB + PKCS#7 helpers.
 */

#include "aes_modes.h"
#include <string.h>
#include <stdint.h>

/* ────────────── ECB ────────────── */
aes_err_t aes_ecb_encrypt(const aes_ctx_t* ctx,
                          const uint8_t* in, size_t in_len,
                          uint8_t* out, size_t* out_len) {
    if (!ctx || !in || !out || !out_len) return AES_ERR_INVALID_ARG;
    if (in_len & 0xF) return AES_ERR_INVALID_ARG; /* must be multiple of 16 */

    size_t nb = in_len >> 4;
    for (size_t i = 0; i < nb; i++) {
        aes_encrypt_block(ctx, in + i*16, out + i*16);
    }
    *out_len = in_len;
    return AES_OK;
}

aes_err_t aes_ecb_decrypt(const aes_ctx_t* ctx,
                          const uint8_t* in, size_t in_len,
                          uint8_t* out, size_t* out_len) {
    if (!ctx || !in || !out || !out_len) return AES_ERR_INVALID_ARG;
    if (in_len & 0xF) return AES_ERR_INVALID_ARG;

    size_t nb = in_len >> 4;
    for (size_t i = 0; i < nb; i++) {
        aes_decrypt_block(ctx, in + i*16, out + i*16);
    }
    *out_len = in_len;
    return AES_OK;
}

/* ────────────── CBC ────────────── */
aes_err_t aes_cbc_init(aes_cbc_state_t* st, aes_ctx_t* ctx, const uint8_t* iv) {
    if (!st || !ctx || !iv) return AES_ERR_INVALID_ARG;
    st->ctx = ctx;
    memcpy(st->prev, iv, 16);
    return AES_OK;
}
void aes_cbc_reset(aes_cbc_state_t* st, const uint8_t* new_iv) {
    if (!st || !new_iv) return;
    memcpy(st->prev, new_iv, 16);
}

aes_err_t aes_cbc_encrypt(const aes_ctx_t* ctx,
                          const uint8_t* iv,
                          const uint8_t* in, size_t in_len,
                          uint8_t* out, size_t* out_len) {
    if (!ctx || !iv || !in || !out || !out_len) return AES_ERR_INVALID_ARG;
    if (in_len & 0xF) return AES_ERR_INVALID_ARG;

    aes_cbc_state_t st;
    aes_cbc_init(&st, (aes_ctx_t*)ctx, iv);

    size_t nb = in_len >> 4;
    for (size_t i = 0; i < nb; i++) {
        uint8_t tmp[16];
        for (int j = 0; j < 16; j++) tmp[j] = in[i*16 + j] ^ st.prev[j];
        aes_encrypt_block(ctx, tmp, out + i*16);
        memcpy(st.prev, out + i*16, 16);
    }
    *out_len = in_len;
    return AES_OK;
}

aes_err_t aes_cbc_decrypt(const aes_ctx_t* ctx,
                          const uint8_t* iv,
                          const uint8_t* in, size_t in_len,
                          uint8_t* out, size_t* out_len) {
    if (!ctx || !iv || !in || !out || !out_len) return AES_ERR_INVALID_ARG;
    if (in_len & 0xF) return AES_ERR_INVALID_ARG;

    aes_cbc_state_t st;
    aes_cbc_init(&st, (aes_ctx_t*)ctx, iv);

    size_t nb = in_len >> 4;
    for (size_t i = 0; i < nb; i++) {
        uint8_t tmp[16];
        aes_decrypt_block(ctx, in + i*16, tmp);
        for (int j = 0; j < 16; j++) out[i*16 + j] = tmp[j] ^ st.prev[j];
        memcpy(st.prev, in + i*16, 16);
    }
    *out_len = in_len;
    return AES_OK;
}

/* ────────────── CTR ────────────── */
aes_err_t aes_ctr_init(aes_ctr_state_t* st, const aes_ctx_t* ctx,
                       const uint8_t* counter) {
    if (!st || !ctx || !counter) return AES_ERR_INVALID_ARG;
    st->ctx      = ctx;
    st->ks_idx   = 16;   /* Force keystream block regeneration on first use. */
    memcpy(st->counter, counter, 16);
    return AES_OK;
}

static void ctr_next_block(aes_ctr_state_t* st) {
    aes_encrypt_block(st->ctx, st->counter, st->ks_buf);
    /* Increment the 16-byte big-endian counter. */
    for (int i = 15; i >= 0; i--) {
        st->counter[i]++;
        if (st->counter[i] != 0) break;
    }
    st->ks_idx = 0;
}

void aes_ctr_keystream(aes_ctr_state_t* st, size_t n, uint8_t* out) {
    for (size_t i = 0; i < n; i++) {
        if (st->ks_idx == 16) ctr_next_block(st);
        out[i] = st->ks_buf[st->ks_idx++];
    }
}

aes_err_t aes_ctr_crypt(aes_ctr_state_t* st,
                        const uint8_t* in, size_t in_len,
                        uint8_t* out) {
    if (!st || !in || !out) return AES_ERR_INVALID_ARG;
    for (size_t i = 0; i < in_len; i++) {
        if (st->ks_idx == 16) ctr_next_block(st);
        out[i] = in[i] ^ st->ks_buf[st->ks_idx++];
    }
    return AES_OK;
}

/* ────────────── CFB128 ────────────── */
aes_err_t aes_cfb_init(aes_cfb_state_t* st, const aes_ctx_t* ctx,
                       const uint8_t* iv) {
    if (!st || !ctx || !iv) return AES_ERR_INVALID_ARG;
    st->ctx = ctx;
    st->idx = 16;
    memcpy(st->shift_reg, iv, 16);
    return AES_OK;
}

static void cfb_keystream_block(aes_cfb_state_t* st) {
    aes_encrypt_block(st->ctx, st->shift_reg, st->buf);
    st->idx = 0;
}

aes_err_t aes_cfb_encrypt(aes_cfb_state_t* st,
                          const uint8_t* in, size_t in_len,
                          uint8_t* out) {
    if (!st || !in || !out) return AES_ERR_INVALID_ARG;
    for (size_t i = 0; i < in_len; i++) {
        if (st->idx == 16) cfb_keystream_block(st);
        uint8_t c = (uint8_t)(in[i] ^ st->buf[st->idx]);
        out[i] = c;
        /* Shift register: shift left 1 byte, put ciphertext at right. */
        memmove(st->shift_reg, st->shift_reg + 1, 15);
        st->shift_reg[15] = c;
        (void)st->idx++; /* increment but also make sure buf gets refreshed */
        if (st->idx > 16) st->idx = 16; /* boundary */
        if (i + 1 < in_len && (i + 1) % 16 == 0) st->idx = 16; /* force refresh */
    }
    return AES_OK;
}

aes_err_t aes_cfb_decrypt(aes_cfb_state_t* st,
                          const uint8_t* in, size_t in_len,
                          uint8_t* out) {
    if (!st || !in || !out) return AES_ERR_INVALID_ARG;
    for (size_t i = 0; i < in_len; i++) {
        if (st->idx == 16) cfb_keystream_block(st);
        uint8_t c = in[i];
        out[i] = (uint8_t)(c ^ st->buf[st->idx]);
        /* IMPORTANT: shift register must be updated with CIPHERTEXT, even while decrypting. */
        memmove(st->shift_reg, st->shift_reg + 1, 15);
        st->shift_reg[15] = c;
        st->idx++;
        if (st->idx > 16) st->idx = 16;
        if (i + 1 < in_len && (i + 1) % 16 == 0) st->idx = 16;
    }
    return AES_OK;
}

/* ────────────── OFB ────────────── */
aes_err_t aes_ofb_init(aes_ofb_state_t* st, const aes_ctx_t* ctx,
                       const uint8_t* iv) {
    if (!st || !ctx || !iv) return AES_ERR_INVALID_ARG;
    st->ctx = ctx;
    st->idx = 16;
    memcpy(st->s, iv, 16);
    return AES_OK;
}

static void ofb_next_block(aes_ofb_state_t* st) {
    aes_encrypt_block(st->ctx, st->s, st->s); /* feedback: s_i = E_k(s_{i-1}) */
    memcpy(st->buf, st->s, 16);
    st->idx = 0;
}

aes_err_t aes_ofb_crypt(aes_ofb_state_t* st,
                        const uint8_t* in, size_t in_len,
                        uint8_t* out) {
    if (!st || !in || !out) return AES_ERR_INVALID_ARG;
    for (size_t i = 0; i < in_len; i++) {
        if (st->idx == 16) ofb_next_block(st);
        out[i] = in[i] ^ st->buf[st->idx++];
    }
    return AES_OK;
}

/* ────────────── PKCS#7 Padding ────────────── */
aes_err_t aes_pkcs7_apply(const uint8_t* in, size_t in_len,
                          uint8_t* out, size_t* out_len) {
    if (!in || !out || !out_len) return AES_ERR_INVALID_ARG;
    uint8_t pad = (uint8_t)(16 - (in_len % 16));
    memcpy(out, in, in_len);
    memset(out + in_len, pad, pad);
    *out_len = in_len + pad;
    return AES_OK;
}

aes_err_t aes_pkcs7_strip(uint8_t* data, size_t* data_len) {
    if (!data || !data_len || *data_len == 0) return AES_ERR_INVALID_ARG;
    if ((*data_len) & 0xF) return AES_ERR_INVALID_ARG;
    uint8_t pad = data[*data_len - 1];
    if (pad == 0 || pad > 16) return AES_ERR_INVALID_ARG;
    /* Verify all padding bytes equal pad (constant-time-ish loop). */
    uint8_t ok = 1;
    for (size_t i = 0; i < pad; i++) {
        if (data[*data_len - 1 - i] != pad) { ok = 0; }
    }
    if (!ok) return AES_ERR_INVALID_ARG;
    *data_len -= pad;
    return AES_OK;
}
