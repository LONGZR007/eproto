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
    sem_t semaphore;  // 信号量
    int semaphore_initialized;  // 信号量初始化状态
    int signal_flag;  // 信号标志，用于模拟裸机情况
    uint8_t rx_buffer[256];  // 接收缓冲区1
    uint8_t rx_buffer2[256];  // 接收缓冲区2（用于第二条总线）
} thread_data_t;

// 全局变量声明
extern uint8_t g_shared_buffer1[SHARED_BUFFER_SIZE];  // 设备1 -> 设备2
extern uint8_t g_shared_buffer2[SHARED_BUFFER_SIZE];  // 设备2 -> 设备1
extern uint8_t g_shared_buffer3[SHARED_BUFFER_SIZE];  // 设备2 -> 设备3
extern uint8_t g_shared_buffer4[SHARED_BUFFER_SIZE];  // 设备3 -> 设备2
extern uint16_t g_shared_buffer1_head;
extern uint16_t g_shared_buffer1_tail;
extern uint16_t g_shared_buffer2_head;
extern uint16_t g_shared_buffer2_tail;
extern uint16_t g_shared_buffer3_head;
extern uint16_t g_shared_buffer3_tail;
extern uint16_t g_shared_buffer4_head;
extern uint16_t g_shared_buffer4_tail;
extern pthread_mutex_t g_mutex1;
extern pthread_mutex_t g_mutex2;
extern pthread_mutex_t g_mutex3;
extern pthread_mutex_t g_mutex4;

// 全局线程数据指针，用于信号函数访问
// 使用线程局部存储来存储当前线程数据，避免线程安全问题
extern __thread thread_data_t* g_current_thread_data;

// 设备1的总线发送函数（写入共享缓冲区1）
void device1_bus_send(uint8_t* data, uint16_t length);

// 设备1的总线接收函数（从共享缓冲区2读取）
uint16_t device1_bus_receive(uint8_t* buffer, uint16_t size);

// 设备2的总线发送函数（写入共享缓冲区2）
void device2_bus_send(uint8_t* data, uint16_t length);

// 设备2的总线接收函数（从共享缓冲区1读取）
uint16_t device2_bus_receive(uint8_t* buffer, uint16_t size);

// 设备2的第二条总线发送函数（写入共享缓冲区3）
void device2_bus2_send(uint8_t* data, uint16_t length);

// 设备2的第二条总线接收函数（从共享缓冲区4读取）
uint16_t device2_bus2_receive(uint8_t* buffer, uint16_t size);

// 设备3的总线发送函数（写入共享缓冲区4）
void device3_bus_send(uint8_t* data, uint16_t length);

// 设备3的总线接收函数（从共享缓冲区3读取）
uint16_t device3_bus_receive(uint8_t* buffer, uint16_t size);

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
void mock_status_callback(eproto_status_t status, uint8_t* data, uint16_t length);

// 设备1接收回调函数
void device1_receive_callback(uint8_t source_address, uint16_t packet_id, uint8_t* data, uint16_t length);

// 设备1发送回调函数
void device1_send_callback(eproto_send_status_t status, uint16_t packet_id, uint8_t* data, uint16_t length, void* private_data);

// 设备2接收回调函数
void device2_receive_callback(uint8_t source_address, uint16_t packet_id, uint8_t* data, uint16_t length);

// 设备2发送回调函数
void device2_send_callback(eproto_send_status_t status, uint16_t packet_id, uint8_t* data, uint16_t length, void* private_data);

// 设备3接收回调函数
void device3_receive_callback(uint8_t source_address, uint16_t packet_id, uint8_t* data, uint16_t length);

// 设备3发送回调函数
void device3_send_callback(eproto_send_status_t status, uint16_t packet_id, uint8_t* data, uint16_t length, void* private_data);

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

#endif // COMMON_H
