#ifndef EPROTO_CRC16_H
#define EPROTO_CRC16_H

#include <stdint.h>

// CRC-16-CCITT 多项式: 0x1021
#define EPROTO_CRC16_CCITT_POLY 0x1021

// CRC-16-CCITT 初始值: 0xFFFF
#define EPROTO_CRC16_CCITT_INIT 0xFFFF

// CRC-16-CCITT 校验函数（使用默认初始值）
uint16_t eproto_crc16_ccitt(const uint8_t* data, uint16_t length);

// CRC-16-CCITT 校验函数（支持外部传入初始值）
uint16_t eproto_crc16_ccitt_ex(const uint8_t* data, uint16_t length, uint16_t init_value);

#endif  // EPROTO_CRC16_H
