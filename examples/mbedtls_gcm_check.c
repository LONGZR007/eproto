#include <stdio.h>
#include <string.h>
#include <mbedtls/gcm.h>
#include <mbedtls/aes.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/platform.h>

int main(void) {
    uint8_t key[16] = {0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,
                       0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff,0x00};
    uint8_t iv[12]  = {0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,
                       0x09,0x0a,0x0b,0x0c};
    uint8_t aad[]   = {0xDE,0xAD,0xBE,0xEF,0x12,0x34,0x56,0x78};
    uint8_t pt[]    = "Hello AEAD!";  // 11 bytes + NUL

    uint8_t ct[20];
    uint8_t tag[16];
    size_t  olen;

    mbedtls_gcm_context gctx;
    mbedtls_gcm_init(&gctx);

    mbedtls_aes_setkey_enc(&gctx.aes, key, 128);
    mbedtls_gcm_starts(&gctx, MBEDTLS_GCM_ENCRYPT, iv, 12, aad, sizeof(aad));
    mbedtls_gcm_update(&gctx, sizeof(pt), pt, 20, ct, &olen);
    mbedtls_gcm_finish(&gctx, tag, 16);

    printf("ct  : "); for (size_t i = 0; i < olen; i++) printf("%02x", ct[i]); printf("\n");
    printf("tag : "); for (int i = 0; i < 16; i++) printf("%02x", tag[i]); printf("\n");
    printf("expected ct : 5be7614b3d743505817cdd\n");
    printf("expected tag: 607a05b8a70668c569148f1504e319a1\n");

    mbedtls_gcm_free(&gctx);
    return 0;
}
