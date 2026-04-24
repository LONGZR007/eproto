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

#include "common.h"

// 共享缓冲区用于模拟总线通信
uint8_t g_shared_buffer1[SHARED_BUFFER_SIZE];  // 设备1 -> 设备2
uint8_t g_shared_buffer2[SHARED_BUFFER_SIZE];  // 设备2 -> 设备1
uint8_t g_shared_buffer3[SHARED_BUFFER_SIZE];  // 设备2 -> 设备3
uint8_t g_shared_buffer4[SHARED_BUFFER_SIZE];  // 设备3 -> 设备2
uint16_t g_shared_buffer1_head = 0;
uint16_t g_shared_buffer1_tail = 0;
uint16_t g_shared_buffer2_head = 0;
uint16_t g_shared_buffer2_tail = 0;
uint16_t g_shared_buffer3_head = 0;
uint16_t g_shared_buffer3_tail = 0;
uint16_t g_shared_buffer4_head = 0;
uint16_t g_shared_buffer4_tail = 0;
pthread_mutex_t g_mutex1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t g_mutex2 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t g_mutex3 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t g_mutex4 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t g_eproto_lock = PTHREAD_MUTEX_INITIALIZER;

// 全局线程数据指针，用于信号函数访问
// 使用线程局部存储来存储当前线程数据，避免线程安全问题
__thread thread_data_t* g_current_thread_data = NULL;

// 模拟内存分配函数
void* mock_malloc(size_t size) {
    return malloc(size);
}

// 模拟内存释放函数
void mock_free(void* ptr) {
    free(ptr);
}

// 模拟时间戳函数
uint32_t mock_get_timestamp(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

// 模拟锁函数
void mock_lock(void) {
    pthread_mutex_lock(&g_eproto_lock);
}

// 模拟解锁函数
void mock_unlock(void) {
    pthread_mutex_unlock(&g_eproto_lock);
}

// 模拟唤醒函数
void mock_wakeup(void) {
    // 简单的模拟，实际应用中应使用真实的唤醒机制
}

// 模拟状态回调函数
void mock_status_callback(eproto_bus_t* bus, eproto_status_t status, uint8_t* data, uint16_t length) {
    (void)bus;
    (void)data;
    (void)length;
    // 简单的模拟，实际应用中应根据状态执行相应操作
    printf("Status callback: status = %d\n", status);
}

// 设备1的总线发送函数（写入共享缓冲区1）
void device1_bus_send(uint8_t* data, uint16_t length) {
    pthread_mutex_lock(&g_mutex1);
    for (uint16_t i = 0; i < length; i++) {
        uint16_t next_head = (g_shared_buffer1_head + 1) % SHARED_BUFFER_SIZE;
        if (next_head != g_shared_buffer1_tail) {
            g_shared_buffer1[g_shared_buffer1_head] = data[i];
            g_shared_buffer1_head = next_head;
        } else {
            printf("Device 1: Buffer overflow, dropping data\n");
            break;
        }
    }
    pthread_mutex_unlock(&g_mutex1);
    printf("Device 1 sent: ");
    for (uint16_t i = 0; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
}

// 设备1的总线接收函数（从共享缓冲区2读取）
uint16_t device1_bus_receive(uint8_t* buffer, uint16_t size) {
    uint16_t count = 0;
    pthread_mutex_lock(&g_mutex2);
    while (g_shared_buffer2_tail != g_shared_buffer2_head && count < size) {
        buffer[count++] = g_shared_buffer2[g_shared_buffer2_tail];
        g_shared_buffer2_tail = (g_shared_buffer2_tail + 1) % SHARED_BUFFER_SIZE;
    }
    pthread_mutex_unlock(&g_mutex2);
    if (count > 0) {
        printf("Device 1 received %d bytes: ", count);
        for (uint16_t i = 0; i < count; i++) {
            printf("%02X ", buffer[i]);
        }
        printf("\n");
    }
    return count;
}

// 设备2的总线发送函数（写入共享缓冲区2）
void device2_bus_send(uint8_t* data, uint16_t length) {
    pthread_mutex_lock(&g_mutex2);
    for (uint16_t i = 0; i < length; i++) {
        uint16_t next_head = (g_shared_buffer2_head + 1) % SHARED_BUFFER_SIZE;
        if (next_head != g_shared_buffer2_tail) {
            g_shared_buffer2[g_shared_buffer2_head] = data[i];
            g_shared_buffer2_head = next_head;
        } else {
            printf("Device 2: Buffer overflow, dropping data\n");
            break;
        }
    }
    pthread_mutex_unlock(&g_mutex2);
    printf("Device 2 sent: ");
    for (uint16_t i = 0; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
}

// 设备2的总线接收函数（从共享缓冲区1读取）
uint16_t device2_bus_receive(uint8_t* buffer, uint16_t size) {
    uint16_t count = 0;
    pthread_mutex_lock(&g_mutex1);
    while (g_shared_buffer1_tail != g_shared_buffer1_head && count < size) {
        buffer[count++] = g_shared_buffer1[g_shared_buffer1_tail];
        g_shared_buffer1_tail = (g_shared_buffer1_tail + 1) % SHARED_BUFFER_SIZE;
    }
    pthread_mutex_unlock(&g_mutex1);
    if (count > 0) {
        printf("Device 2 received %d bytes: ", count);
        for (uint16_t i = 0; i < count; i++) {
            printf("%02X ", buffer[i]);
        }
        printf("\n");
    }
    return count;
}

// 设备2的第二条总线发送函数（写入共享缓冲区3）
void device2_bus2_send(uint8_t* data, uint16_t length) {
    pthread_mutex_lock(&g_mutex3);
    for (uint16_t i = 0; i < length; i++) {
        uint16_t next_head = (g_shared_buffer3_head + 1) % SHARED_BUFFER_SIZE;
        if (next_head != g_shared_buffer3_tail) {
            g_shared_buffer3[g_shared_buffer3_head] = data[i];
            g_shared_buffer3_head = next_head;
        } else {
            printf("Device 2 (bus 2): Buffer overflow, dropping data\n");
            break;
        }
    }
    pthread_mutex_unlock(&g_mutex3);
    printf("Device 2 (bus 2) sent: ");
    for (uint16_t i = 0; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
}

// 设备2的第二条总线接收函数（从共享缓冲区4读取）
uint16_t device2_bus2_receive(uint8_t* buffer, uint16_t size) {
    uint16_t count = 0;
    pthread_mutex_lock(&g_mutex4);
    while (g_shared_buffer4_tail != g_shared_buffer4_head && count < size) {
        buffer[count++] = g_shared_buffer4[g_shared_buffer4_tail];
        g_shared_buffer4_tail = (g_shared_buffer4_tail + 1) % SHARED_BUFFER_SIZE;
    }
    pthread_mutex_unlock(&g_mutex4);
    if (count > 0) {
        printf("Device 2 (bus 2) received %d bytes: ", count);
        for (uint16_t i = 0; i < count; i++) {
            printf("%02X ", buffer[i]);
        }
        printf("\n");
    }
    return count;
}

// 设备3的总线发送函数（写入共享缓冲区4）
void device3_bus_send(uint8_t* data, uint16_t length) {
    pthread_mutex_lock(&g_mutex4);
    for (uint16_t i = 0; i < length; i++) {
        uint16_t next_head = (g_shared_buffer4_head + 1) % SHARED_BUFFER_SIZE;
        if (next_head != g_shared_buffer4_tail) {
            g_shared_buffer4[g_shared_buffer4_head] = data[i];
            g_shared_buffer4_head = next_head;
        } else {
            printf("Device 3: Buffer overflow, dropping data\n");
            break;
        }
    }
    pthread_mutex_unlock(&g_mutex4);
    printf("Device 3 sent: ");
    for (uint16_t i = 0; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
}

// 设备3的总线接收函数（从共享缓冲区3读取）
uint16_t device3_bus_receive(uint8_t* buffer, uint16_t size) {
    uint16_t count = 0;
    pthread_mutex_lock(&g_mutex3);
    while (g_shared_buffer3_tail != g_shared_buffer3_head && count < size) {
        buffer[count++] = g_shared_buffer3[g_shared_buffer3_tail];
        g_shared_buffer3_tail = (g_shared_buffer3_tail + 1) % SHARED_BUFFER_SIZE;
    }
    pthread_mutex_unlock(&g_mutex3);
    if (count > 0) {
        printf("Device 3 received %d bytes: ", count);
        for (uint16_t i = 0; i < count; i++) {
            printf("%02X ", buffer[i]);
        }
        printf("\n");
    }
    return count;
}
