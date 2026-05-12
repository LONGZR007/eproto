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

#ifndef EPROTO_APP_H
#define EPROTO_APP_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================
// 上层应用协议帧结构定义
// ============================================

/*
 * 上层应用协议帧格式：
 * | 功能码(1) | 标志位(1) | 数据长度(2) | 数据(n) |
 *
 * 标志位定义：
 *   bit0: 是否加密 (1=加密, 0=不加密)
 *   bit1: 是否需要回复 (1=需要, 0=不需要)
 *   bit2-7: 预留扩展
 */

// 功能码定义
typedef enum {
    EPROTO_APP_FUNC_FILE_START = 0x01,   // 文件传输启动
    EPROTO_APP_FUNC_FILE_DATA = 0x02,    // 文件数据包
    EPROTO_APP_FUNC_FILE_END = 0x03,     // 文件传输结束
    EPROTO_APP_FUNC_FILE_ACK = 0x04,     // 文件传输确认
    // 预留其他功能码
} eproto_app_func_t;

// 标志位掩码
#define EPROTO_APP_FLAG_ENCRYPTED     0x01  // bit0: 加密标志
#define EPROTO_APP_FLAG_NEED_REPLY    0x02  // bit1: 需要回复标志

// 上层应用协议帧头部结构
#pragma pack(push, 1)
typedef struct {
    uint8_t  func_code;     // 功能码
    uint8_t  flags;         // 标志位
    uint16_t data_len;      // 数据长度
} eproto_app_header_t;

// 文件传输启动协议数据结构
typedef struct {
    uint32_t file_size;     // 文件大小（字节）
    uint16_t filename_len;  // 文件名长度
    uint8_t  filename[];    // 文件名字符串（UTF-8编码）
} eproto_app_file_start_t;

// 文件数据包结构
typedef struct {
    uint32_t packet_index;  // 包索引（从0开始）
    uint16_t data_len;      // 本包数据长度
    uint8_t  data[];        // 文件数据
} eproto_app_file_data_t;

// 文件传输结束协议数据结构
typedef struct {
    uint32_t total_packets; // 总包数
    uint32_t file_crc32;    // 文件CRC32校验（可选，为0表示不使用）
} eproto_app_file_end_t;

// 文件传输确认结构
typedef struct {
    uint32_t packet_index;  // 确认的包索引（或0xFFFFFFFF表示确认整个传输）
    uint8_t  result;        // 结果（0=成功，其他=错误码）
} eproto_app_file_ack_t;
#pragma pack(pop)

// 文件名最大长度
#define EPROTO_APP_MAX_FILENAME_LEN  256

// 单个数据包最大数据长度（根据eProto数据字段最大长度调整）
#define EPROTO_APP_MAX_DATA_LEN      2048

// ============================================
// 错误码定义
// ============================================
typedef enum {
    EPROTO_APP_OK = 0,
    EPROTO_APP_ERROR_INVALID_ARG,
    EPROTO_APP_ERROR_BUFFER_TOO_SMALL,
    EPROTO_APP_ERROR_INVALID_FRAME,
    EPROTO_APP_ERROR_UNKNOWN_FUNC
} eproto_app_error_t;

// ============================================
// 函数声明 - 协议封装
// ============================================

/**
 * 封装上层应用协议帧
 * @param buffer        输出缓冲区
 * @param buffer_size   缓冲区大小
 * @param func_code     功能码
 * @param flags         标志位
 * @param data          数据（可为NULL）
 * @param data_len      数据长度
 * @return              封装后的帧长度，失败返回0
 */
size_t eproto_app_pack_frame(uint8_t* buffer, size_t buffer_size,
                             uint8_t func_code, uint8_t flags,
                             const uint8_t* data, uint16_t data_len);

/**
 * 封装文件传输启动协议
 * @param buffer        输出缓冲区
 * @param buffer_size   缓冲区大小
 * @param flags         标志位
 * @param filename      文件名
 * @param filename_len  文件名长度
 * @param file_size     文件大小
 * @return              封装后的帧长度，失败返回0
 */
size_t eproto_app_pack_file_start(uint8_t* buffer, size_t buffer_size,
                                  uint8_t flags, const char* filename,
                                  size_t filename_len, uint32_t file_size);

/**
 * 封装文件数据包
 * @param buffer        输出缓冲区
 * @param buffer_size   缓冲区大小
 * @param flags         标志位
 * @param packet_index  包索引
 * @param data          文件数据
 * @param data_len      数据长度
 * @return              封装后的帧长度，失败返回0
 */
size_t eproto_app_pack_file_data(uint8_t* buffer, size_t buffer_size,
                                 uint8_t flags, uint32_t packet_index,
                                 const uint8_t* data, uint16_t data_len);

/**
 * 封装文件传输结束协议
 * @param buffer        输出缓冲区
 * @param buffer_size   缓冲区大小
 * @param flags         标志位
 * @param total_packets 总包数
 * @param file_crc32    文件CRC32（0表示不使用）
 * @return              封装后的帧长度，失败返回0
 */
size_t eproto_app_pack_file_end(uint8_t* buffer, size_t buffer_size,
                                uint8_t flags, uint32_t total_packets,
                                uint32_t file_crc32);

/**
 * 封装文件传输确认协议
 * @param buffer        输出缓冲区
 * @param buffer_size   缓冲区大小
 * @param flags         标志位
 * @param packet_index  确认的包索引
 * @param result        结果
 * @return              封装后的帧长度，失败返回0
 */
size_t eproto_app_pack_file_ack(uint8_t* buffer, size_t buffer_size,
                                uint8_t flags, uint32_t packet_index,
                                uint8_t result);

// ============================================
// 函数声明 - 协议解析
// ============================================

/**
 * 解析上层应用协议帧头
 * @param data          输入数据
 * @param data_len      数据长度
 * @param header        输出头部信息
 * @return              成功返回EPROTO_APP_OK，其他表示错误
 */
eproto_app_error_t eproto_app_parse_header(const uint8_t* data, size_t data_len,
                                           eproto_app_header_t* header);

/**
 * 解析文件传输启动协议
 * @param data          输入数据（仅包含数据部分，不含头部）
 * @param data_len      数据长度
 * @param file_size     输出文件大小
 * @param filename      输出文件名缓冲区
 * @param filename_buf_size 文件名缓冲区大小
 * @param filename_len  输出文件名长度
 * @return              成功返回EPROTO_APP_OK，其他表示错误
 */
eproto_app_error_t eproto_app_parse_file_start(const uint8_t* data, size_t data_len,
                                               uint32_t* file_size,
                                               char* filename, size_t filename_buf_size,
                                               size_t* filename_len);

/**
 * 解析文件数据包
 * @param data          输入数据（仅包含数据部分，不含头部）
 * @param data_len      数据长度
 * @param packet_index  输出包索引
 * @param file_data     输出文件数据指针（指向data内部）
 * @param file_data_len 输出文件数据长度
 * @return              成功返回EPROTO_APP_OK，其他表示错误
 */
eproto_app_error_t eproto_app_parse_file_data(const uint8_t* data, size_t data_len,
                                              uint32_t* packet_index,
                                              const uint8_t** file_data,
                                              uint16_t* file_data_len);

/**
 * 解析文件传输结束协议
 * @param data          输入数据（仅包含数据部分，不含头部）
 * @param data_len      数据长度
 * @param total_packets 输出总包数
 * @param file_crc32    输出文件CRC32
 * @return              成功返回EPROTO_APP_OK，其他表示错误
 */
eproto_app_error_t eproto_app_parse_file_end(const uint8_t* data, size_t data_len,
                                             uint32_t* total_packets,
                                             uint32_t* file_crc32);

/**
 * 解析文件传输确认协议
 * @param data          输入数据（仅包含数据部分，不含头部）
 * @param data_len      数据长度
 * @param packet_index  输出包索引
 * @param result        输出结果
 * @return              成功返回EPROTO_APP_OK，其他表示错误
 */
eproto_app_error_t eproto_app_parse_file_ack(const uint8_t* data, size_t data_len,
                                             uint32_t* packet_index,
                                             uint8_t* result);

#ifdef __cplusplus
}
#endif

#endif  // EPROTO_APP_H
