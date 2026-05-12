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

#include "eproto_app.h"
#include <string.h>

// 辅助宏：写16位大端序
static inline void write_u16_be(uint8_t* buf, uint16_t val) {
    buf[0] = (uint8_t)(val >> 8);
    buf[1] = (uint8_t)(val & 0xFF);
}

// 辅助宏：写32位大端序
static inline void write_u32_be(uint8_t* buf, uint32_t val) {
    buf[0] = (uint8_t)(val >> 24);
    buf[1] = (uint8_t)((val >> 16) & 0xFF);
    buf[2] = (uint8_t)((val >> 8) & 0xFF);
    buf[3] = (uint8_t)(val & 0xFF);
}

// 辅助宏：读16位大端序
static inline uint16_t read_u16_be(const uint8_t* buf) {
    return ((uint16_t)buf[0] << 8) | (uint16_t)buf[1];
}

// 辅助宏：读32位大端序
static inline uint32_t read_u32_be(const uint8_t* buf) {
    return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] << 8) | (uint32_t)buf[3];
}

// ============================================
// 协议封装函数
// ============================================

size_t eproto_app_pack_frame(uint8_t* buffer, size_t buffer_size,
                             uint8_t func_code, uint8_t flags,
                             const uint8_t* data, uint16_t data_len) {
    size_t header_size = sizeof(eproto_app_header_t);
    size_t total_size = header_size + data_len;

    if (!buffer || buffer_size < total_size) {
        return 0;
    }

    // 填充头部
    buffer[0] = func_code;
    buffer[1] = flags;
    write_u16_be(&buffer[2], data_len);

    // 填充数据
    if (data && data_len > 0) {
        memcpy(&buffer[header_size], data, data_len);
    }

    return total_size;
}

size_t eproto_app_pack_file_start(uint8_t* buffer, size_t buffer_size,
                                  uint8_t flags, const char* filename,
                                  size_t filename_len, uint32_t file_size) {
    if (!buffer || !filename || filename_len == 0 ||
        filename_len > EPROTO_APP_MAX_FILENAME_LEN) {
        return 0;
    }

    size_t data_size = sizeof(uint32_t) + sizeof(uint16_t) + filename_len;
    size_t total_size = sizeof(eproto_app_header_t) + data_size;

    if (buffer_size < total_size) {
        return 0;
    }

    uint8_t* ptr = buffer;

    // 写头部
    *ptr++ = EPROTO_APP_FUNC_FILE_START;
    *ptr++ = flags;
    write_u16_be(ptr, (uint16_t)data_size);
    ptr += 2;

    // 写文件大小
    write_u32_be(ptr, file_size);
    ptr += 4;

    // 写文件名长度
    write_u16_be(ptr, (uint16_t)filename_len);
    ptr += 2;

    // 写文件名
    memcpy(ptr, filename, filename_len);

    return total_size;
}

size_t eproto_app_pack_file_data(uint8_t* buffer, size_t buffer_size,
                                 uint8_t flags, uint32_t packet_index,
                                 const uint8_t* data, uint16_t data_len) {
    if (!buffer || !data || data_len == 0 || data_len > EPROTO_APP_MAX_DATA_LEN) {
        return 0;
    }

    size_t data_size = sizeof(uint32_t) + sizeof(uint16_t) + data_len;
    size_t total_size = sizeof(eproto_app_header_t) + data_size;

    if (buffer_size < total_size) {
        return 0;
    }

    uint8_t* ptr = buffer;

    // 写头部
    *ptr++ = EPROTO_APP_FUNC_FILE_DATA;
    *ptr++ = flags;
    write_u16_be(ptr, (uint16_t)data_size);
    ptr += 2;

    // 写包索引
    write_u32_be(ptr, packet_index);
    ptr += 4;

    // 写数据长度
    write_u16_be(ptr, data_len);
    ptr += 2;

    // 写数据
    memcpy(ptr, data, data_len);

    return total_size;
}

size_t eproto_app_pack_file_end(uint8_t* buffer, size_t buffer_size,
                                uint8_t flags, uint32_t total_packets,
                                uint32_t file_crc32) {
    if (!buffer) {
        return 0;
    }

    size_t data_size = sizeof(uint32_t) + sizeof(uint32_t);
    size_t total_size = sizeof(eproto_app_header_t) + data_size;

    if (buffer_size < total_size) {
        return 0;
    }

    uint8_t* ptr = buffer;

    // 写头部
    *ptr++ = EPROTO_APP_FUNC_FILE_END;
    *ptr++ = flags;
    write_u16_be(ptr, (uint16_t)data_size);
    ptr += 2;

    // 写总包数
    write_u32_be(ptr, total_packets);
    ptr += 4;

    // 写CRC32
    write_u32_be(ptr, file_crc32);

    return total_size;
}

size_t eproto_app_pack_file_ack(uint8_t* buffer, size_t buffer_size,
                                uint8_t flags, uint32_t packet_index,
                                uint8_t result) {
    if (!buffer) {
        return 0;
    }

    size_t data_size = sizeof(uint32_t) + sizeof(uint8_t);
    size_t total_size = sizeof(eproto_app_header_t) + data_size;

    if (buffer_size < total_size) {
        return 0;
    }

    uint8_t* ptr = buffer;

    // 写头部
    *ptr++ = EPROTO_APP_FUNC_FILE_ACK;
    *ptr++ = flags;
    write_u16_be(ptr, (uint16_t)data_size);
    ptr += 2;

    // 写包索引
    write_u32_be(ptr, packet_index);
    ptr += 4;

    // 写结果
    *ptr = result;

    return total_size;
}

// ============================================
// 协议解析函数
// ============================================

eproto_app_error_t eproto_app_parse_header(const uint8_t* data, size_t data_len,
                                           eproto_app_header_t* header) {
    if (!data || !header || data_len < sizeof(eproto_app_header_t)) {
        return EPROTO_APP_ERROR_INVALID_ARG;
    }

    header->func_code = data[0];
    header->flags = data[1];
    header->data_len = read_u16_be(&data[2]);

    // 验证帧长度
    if (data_len < sizeof(eproto_app_header_t) + header->data_len) {
        return EPROTO_APP_ERROR_INVALID_FRAME;
    }

    return EPROTO_APP_OK;
}

eproto_app_error_t eproto_app_parse_file_start(const uint8_t* data, size_t data_len,
                                               uint32_t* file_size,
                                               char* filename, size_t filename_buf_size,
                                               size_t* filename_len) {
    if (!data || !file_size || !filename || !filename_len) {
        return EPROTO_APP_ERROR_INVALID_ARG;
    }

    if (data_len < sizeof(uint32_t) + sizeof(uint16_t)) {
        return EPROTO_APP_ERROR_INVALID_FRAME;
    }

    const uint8_t* ptr = data;

    // 读文件大小
    *file_size = read_u32_be(ptr);
    ptr += 4;

    // 读文件名长度
    uint16_t fname_len = read_u16_be(ptr);
    ptr += 2;

    if (data_len < sizeof(uint32_t) + sizeof(uint16_t) + fname_len) {
        return EPROTO_APP_ERROR_INVALID_FRAME;
    }

    if (fname_len > filename_buf_size - 1) {
        return EPROTO_APP_ERROR_BUFFER_TOO_SMALL;
    }

    // 读文件名
    memcpy(filename, ptr, fname_len);
    filename[fname_len] = '\0';
    *filename_len = fname_len;

    return EPROTO_APP_OK;
}

eproto_app_error_t eproto_app_parse_file_data(const uint8_t* data, size_t data_len,
                                              uint32_t* packet_index,
                                              const uint8_t** file_data,
                                              uint16_t* file_data_len) {
    if (!data || !packet_index || !file_data || !file_data_len) {
        return EPROTO_APP_ERROR_INVALID_ARG;
    }

    if (data_len < sizeof(uint32_t) + sizeof(uint16_t)) {
        return EPROTO_APP_ERROR_INVALID_FRAME;
    }

    const uint8_t* ptr = data;

    // 读包索引
    *packet_index = read_u32_be(ptr);
    ptr += 4;

    // 读数据长度
    *file_data_len = read_u16_be(ptr);
    ptr += 2;

    if (data_len < sizeof(uint32_t) + sizeof(uint16_t) + *file_data_len) {
        return EPROTO_APP_ERROR_INVALID_FRAME;
    }

    *file_data = ptr;

    return EPROTO_APP_OK;
}

eproto_app_error_t eproto_app_parse_file_end(const uint8_t* data, size_t data_len,
                                             uint32_t* total_packets,
                                             uint32_t* file_crc32) {
    if (!data || !total_packets || !file_crc32) {
        return EPROTO_APP_ERROR_INVALID_ARG;
    }

    if (data_len < sizeof(uint32_t) + sizeof(uint32_t)) {
        return EPROTO_APP_ERROR_INVALID_FRAME;
    }

    const uint8_t* ptr = data;

    // 读总包数
    *total_packets = read_u32_be(ptr);
    ptr += 4;

    // 读CRC32
    *file_crc32 = read_u32_be(ptr);

    return EPROTO_APP_OK;
}

eproto_app_error_t eproto_app_parse_file_ack(const uint8_t* data, size_t data_len,
                                             uint32_t* packet_index,
                                             uint8_t* result) {
    if (!data || !packet_index || !result) {
        return EPROTO_APP_ERROR_INVALID_ARG;
    }

    if (data_len < sizeof(uint32_t) + sizeof(uint8_t)) {
        return EPROTO_APP_ERROR_INVALID_FRAME;
    }

    const uint8_t* ptr = data;

    // 读包索引
    *packet_index = read_u32_be(ptr);
    ptr += 4;

    // 读结果
    *result = *ptr;

    return EPROTO_APP_OK;
}
