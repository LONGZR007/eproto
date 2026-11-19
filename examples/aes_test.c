/*
 * AES library self-test with known-answer test vectors.
 * Build: gcc -O2 -Iinc src/aes_core.c src/aes_modes.c src/aes_gcm.c src/aes_ccm.c -o aes_test test/test_aes.c && ./aes_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "aes_core.h"
#include "aes_modes.h"
#include "aes_gcm.h"
#include "aes_ccm.h"

static int g_passed = 0;
static int g_failed = 0;

static void hex_print(const char* label, const uint8_t* data, size_t len) {
    printf("  %s: ", label);
    for (size_t i = 0; i < len; i++) printf("%02x", data[i]);
    printf("\n");
}

static void assert_eq(const char* name,
                      const uint8_t* got, const uint8_t* expect,
                      size_t len) {
    if (memcmp(got, expect, len) == 0) {
        printf("  ✓ %s\n", name);
        g_passed++;
    } else {
        printf("  ✗ FAIL: %s\n", name);
        hex_print("    got   ", got, len);
        hex_print("    expect", expect, len);
        g_failed++;
    }
}

/* ────────────── FIPS 197 vectors ────────────── */

static void test_aes128_block(void) {
    printf("AES-128 block encrypt/decrypt (FIPS 197):\n");
    uint8_t key[16] = {0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
                       0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f};
    uint8_t pt[16]  = {0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
                       0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff};
    uint8_t expect_ct[16] = {0x69,0xc4,0xe0,0xd8,0x6a,0x7b,0x04,0x30,
                             0xd8,0xcd,0xb7,0x80,0x70,0xb4,0xc5,0x5a};
    uint8_t ct[16], pt2[16];

    aes_ctx_t ctx;
    aes_init(&ctx, key, 16);
    aes_encrypt_block(&ctx, pt, ct);
    assert_eq("encrypt", ct, expect_ct, 16);

    aes_decrypt_block(&ctx, ct, pt2);
    assert_eq("decrypt (round-trip)", pt2, pt, 16);
}

static void test_aes256_block(void) {
    printf("AES-256 block encrypt/decrypt (FIPS 197):\n");
    uint8_t key[32] = {
        0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
        0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
        0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,
        0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f
    };
    uint8_t pt[16] = {0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
                      0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff};
    uint8_t expect_ct[16] = {0x8e,0xa2,0xb7,0xca,0x51,0x67,0x45,0xbf,
                             0xea,0xfc,0x49,0x90,0x4b,0x49,0x60,0x89};
    uint8_t ct[16], pt2[16];

    aes_ctx_t ctx;
    aes_init(&ctx, key, 32);
    aes_encrypt_block(&ctx, pt, ct);
    assert_eq("encrypt", ct, expect_ct, 16);
    aes_decrypt_block(&ctx, ct, pt2);
    assert_eq("decrypt (round-trip)", pt2, pt, 16);
}

static void test_aes192_block(void) {
    /* FIPS 197 Appendix C.3 — AES-192 test vector. */
    printf("AES-192 block encrypt (FIPS 197):\n");
    uint8_t key[24] = {
        0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
        0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
        0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17
    };
    uint8_t pt[16] = {0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
                      0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff};
    uint8_t expect_ct[16] = {0xdd,0xa9,0x7c,0xa4,0x86,0x4c,0xdf,0xe0,
                             0x6e,0xaf,0x70,0xa0,0xec,0x0d,0x71,0x91};
    uint8_t ct[16];
    aes_ctx_t ctx; aes_init(&ctx, key, 24);
    aes_encrypt_block(&ctx, pt, ct);
    assert_eq("encrypt", ct, expect_ct, 16);
}

/* ────────────── AES-CBC round-trip ────────────── */

static void test_cbc(void) {
    printf("AES-CBC round-trip:\n");
    uint8_t key[16] = {
        0x2b,0x7e,0x15,0x16,0x28,0xae,0xd2,0xa6,
        0xab,0xf7,0x15,0x88,0x09,0xcf,0x4f,0x3c
    };
    uint8_t iv[16] = {
        0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
        0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f
    };
    uint8_t pt[32] = "This is exactly 32 bytes of plainte"; /* 31 chars + NUL = 32 */
    /* Make exactly 32 bytes (2 blocks). */
    pt[30] = 'x'; pt[31] = 0;

    uint8_t padded[48];
    size_t padded_len = 0;
    aes_pkcs7_apply(pt, 32, padded, &padded_len);
    assert_eq("PKCS7 pad", padded + 32, (uint8_t*)"\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10", 16);

    aes_ctx_t ctx;
    aes_init(&ctx, key, 16);

    uint8_t ct[48];
    size_t ct_len = 0;
    aes_cbc_encrypt(&ctx, iv, padded, padded_len, ct, &ct_len);

    uint8_t pt_out[48];
    size_t pt_out_len = padded_len;
    aes_cbc_decrypt(&ctx, iv, ct, ct_len, pt_out, &pt_out_len);

    aes_pkcs7_strip(pt_out, &pt_out_len);
    assert_eq("CBC round-trip", pt_out, pt, 32);
}

/* ────────────── AES-CTR round-trip (all-zeros counter) ────────────── */

static void test_ctr(void) {
    printf("AES-CTR round-trip:\n");
    uint8_t key[16] = {
        0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
        0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f
    };
    uint8_t ctr[16] = {0};
    uint8_t pt[20] = "Hello, world! CTR mode.";
    /* AES-128 CTR keystream with all-zeros counter = first 16 bytes of AES-ECB-128 encrypt of 0..0.
     * We won't test against that vector here, just round-trip. */

    aes_ctx_t ctx;
    aes_init(&ctx, key, 16);

    aes_ctr_state_t st;
    aes_ctr_init(&st, &ctx, ctr);
    uint8_t ct[20];
    aes_ctr_crypt(&st, pt, 20, ct);

    aes_ctr_state_t st2;
    aes_ctr_init(&st2, &ctx, ctr);
    uint8_t pt_out[20];
    aes_ctr_crypt(&st2, ct, 20, pt_out);
    assert_eq("CTR round-trip", pt_out, pt, 20);
}

/* ────────────── AES-OFB round-trip ────────────── */

static void test_ofb(void) {
    printf("AES-OFB round-trip:\n");
    uint8_t key[16] = {
        0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,
        0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x10
    };
    uint8_t iv[16] = {0xde,0xad,0xbe,0xef,0xca,0xfe,0xba,0xbe,
                      0x12,0x34,0x56,0x78,0x90,0xab,0xcd,0xef};
    uint8_t pt[13] = "OFB streaming!";

    aes_ctx_t ctx;
    aes_init(&ctx, key, 16);

    aes_ofb_state_t st;
    aes_ofb_init(&st, &ctx, iv);
    uint8_t ct[13];
    aes_ofb_crypt(&st, pt, 13, ct);

    aes_ofb_state_t st2;
    aes_ofb_init(&st2, &ctx, iv);
    uint8_t pt_out[13];
    aes_ofb_crypt(&st2, ct, 13, pt_out);
    assert_eq("OFB round-trip", pt_out, pt, 13);
}

/* ────────────── AES-CFB128 round-trip ────────────── */

static void test_cfb128(void) {
    printf("AES-CFB128 round-trip:\n");
    uint8_t key[16] = {
        0xc2,0x86,0x69,0x6d,0x88,0x07,0x7b,0x25,
        0x60,0x0b,0x28,0x02,0xa3,0x7b,0x39,0x35
    };
    uint8_t iv[16] = {0x56,0x2e,0x17,0x99,0x6d,0x09,0x36,0x76,
                      0xbb,0x7b,0xd8,0x66,0x04,0xe9,0x9a,0xc1};
    uint8_t pt[20] = "CFB128 round trip ok!";

    aes_ctx_t ctx;
    aes_init(&ctx, key, 16);

    aes_cfb_state_t st;
    aes_cfb_init(&st, &ctx, iv);
    uint8_t ct[20];
    aes_cfb_encrypt(&st, pt, 20, ct);

    aes_cfb_state_t st2;
    aes_cfb_init(&st2, &ctx, iv);
    uint8_t pt_out[20];
    aes_cfb_decrypt(&st2, ct, 20, pt_out);
    assert_eq("CFB128 round-trip", pt_out, pt, 20);
}

/* ────────────── AES-GCM NIST test vector (F.3) ────────────── */
/* Key:    000102030405060708090a0b0c0d0e0f
 * IV:     000000000000000000000000
 * AAD:    ""
 * Pt:     ""
 * Tag:    58e2fccefa7e3061367f1d57a4e7455a
 */
static void test_gcm_nist_f3(void) {
    /* NIST SP 800-38D Appendix F.3 — all-zeros key, IV, AAD, plaintext. */
    printf("AES-GCM NIST F.3 (all-zero key, empty pt+AAD):\n");
    uint8_t key[16] = {0};
    uint8_t iv[12]  = {0};
    uint8_t expect_tag[16] = {0x58,0xe2,0xfc,0xce,0xfa,0x7e,0x30,0x61,
                              0x36,0x7f,0x1d,0x57,0xa4,0xe7,0x45,0x5a};

    aes_ctx_t ctx;
    aes_init(&ctx, key, 16);

    uint8_t tag[16];
    aes_gcm_encrypt(&ctx, iv, 12, NULL, 0, NULL, 0, NULL, tag, 16);
    assert_eq("GCM empty encrypt tag", tag, expect_tag, 16);

    aes_err_t e = aes_gcm_decrypt(&ctx, iv, 12, NULL, 0, NULL, 0, NULL, tag, 16);
    if (e == AES_OK) { printf("  ✓ GCM empty decrypt verify\n"); g_passed++; }
    else { printf("  ✗ FAIL: GCM empty decrypt (rc=%d)\n", e); g_failed++; }

    /* Tampered tag must fail. */
    tag[0] ^= 0x01;
    e = aes_gcm_decrypt(&ctx, iv, 12, NULL, 0, NULL, 0, NULL, tag, 16);
    if (e != AES_OK) { printf("  ✓ GCM tampered tag rejected\n"); g_passed++; }
    else { printf("  ✗ FAIL: tampered tag accepted!\n"); g_failed++; }
}

/* ────────────── AES-GCM with AAD and plaintext (cross-checked with OpenSSL/Python) ────────────── */
static void test_gcm_with_data(void) {
    printf("AES-GCM round-trip + known-answer (vs Python cryptography/OpenSSL):\n");
    uint8_t key[16] = {
        0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,
        0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff,0x00
    };
    uint8_t iv[12] = {0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c};
    uint8_t aad[]  = {0xDE,0xAD,0xBE,0xEF,0x12,0x34,0x56,0x78};
    uint8_t pt[]   = {'H','e','l','l','o',' ','A','E','A','D','!'}; /* 11 bytes */

    /* Expected values from Python cryptography (OpenSSL AES-GCM reference):
     *   ct  = 5be7614b3d743505817cdd
     *   tag = 607a05b8a70668c569148f1504e319a1
     */
    uint8_t expect_ct[]  = {0x5b,0xe7,0x61,0x4b,0x3d,0x74,0x35,0x05,0x81,0x7c,0xdd};
    uint8_t expect_tag[] = {0x60,0x7a,0x05,0xb8,0xa7,0x06,0x68,0xc5,
                             0x69,0x14,0x8f,0x15,0x04,0xe3,0x19,0xa1};

    uint8_t ct[11], pt_out[11], tag[16];

    aes_ctx_t ctx;
    aes_init(&ctx, key, 16);

    aes_gcm_encrypt(&ctx, iv, 12, aad, sizeof(aad), pt, sizeof(pt), ct, tag, 16);
    assert_eq("GCM ciphertext (vs OpenSSL)", ct, expect_ct, 11);
    assert_eq("GCM tag       (vs OpenSSL)", tag, expect_tag, 16);

    aes_err_t e = aes_gcm_decrypt(&ctx, iv, 12, aad, sizeof(aad), ct, sizeof(ct), pt_out, tag, 16);
    if (e == AES_OK) { printf("  ✓ GCM decrypt OK\n"); g_passed++; }
    else             { printf("  ✗ FAIL: GCM decrypt (rc=%d)\n", e); g_failed++; }
    assert_eq("GCM plaintext recovery", pt_out, pt, sizeof(pt));

    /* Tampered ciphertext must fail. */
    uint8_t tampered[11]; memcpy(tampered, ct, 11); tampered[5] ^= 0x80;
    e = aes_gcm_decrypt(&ctx, iv, 12, aad, sizeof(aad), tampered, 11, pt_out, tag, 16);
    if (e != AES_OK) { printf("  ✓ GCM tampered ct rejected\n"); g_passed++; }
    else             { printf("  ✗ FAIL: tampered ct accepted!\n"); g_failed++; }

    /* Tampered AAD must fail. */
    uint8_t bad_aad[] = {0xDE,0xAD,0xBE,0xEF,0x12,0x34,0x56,0x79};
    e = aes_gcm_decrypt(&ctx, iv, 12, bad_aad, sizeof(bad_aad), ct, sizeof(ct), pt_out, tag, 16);
    if (e != AES_OK) { printf("  ✓ GCM tampered AAD rejected\n"); g_passed++; }
    else             { printf("  ✗ FAIL: tampered AAD accepted!\n"); g_failed++; }
}

/* ────────────── AES-CCM round-trip ────────────── */
static void test_ccm(void) {
    printf("AES-CCM known-answer + round-trip (vs Python cryptography/OpenSSL):\n");
    uint8_t key[16] = {
        0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,
        0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f
    };
    uint8_t nonce[7] = {0x10,0x11,0x12,0x13,0x14,0x15,0x16};
    uint8_t aad[8]   = {0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07};
    uint8_t pt[10]   = {'h','e','l','l','o','_','c','c','m','!'};

    /* Cross-checked against Python cryptography (OpenSSL):
     *   ct  = 39264f148b2bd05a1c10
     *   tag = 168898cbe8c032fa  (tag_len = 8)
     */
    uint8_t expect_ct[10]  = {0x39,0x26,0x4f,0x14,0x8b,0x2b,0xd0,0x5a,0x1c,0x10};
    uint8_t expect_tag[8]  = {0x16,0x88,0x98,0xcb,0xe8,0xc0,0x32,0xfa};

    uint8_t ct[10], tag[8], pt_out[10];
    aes_ctx_t ctx;
    aes_init(&ctx, key, 16);

    aes_ccm_encrypt(&ctx, nonce, 7, aad, sizeof(aad), pt, sizeof(pt), ct, tag, 8);
    assert_eq("CCM ciphertext (vs OpenSSL)", ct, expect_ct, 10);
    assert_eq("CCM tag       (vs OpenSSL)", tag, expect_tag, 8);

    aes_err_t e = aes_ccm_decrypt(&ctx, nonce, 7, aad, sizeof(aad), ct, sizeof(ct), pt_out, tag, 8);
    if (e == AES_OK) { printf("  ✓ CCM decrypt OK\n"); g_passed++; }
    else             { printf("  ✗ FAIL: CCM decrypt (rc=%d)\n", e); g_failed++; }
    assert_eq("CCM plaintext recovery", pt_out, pt, sizeof(pt));

    /* Tampered ct must fail. */
    uint8_t bad_ct[10]; memcpy(bad_ct, ct, 10); bad_ct[3] ^= 0xFF;
    e = aes_ccm_decrypt(&ctx, nonce, 7, aad, sizeof(aad), bad_ct, 10, pt_out, tag, 8);
    if (e != AES_OK) { printf("  ✓ CCM tampered ct rejected\n"); g_passed++; }
    else             { printf("  ✗ FAIL: tampered ct accepted!\n"); g_failed++; }

    /* Wrong nonce must fail. */
    uint8_t bad_nonce[7] = {0x10,0x11,0x12,0x13,0x14,0x15,0x17};
    e = aes_ccm_decrypt(&ctx, bad_nonce, 7, aad, sizeof(aad), ct, 10, pt_out, tag, 8);
    if (e != AES_OK) { printf("  ✓ CCM wrong nonce rejected\n"); g_passed++; }
    else             { printf("  ✗ FAIL: wrong nonce accepted!\n"); g_failed++; }
}

/* ────────────── AES-GCM with 16-byte IV (non-96-bit) vs OpenSSL ────────────── */
static void test_gcm_non96_iv(void) {
    printf("AES-GCM non-96-bit IV (16 bytes, vs Python cryptography):\n");
    uint8_t key[16] = {0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,
                       0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff,0x00};
    uint8_t iv[16]  = {0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,
                       0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x10};
    uint8_t aad[8]  = {0xDE,0xAD,0xBE,0xEF,0x12,0x34,0x56,0x78};
    uint8_t pt[11]  = {'H','e','l','l','o',' ','A','E','A','D','!'};
    uint8_t expect_ct[11]  = {0x08,0x2d,0x3c,0xc7,0xb6,0xd3,0xe6,0x66,0x98,0x06,0x5e};
    uint8_t expect_tag[16] = {0xc8,0x6d,0xc9,0xe5,0x84,0x1d,0x6b,0x86,
                              0x14,0x5c,0xe1,0x24,0xd9,0x4b,0x75,0x62};
    uint8_t ct[11], tag[16], pt_out[11];

    aes_ctx_t ctx; aes_init(&ctx, key, 16);
    aes_gcm_encrypt(&ctx, iv, 16, aad, sizeof(aad), pt, sizeof(pt), ct, tag, 16);
    assert_eq("GCM ct  (non-96-bit IV)", ct, expect_ct, 11);
    assert_eq("GCM tag (non-96-bit IV)", tag, expect_tag, 16);

    aes_err_t e = aes_gcm_decrypt(&ctx, iv, 16, aad, sizeof(aad), ct, sizeof(ct), pt_out, tag, 16);
    if (e == AES_OK) { printf("  ✓ GCM decrypt OK\n"); g_passed++; }
    else             { printf("  ✗ FAIL: GCM decrypt (rc=%d)\n", e); g_failed++; }
    assert_eq("GCM plaintext recovery", pt_out, pt, sizeof(pt));
}

int main(void) {
    printf("===== AES Library Known-Answer Tests =====\n\n");
    test_aes128_block();
    test_aes192_block();
    test_aes256_block();
    printf("\n");
    test_cbc();
    printf("\n");
    test_ctr();
    printf("\n");
    test_cfb128();
    printf("\n");
    test_ofb();
    printf("\n");
    test_gcm_nist_f3();
    printf("\n");
    test_gcm_with_data();
    printf("\n");
    test_gcm_non96_iv();
    printf("\n");
    test_ccm();
    printf("\n==========================================\n");
    printf("Result: %d passed, %d failed, %d total\n",
           g_passed, g_failed, g_passed + g_failed);
    return g_failed ? 1 : 0;
}
