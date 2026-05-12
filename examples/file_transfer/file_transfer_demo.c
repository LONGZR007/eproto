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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "eproto_app.h"

#define BUFFER_SIZE 4096

// 模拟发送方函数
static void demo_sender(void) {
    printf("========== 发送方演示 ==========\n");

    uint8_t buffer[BUFFER_SIZE];
    size_t len;

    // 1. 发送文件传输启动协议
    const char* filename = "test_file.bin";
    uint32_t file_size = 5000;  // 模拟5KB文件

    len = eproto_app_pack_file_start(buffer, BUFFER_SIZE,
                                     EPROTO_APP_FLAG_NEED_REPLY,
                                     filename, strlen(filename), file_size);
    if (len > 0) {
        printf("1. 发送文件启动协议\n");
        printf("   - 文件名: %s\n", filename);
        printf("   - 文件大小: %u 字节\n", file_size);
        printf("   - 协议长度: %zu 字节\n\n", len);
    }

    // 2. 发送文件数据包（模拟3个包）
    uint8_t file_data[512];
    // 填充一些测试数据
    for (int i = 0; i < 512; i++) {
        file_data[i] = (uint8_t)(i & 0xFF);
    }

    uint32_t packet_index;
    uint16_t data_len;

    // 包0
    packet_index = 0;
    data_len = 512;
    len = eproto_app_pack_file_data(buffer, BUFFER_SIZE, 0,
                                    packet_index, file_data, data_len);
    if (len > 0) {
        printf("2. 发送文件数据包 #%u\n", packet_index);
        printf("   - 数据长度: %u 字节\n", data_len);
        printf("   - 协议长度: %zu 字节\n", len);
    }

    // 包1
    packet_index = 1;
    data_len = 512;
    len = eproto_app_pack_file_data(buffer, BUFFER_SIZE, 0,
                                    packet_index, file_data, data_len);
    if (len > 0) {
        printf("   发送文件数据包 #%u\n", packet_index);
        printf("   - 数据长度: %u 字节\n", data_len);
        printf("   - 协议长度: %zu 字节\n", len);
    }

    // 包2（最后一个包，只有488字节）
    packet_index = 2;
    data_len = 488;
    len = eproto_app_pack_file_data(buffer, BUFFER_SIZE, 0,
                                    packet_index, file_data, data_len);
    if (len > 0) {
        printf("   发送文件数据包 #%u\n", packet_index);
        printf("   - 数据长度: %u 字节\n", data_len);
        printf("   - 协议长度: %zu 字节\n\n", len);
    }

    // 3. 发送文件传输结束协议
    uint32_t total_packets = 3;
    uint32_t file_crc32 = 0;  // 不使用CRC32
    len = eproto_app_pack_file_end(buffer, BUFFER_SIZE,
                                   EPROTO_APP_FLAG_NEED_REPLY,
                                   total_packets, file_crc32);
    if (len > 0) {
        printf("3. 发送文件结束协议\n");
        printf("   - 总包数: %u\n", total_packets);
        printf("   - 协议长度: %zu 字节\n\n", len);
    }
}

// 模拟接收方函数
static void demo_receiver(void) {
    printf("========== 接收方演示 ==========\n");

    uint8_t buffer[BUFFER_SIZE];
    uint8_t recv_buffer[BUFFER_SIZE];
    size_t len;

    eproto_app_header_t header;
    eproto_app_error_t err;

    // ======================================
    // 1. 模拟接收并解析文件启动协议
    // ======================================
    const char* filename = "test_file.bin";
    uint32_t file_size = 5000;
    len = eproto_app_pack_file_start(buffer, BUFFER_SIZE,
                                     EPROTO_APP_FLAG_NEED_REPLY,
                                     filename, strlen(filename), file_size);
    memcpy(recv_buffer, buffer, len);

    printf("1. 接收文件启动协议\n");
    err = eproto_app_parse_header(recv_buffer, len, &header);
    if (err == EPROTO_APP_OK && header.func_code == EPROTO_APP_FUNC_FILE_START) {
        printf("   - 功能码: 0x%02X (文件启动)\n", header.func_code);
        printf("   - 标志位: 0x%02X\n", header.flags);
        printf("   - 数据长度: %u\n", header.data_len);

        uint32_t recv_file_size;
        char recv_filename[256];
        size_t recv_filename_len;
        err = eproto_app_parse_file_start(recv_buffer + sizeof(eproto_app_header_t),
                                          header.data_len,
                                          &recv_file_size,
                                          recv_filename, sizeof(recv_filename),
                                          &recv_filename_len);
        if (err == EPROTO_APP_OK) {
            printf("   - 文件名: %s\n", recv_filename);
            printf("   - 文件大小: %u 字节\n\n", recv_file_size);
        }
    }

    // ======================================
    // 2. 模拟接收并解析文件数据包
    // ======================================
    uint8_t file_data[512];
    for (int i = 0; i < 512; i++) {
        file_data[i] = (uint8_t)(i & 0xFF);
    }

    uint32_t packet_index = 1;
    len = eproto_app_pack_file_data(buffer, BUFFER_SIZE, 0,
                                    packet_index, file_data, 512);
    memcpy(recv_buffer, buffer, len);

    printf("2. 接收文件数据包\n");
    err = eproto_app_parse_header(recv_buffer, len, &header);
    if (err == EPROTO_APP_OK && header.func_code == EPROTO_APP_FUNC_FILE_DATA) {
        printf("   - 功能码: 0x%02X (文件数据)\n", header.func_code);

        uint32_t recv_packet_index;
        const uint8_t* recv_file_data;
        uint16_t recv_data_len;
        err = eproto_app_parse_file_data(recv_buffer + sizeof(eproto_app_header_t),
                                         header.data_len,
                                         &recv_packet_index,
                                         &recv_file_data,
                                         &recv_data_len);
        if (err == EPROTO_APP_OK) {
            printf("   - 包索引: %u\n", recv_packet_index);
            printf("   - 数据长度: %u 字节\n", recv_data_len);
            printf("   - 前10字节数据: ");
            for (int i = 0; i < 10 && i < recv_data_len; i++) {
                printf("%02X ", recv_file_data[i]);
            }
            printf("\n\n");
        }
    }

    // ======================================
    // 3. 模拟接收并解析文件结束协议
    // ======================================
    len = eproto_app_pack_file_end(buffer, BUFFER_SIZE, 0, 3, 0);
    memcpy(recv_buffer, buffer, len);

    printf("3. 接收文件结束协议\n");
    err = eproto_app_parse_header(recv_buffer, len, &header);
    if (err == EPROTO_APP_OK && header.func_code == EPROTO_APP_FUNC_FILE_END) {
        printf("   - 功能码: 0x%02X (文件结束)\n", header.func_code);

        uint32_t recv_total_packets;
        uint32_t recv_crc32;
        err = eproto_app_parse_file_end(recv_buffer + sizeof(eproto_app_header_t),
                                        header.data_len,
                                        &recv_total_packets,
                                        &recv_crc32);
        if (err == EPROTO_APP_OK) {
            printf("   - 总包数: %u\n", recv_total_packets);
            printf("   - CRC32: %08X\n\n", recv_crc32);
        }
    }

    // ======================================
    // 4. 模拟发送确认协议
    // ======================================
    printf("4. 发送文件传输确认\n");
    len = eproto_app_pack_file_ack(buffer, BUFFER_SIZE, 0, 0xFFFFFFFF, 0);
    if (len > 0) {
        printf("   - 确认整个传输\n");
        printf("   - 结果: 成功\n\n");
    }
}

// 演示完整的协议流程
static void demo_protocol_flow(void) {
    printf("========================================\n");
    printf("  eProto 上层应用协议 - 文件传输演示\n");
    printf("========================================\n\n");

    // 演示发送方
    demo_sender();

    printf("\n");

    // 演示接收方
    demo_receiver();

    printf("========================================\n");
    printf("  演示完成\n");
    printf("========================================\n");
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    demo_protocol_flow();

    return 0;
}
