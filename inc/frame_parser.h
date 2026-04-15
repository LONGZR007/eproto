#ifndef FRAME_PARSER_H
#define FRAME_PARSER_H

#include <stdint.h>
#include <stddef.h>
#include "ring_buffer.h"

// 协议帧格式定义
// 帧结构：| 帧头(1) | 版本号(1) | 长度(2) | 包类型(1) | 原地址(1) | 设备地址(1)
// | 包ID(2) | 数据(n) | CRC(2) |

// 重发标志定义
#define EPROTO_PACKET_TYPE_RETRANSMIT_FLAG 0x80
#define EPROTO_PACKET_TYPE_HANDSHAKE_FLAG 0x40  // 握手包标志

// 包类型枚举
typedef enum {
    EPROTO_PACKET_TYPE_USER_SEND = 0,  // 用户发送包
    EPROTO_PACKET_TYPE_USER_REPLY,     // 用户回复包
    EPROTO_PACKET_TYPE_PROTOCOL_ACK    // 协议层应答包
} eproto_packet_type_t;

// 帧结构 typedef
typedef struct eproto_frame {
    uint8_t header;                    // 帧头，固定为 EPROTO_FRAME_HEADER
    uint8_t version;                   // 版本号
    uint16_t length;                   // 数据长度
    eproto_packet_type_t packet_type;  // 包类型
    uint8_t source_address;            // 原地址
    uint8_t destination_address;       // 目的地址
    uint16_t packet_id;                // 包ID
    uint8_t* data;                     // 数据
} eproto_frame_t;

// 帧解析器错误码
typedef enum {
    FRAME_PARSER_OK = 0,
    FRAME_PARSER_ERROR_NO_HEADER,
    FRAME_PARSER_ERROR_INVALID_LENGTH,
    FRAME_PARSER_ERROR_INSUFFICIENT_DATA,
    FRAME_PARSER_ERROR_MEMORY_ALLOC,
    FRAME_PARSER_ERROR_CRC_CHECK
} frame_parser_error_t;

// 帧解析器配置
typedef struct {
    uint8_t frame_header;       // 帧头
    uint16_t max_frame_length;  // 最大帧长度
} frame_parser_config_t;

// 帧解析器结果
typedef struct {
    uint8_t* frame_data;    // 帧数据
    uint16_t frame_length;  // 帧长度
} frame_parser_result_t;

// 内存分配函数类型
typedef void* (*malloc_func_t)(size_t size);
typedef void (*free_func_t)(void* ptr);

// 帧解析器结构体
typedef struct {
    frame_parser_config_t config;  // 配置
    malloc_func_t malloc_func;     // 内存分配函数
    free_func_t free_func;         // 内存释放函数
} frame_parser_t;

// 初始化帧解析器
void frame_parser_init(frame_parser_t* parser, frame_parser_config_t* config, malloc_func_t malloc_func,
                       free_func_t free_func);

// 解析帧
frame_parser_error_t frame_parser_parse(ring_buffer_t* rb, frame_parser_t* parser, eproto_frame_t* result);

// 释放解析结果
void frame_parser_free_result(frame_parser_t* parser, eproto_frame_t* result);

// 打包帧
// buffer: 外部提供的缓冲区，用于存储打包后的帧
// buffer_size: 缓冲区大小
// frame_header: 帧头
// source_address: 源地址
// destination_address: 目标地址
// packet_id: 包ID
// packet_type: 包类型
// data: 数据
// data_length: 数据长度
// 返回值: 打包后的帧长度，0表示失败
uint16_t frame_parser_pack_frame(uint8_t* buffer, uint16_t buffer_size, uint8_t frame_header, uint8_t source_address,
                                 uint8_t destination_address, uint16_t packet_id, uint8_t packet_type, uint8_t* data,
                                 uint16_t data_length);

#endif  // FRAME_PARSER_H