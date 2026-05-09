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

#ifndef EPROTO_FRAME_PARSER_H
#define EPROTO_FRAME_PARSER_H

#include <stdint.h>
#include <stddef.h>
#include "eproto_ring_buffer.h"

// 包类型按bit定义：
// bit0: 1=用户回复包, 0=用户发送包
// bit1: 1=协议确认包
// bit6: 1=握手包
// bit7: 1=重发包
#define EPROTO_PACKET_TYPE_REPLY_FLAG 0x01
#define EPROTO_PACKET_TYPE_ACK_FLAG 0x02
#define EPROTO_PACKET_TYPE_HANDSHAKE_FLAG 0x40
#define EPROTO_PACKET_TYPE_RETRANSMIT_FLAG 0x80

// 常用包类型组合
#define EPROTO_PACKET_TYPE_USER_SEND (0)                              // 用户发送包
#define EPROTO_PACKET_TYPE_USER_REPLY (EPROTO_PACKET_TYPE_REPLY_FLAG) // 用户回复包
#define EPROTO_PACKET_TYPE_PROTOCOL_ACK (EPROTO_PACKET_TYPE_ACK_FLAG)  // 协议确认包

typedef uint8_t eproto_packet_type_t;

typedef struct eproto_frame {
    uint8_t header;
    uint8_t version;
    uint16_t length;
    eproto_packet_type_t packet_type;
    uint8_t src_addr;
    uint8_t dst_addr;
    uint16_t packet_id;
    uint8_t* data;
} eproto_frame_t;

// 协议帧格式定义
// 帧结构：| 帧头(1) | 版本号(1) | 长度(2) | 包类型(1) | 原地址(1) | 设备地址(1)
// | 包ID(2) | 数据(n) | CRC(2) |
// 包类型(1字节)按bit定义：
//   bit0: 1=用户回复包, 0=用户发送包
//   bit1: 1=协议确认包
//   bit6: 1=握手包
//   bit7: 1=重发包

typedef enum {
    EPROTO_FRAME_PARSER_OK = 0,
    EPROTO_FRAME_PARSER_ERROR_NO_HEADER,
    EPROTO_FRAME_PARSER_ERROR_INVALID_LENGTH,
    EPROTO_FRAME_PARSER_ERROR_INSUFFICIENT_DATA,
    EPROTO_FRAME_PARSER_ERROR_MEMORY_ALLOC,
    EPROTO_FRAME_PARSER_ERROR_CRC_CHECK
} eproto_frame_parser_error_t;

typedef struct {
    uint8_t frame_header;
    uint16_t max_frame_length;
} eproto_frame_parser_config_t;

typedef struct {
    uint8_t* frame_data;
    uint16_t frame_length;
} eproto_frame_parser_result_t;

typedef void* (*eproto_malloc_func_t)(size_t size);
typedef void (*eproto_free_func_t)(void* ptr);

typedef struct {
    eproto_frame_parser_config_t config;
    eproto_malloc_func_t malloc_func;
    eproto_free_func_t free_func;
} eproto_frame_parser_t;

void eproto_frame_parser_init(eproto_frame_parser_t* parser, eproto_frame_parser_config_t* config,
                              eproto_malloc_func_t malloc_func, eproto_free_func_t free_func);

eproto_frame_parser_error_t eproto_frame_parser_parse(eproto_ring_buffer_t* rb, eproto_frame_parser_t* parser,
                                                      eproto_frame_t* result);

void eproto_frame_parser_free_result(eproto_frame_parser_t* parser, eproto_frame_t* result);

uint16_t eproto_frame_parser_pack_frame(uint8_t* buffer, uint16_t buffer_size, uint8_t frame_header, uint8_t src_addr,
                                        uint8_t dst_addr, uint16_t packet_id, uint8_t packet_type, uint8_t* data,
                                        uint16_t data_length);

#endif
