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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include <semaphore.h>

// 共享缓冲区用于模拟总线通信
#define SHARED_BUFFER_SIZE 1024

// 上层协议头部定义
#define PROTOCOL_HEADER_SIZE 2
#define PROTOCOL_FLAG_ENCRYPTED   0x80
#define PROTOCOL_FLAG_NEED_REPLY  0x01

// 加密密钥定义
#define KEY_BUS_1_2 0x5A  // 总线1-2之间的密钥
#define KEY_BUS_3_4 0xA5  // 总线3-4之间的密钥

// 线程类型枚举
typedef enum {
    THREAD_TYPE_RECEIVE,  // 接收线程
    THREAD_TYPE_PROCESS   // 处理线程
} thread_type_t;

// 线程数据结构体
typedef struct {
    uint8_t device_address;
    eproto_t eproto_inst;
    char* device_name;
    uint32_t timestamp;
    pthread_mutex_t timestamp_mutex;
    thread_type_t thread_type;  // 线程类型
    sem_t semaphore;            // 信号量
    int semaphore_initialized;  // 信号量初始化状态
    int signal_flag;            // 信号标志，用于模拟裸机情况
    uint8_t rx_buffer[256];     // 接收缓冲区1
    uint8_t rx_buffer2[256];    // 接收缓冲区2（用于第二条总线）
    uint8_t need_reply_flag;    // 当前发送数据是否需要回复的标志
} thread_data_t;

// 全局变量声明
extern uint8_t g_shared_buffer1[SHARED_BUFFER_SIZE];  // 设备1 -> 设备2
extern uint8_t g_shared_buffer2[SHARED_BUFFER_SIZE];  // 设备2 -> 设备1
extern uint8_t g_shared_buffer3[SHARED_BUFFER_SIZE];  // 设备2 -> 设备3
extern uint8_t g_shared_buffer4[SHARED_BUFFER_SIZE];  // 设备3 -> 设备2
extern uint8_t g_shared_buffer5[SHARED_BUFFER_SIZE];  // 设备2 -> 设备4
extern uint8_t g_shared_buffer6[SHARED_BUFFER_SIZE];  // 设备4 -> 设备2
extern uint16_t g_shared_buffer1_head;
extern uint16_t g_shared_buffer1_tail;
extern uint16_t g_shared_buffer2_head;
extern uint16_t g_shared_buffer2_tail;
extern uint16_t g_shared_buffer3_head;
extern uint16_t g_shared_buffer3_tail;
extern uint16_t g_shared_buffer4_head;
extern uint16_t g_shared_buffer4_tail;
extern uint16_t g_shared_buffer5_head;
extern uint16_t g_shared_buffer5_tail;
extern uint16_t g_shared_buffer6_head;
extern uint16_t g_shared_buffer6_tail;
extern pthread_mutex_t g_mutex1;
extern pthread_mutex_t g_mutex2;
extern pthread_mutex_t g_mutex3;
extern pthread_mutex_t g_mutex4;
extern pthread_mutex_t g_mutex5;
extern pthread_mutex_t g_mutex6;

// 全局线程数据指针，用于信号函数访问
// 使用线程局部存储来存储当前线程数据，避免线程安全问题
extern __thread thread_data_t* g_current_thread_data;

// 设备1的总线发送函数（写入共享缓冲区1）
void device1_bus_send(eproto_bus_t* bus, uint8_t* data, uint16_t length);

// 设备1的总线接收函数（从共享缓冲区2读取）
uint16_t device1_bus_receive(uint8_t* buffer, uint16_t size);

// 设备2的总线发送函数（写入共享缓冲区2）
void device2_bus_send(eproto_bus_t* bus, uint8_t* data, uint16_t length);

// 设备2的总线接收函数（从共享缓冲区1读取）
uint16_t device2_bus_receive(uint8_t* buffer, uint16_t size);

// 设备2的第二条总线发送函数（写入共享缓冲区3）
void device2_bus2_send(eproto_bus_t* bus, uint8_t* data, uint16_t length);

// 设备2的第二条总线接收函数（从共享缓冲区4读取）
uint16_t device2_bus2_receive(uint8_t* buffer, uint16_t size);

// 设备3的总线发送函数（写入共享缓冲区4）
void device3_bus_send(eproto_bus_t* bus, uint8_t* data, uint16_t length);

// 设备3的总线接收函数（从共享缓冲区3读取）
uint16_t device3_bus_receive(uint8_t* buffer, uint16_t size);

// 设备4的总线发送函数（写入共享缓冲区5）
void device4_bus_send(eproto_bus_t* bus, uint8_t* data, uint16_t length);

// 设备4的总线接收函数（从共享缓冲区6读取）
uint16_t device4_bus_receive(uint8_t* buffer, uint16_t size);

// 模拟内存分配函数
void* mock_malloc(size_t size);

// 模拟内存释放函数
void mock_free(void* ptr);

// 模拟时间戳函数
uint32_t mock_get_timestamp(void);

// 模拟锁函数
void mock_lock(void);

// 模拟解锁函数
void mock_unlock(void);

// 模拟唤醒函数
void mock_wakeup(void);

// 模拟状态回调函数
void mock_status_callback(eproto_bus_t* bus, eproto_status_t status, uint8_t* data, uint16_t length);

// 协议包装函数：在数据前添加2字节协议头
// encrypted: 加密标志（保留不使用）
// need_reply: 是否需要回复
// 返回包装后的数据（调用者负责释放内存）
uint8_t* protocol_wrap(uint8_t encrypted, uint8_t need_reply, const uint8_t* data, uint16_t length, uint16_t* out_length);

// 协议解包函数：从数据中解析协议头
// 返回有效数据的起始位置和长度
uint8_t* protocol_unwrap(const uint8_t* data, uint16_t length, uint8_t* out_encrypted, uint8_t* out_need_reply, uint16_t* out_data_length);

// 设备1接收回调函数
void device1_receive_callback(eproto_bus_t* bus, uint8_t source_address, uint16_t packet_id, uint8_t* data, uint16_t length);

// 设备1发送回调函数
void device1_send_callback(eproto_send_status_t status, uint16_t packet_id, uint8_t* data, uint16_t length,
                           void* private_data);

// 设备2接收回调函数
void device2_receive_callback(eproto_bus_t* bus, uint8_t source_address, uint16_t packet_id, uint8_t* data, uint16_t length);

// 设备2发送回调函数
void device2_send_callback(eproto_send_status_t status, uint16_t packet_id, uint8_t* data, uint16_t length,
                           void* private_data);

// 设备3接收回调函数
void device3_receive_callback(eproto_bus_t* bus, uint8_t source_address, uint16_t packet_id, uint8_t* data, uint16_t length);

// 设备3发送回调函数
void device3_send_callback(eproto_send_status_t status, uint16_t packet_id, uint8_t* data, uint16_t length,
                           void* private_data);

// 设备1信号等待函数
eproto_signal_result_t device1_signal_wait(uint32_t timestamp);

// 设备1信号发送函数
void device1_signal_send(void);

// 设备2信号等待函数（模拟裸机环境）
eproto_signal_result_t device2_signal_wait(uint32_t timestamp);

// 设备2信号发送函数（模拟裸机环境）
void device2_signal_send(void);

// 设备3信号等待函数
eproto_signal_result_t device3_signal_wait(uint32_t timestamp);

// 设备3信号发送函数
void device3_signal_send(void);

// 设备4信号等待函数
eproto_signal_result_t device4_signal_wait(uint32_t timestamp);

// 设备4信号发送函数
void device4_signal_send(void);

// 设备1接收线程
void* device1_receive_thread(void* arg);

// 设备1处理线程
void* device1_process_thread(void* arg);

// 设备1线程
void* device1_thread(void* arg);

// 设备2接收线程
void* device2_receive_thread(void* arg);

// 设备2处理线程
void* device2_process_thread(void* arg);

// 设备2线程
void* device2_thread(void* arg);

// 设备3接收线程
void* device3_receive_thread(void* arg);

// 设备3处理线程
void* device3_process_thread(void* arg);

// 设备3线程
void* device3_thread(void* arg);

// 设备4接收线程
void* device4_receive_thread(void* arg);

// 设备4处理线程
void* device4_process_thread(void* arg);

// 设备4线程
void* device4_thread(void* arg);

// 加解密函数
uint8_t* encrypt_data(uint8_t* data, uint16_t length, uint8_t key);
uint8_t* decrypt_data(uint8_t* data, uint16_t length, uint8_t key);
uint8_t get_key_for_bus(uint8_t source_addr, uint8_t dest_addr);

// 转发回调函数
eproto_error_t device2_forward_callback(eproto_bus_t* bus, uint8_t source_addr, uint8_t dest_addr,
                                        uint8_t* data, uint16_t length,
                                        uint8_t** out_data, uint16_t* out_length,
                                        eproto_forward_post_func_t* post_func,
                                        void** private_data);
void device2_forward_post_func(eproto_bus_t* bus, uint8_t source_addr, uint8_t dest_addr,
                                uint8_t* out_data, uint16_t out_length,
                                void* private_data);

#endif  // COMMON_H
