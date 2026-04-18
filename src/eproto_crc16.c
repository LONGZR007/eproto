/*
 * MIT License
 *
 * Copyright (c) 2026 LONGZR007
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

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
