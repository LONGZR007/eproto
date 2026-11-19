/*
 * Quick GCM debug: verify H, J0, and GHASH for NIST F.3 vector.
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "aes_core.h"

int main(void) {
    uint8_t key[16] = {0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
                       0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f};
    uint8_t iv[12] = {0};

    aes_ctx_t ctx;
    aes_init(&ctx, key, 16);

    /* H = AES_Encrypt(K, 0^128) */
    uint8_t zero[16] = {0};
    uint8_t H[16];
    aes_encrypt_block(&ctx, zero, H);
    printf("H = "); for(int i=0;i<16;i++) printf("%02x", H[i]); printf("\n");

    /* J0 = IV || 0x00000001 */
    uint8_t J0[16];
    memcpy(J0, iv, 12);
    J0[12]=J0[13]=J0[14]=0; J0[15]=1;
    printf("J0 = "); for(int i=0;i<16;i++) printf("%02x", J0[i]); printf("\n");

    /* AES_Encrypt(K, J0) — expected to be tag since GHASH=0 */
    uint8_t J0enc[16];
    aes_encrypt_block(&ctx, J0, J0enc);
    printf("AES_Encrypt(K, J0) = "); for(int i=0;i<16;i++) printf("%02x", J0enc[i]); printf("\n");

    /* NIST expected tag */
    printf("NIST expected tag  = 58e2fccefa7e3061367f1d57a4e7455a\n");

    /* Now let's compute GHASH(H, zeros) to make sure we get zero */
    /* GHASH(H, X): Y = 0; Y = (Y XOR X) * H */
    /* If X is zeros: Y = (0 XOR 0) * H = 0 * H = 0 */
    printf("GHASH of all-zeros = should be 00000000000000000000000000000000\n");

    return 0;
}
