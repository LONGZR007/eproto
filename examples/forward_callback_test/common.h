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

#ifndef COMMON_H
#define COMMON_H

#include "eproto.h"
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// 设备地址定义
#define DEVICE_A_ADDRESS 0x01
#define DEVICE_B_ADDRESS_1 0x02
#define DEVICE_B_ADDRESS_2 0x03
#define DEVICE_C_ADDRESS 0x04

// 总线地址定义
#define BUS_A_B_ADDRESS 0x02  // Device A 和 Device B 之间的总线
#define BUS_B_C_ADDRESS 0x03  // Device B 和 Device C 之间的总线

// 加密密钥定义
#define KEY_BUS_A_B 0x55  // 总线 A-B 的密钥
#define KEY_BUS_B_C 0xAA  // 总线 B-C 的密钥

// 线程数据结构
typedef struct {
    uint8_t device_address;
    const char* device_name;
    eproto_t eproto_inst;
    uint8_t rx_buffer[256];
    uint8_t rx_buffer2[256];  // 用于 Device B 的第二条总线
    pthread_mutex_t tx_mutex;
    pthread_cond_t tx_cond;
    uint8_t tx_buffer[256];
    uint16_t tx_length;
    uint8_t tx_bus_address;  // 标记数据要发送到哪个总线
} thread_data_t;

// 全局变量
extern thread_data_t* g_device_a_data;
extern thread_data_t* g_device_b_data;
extern thread_data_t* g_device_c_data;

// 公共函数声明
void mock_status_callback(eproto_status_t status, uint8_t* data, uint16_t length);
void device_a_receive_callback(uint8_t source_address, uint16_t packet_id, uint8_t* data, uint16_t length);
void device_b_receive_callback(uint8_t source_address, uint16_t packet_id, uint8_t* data, uint16_t length);
void device_c_receive_callback(uint8_t source_address, uint16_t packet_id, uint8_t* data, uint16_t length);

// 总线发送函数
void device_a_bus_send(uint8_t* data, uint16_t length);
void device_b_bus1_send(uint8_t* data, uint16_t length);
void device_b_bus2_send(uint8_t* data, uint16_t length);
void device_c_bus_send(uint8_t* data, uint16_t length);

// 加解密函数
uint8_t* encrypt_data(uint8_t* data, uint16_t length, uint8_t key);
uint8_t* decrypt_data(uint8_t* data, uint16_t length, uint8_t key);

// 密钥获取函数
uint8_t get_key_for_bus(uint8_t bus_address);

// 转发回调函数
eproto_error_t device_b_forward_callback(uint8_t source_addr, uint8_t dest_addr, 
                                       uint8_t* data, uint16_t length, 
                                       uint8_t** out_data, uint16_t* out_length,
                                       eproto_forward_post_func_t* post_func,
                                       void** private_data);
void device_b_forward_post_func(uint8_t source_addr, uint8_t dest_addr, 
                              uint8_t* out_data, uint16_t out_length,
                              void* private_data);

// 线程函数
void* device_a_thread(void* arg);
void* device_b_thread(void* arg);
void* device_c_thread(void* arg);

// 工具函数
void print_hex(uint8_t* data, uint16_t length, const char* prefix);

// 模拟函数声明
void* mock_malloc(size_t size);
void mock_free(void* ptr);
eproto_signal_result_t mock_signal_wait(uint32_t timestamp);
void mock_signal_send(void);
void mock_lock(void);
void mock_unlock(void);
uint32_t mock_get_timestamp(void);

#endif // COMMON_H