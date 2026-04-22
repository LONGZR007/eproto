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

/* 定义宏以支持 clock_gettime 和 sem_timedwait */
#define _POSIX_C_SOURCE 200809L

#include "eproto.h"
#include "fixed_block_allocator.h"
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <semaphore.h>
#include <time.h>

// 共享缓冲区大小
#define SHARED_BUFFER_SIZE 1024

// 设备地址定义
#define DEVICE_A_ADDRESS 0x01
#define DEVICE_B_ADDRESS_1 0x02
#define DEVICE_B_ADDRESS_2 0x03
#define DEVICE_C_ADDRESS 0x04



// 加密密钥定义
#define KEY_BUS_A_B 0x55  // 总线 A-B 的密钥
#define KEY_BUS_B_C 0xAA  // 总线 B-C 的密钥

// 线程类型枚举
typedef enum {
    THREAD_TYPE_RECEIVE,
    THREAD_TYPE_PROCESS
} thread_type_t;

// 线程数据结构
typedef struct {
    uint8_t device_address;
    eproto_t eproto_inst;
    const char* device_name;
    uint32_t timestamp;
    pthread_mutex_t timestamp_mutex;
    thread_type_t thread_type;
    sem_t semaphore;
    int semaphore_initialized;
    int signal_flag;
    uint8_t rx_buffer[256];
    uint8_t rx_buffer2[256];  // 用于 Device B 的第二条总线
} thread_data_t;

// 线程局部存储
extern __thread thread_data_t* g_current_thread_data;

// 全局变量（共享缓冲区）
// A-B 之间的双向缓冲区
extern uint8_t g_shared_buffer_ab[SHARED_BUFFER_SIZE];  // A 发送到 B 的缓冲区
extern uint16_t g_shared_buffer_ab_head;
extern uint16_t g_shared_buffer_ab_tail;
extern pthread_mutex_t g_mutex_ab;

extern uint8_t g_shared_buffer_ba[SHARED_BUFFER_SIZE];  // B 发送到 A 的缓冲区
extern uint16_t g_shared_buffer_ba_head;
extern uint16_t g_shared_buffer_ba_tail;
extern pthread_mutex_t g_mutex_ba;

// B-C 之间的双向缓冲区
extern uint8_t g_shared_buffer_bc[SHARED_BUFFER_SIZE];  // B 发送到 C 的缓冲区
extern uint16_t g_shared_buffer_bc_head;
extern uint16_t g_shared_buffer_bc_tail;
extern pthread_mutex_t g_mutex_bc;

extern uint8_t g_shared_buffer_cb[SHARED_BUFFER_SIZE];  // C 发送到 B 的缓冲区
extern uint16_t g_shared_buffer_cb_head;
extern uint16_t g_shared_buffer_cb_tail;
extern pthread_mutex_t g_mutex_cb;

extern pthread_mutex_t g_eproto_lock;

extern thread_data_t* g_device_a_data;
extern thread_data_t* g_device_b_data;
extern thread_data_t* g_device_c_data;

// 公共函数声明
void mock_status_callback(eproto_status_t status, uint8_t* data, uint16_t length);
void device_a_receive_callback(uint8_t source_address, uint16_t packet_id, uint8_t* data, uint16_t length);
void device_b_receive_callback(uint8_t source_address, uint16_t packet_id, uint8_t* data, uint16_t length);
void device_c_receive_callback(uint8_t source_address, uint16_t packet_id, uint8_t* data, uint16_t length);

// 总线发送/接收函数
void device_a_bus_send(uint8_t* data, uint16_t length);
uint16_t device_a_bus_receive(uint8_t* buffer, uint16_t size);

void device_b_bus1_send(uint8_t* data, uint16_t length);
uint16_t device_b_bus1_receive(uint8_t* buffer, uint16_t size);

void device_b_bus2_send(uint8_t* data, uint16_t length);
uint16_t device_b_bus2_receive(uint8_t* buffer, uint16_t size);

void device_c_bus_send(uint8_t* data, uint16_t length);
uint16_t device_c_bus_receive(uint8_t* buffer, uint16_t size);

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
