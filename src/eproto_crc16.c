#include "eproto_crc16.h"

uint16_t eproto_crc16_ccitt_ex(const uint8_t* data, uint16_t length, uint16_t init_value) {
    uint16_t crc = init_value;
    uint8_t i;

    while (length--) {
        crc ^= (*data++) << 8;
        for (i = 0; i < 8; i++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ EPROTO_CRC16_CCITT_POLY;
            else
                crc <<= 1;
        }
    }

    return crc;
}

uint16_t eproto_crc16_ccitt(const uint8_t* data, uint16_t length) {
    return eproto_crc16_ccitt_ex(data, length, EPROTO_CRC16_CCITT_INIT);
}
