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

// 线程局部存储
__thread thread_data_t* g_current_thread_data = NULL;

// 全局变量（共享缓冲区）
uint8_t g_shared_buffer_ab[SHARED_BUFFER_SIZE] = {0};  // A-B 共享缓冲区
uint16_t g_shared_buffer_ab_head = 0;
uint16_t g_shared_buffer_ab_tail = 0;
pthread_mutex_t g_mutex_ab = PTHREAD_MUTEX_INITIALIZER;

uint8_t g_shared_buffer_bc[SHARED_BUFFER_SIZE] = {0};  // B-C 共享缓冲区
uint16_t g_shared_buffer_bc_head = 0;
uint16_t g_shared_buffer_bc_tail = 0;
pthread_mutex_t g_mutex_bc = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t g_eproto_lock = PTHREAD_MUTEX_INITIALIZER;

thread_data_t* g_device_a_data = NULL;
thread_data_t* g_device_b_data = NULL;
thread_data_t* g_device_c_data = NULL;

// 模拟状态回调函数
void mock_status_callback(eproto_status_t status, uint8_t* data, uint16_t length) {
    (void)data;
    (void)length;
    printf("Status callback: status = %d\n", status);
}

// 设备 A 接收回调
void device_a_receive_callback(uint8_t source_address, uint16_t packet_id, uint8_t* data, uint16_t length) {
    printf("Device A received data from device 0x%02X, packet ID: %d: ", source_address, packet_id);
    print_hex(data, length, "");
}

// 设备 B 接收回调
void device_b_receive_callback(uint8_t source_address, uint16_t packet_id, uint8_t* data, uint16_t length) {
    printf("Device B received data from device 0x%02X, packet ID: %d: ", source_address, packet_id);
    print_hex(data, length, "");
}

// 设备 C 接收回调
void device_c_receive_callback(uint8_t source_address, uint16_t packet_id, uint8_t* data, uint16_t length) {
    // 解密数据
    uint8_t key = KEY_BUS_B_C;
    uint8_t* decrypted_data = decrypt_data(data, length, key);
    
    printf("Device C received data from device 0x%02X, packet ID: %d: ", source_address, packet_id);
    print_hex(decrypted_data, length, "");
    
    // 验证数据是否正确
    if (length == 5 && memcmp(decrypted_data, "Hello", 5) == 0) {
        printf("Device C: Data verification successful!\n");
    } else {
        printf("Device C: Data verification failed!\n");
    }
    
    free(decrypted_data);
}

// 设备 A 总线发送函数（发送到 A-B 共享缓冲区）
void device_a_bus_send(uint8_t* data, uint16_t length) {
    pthread_mutex_lock(&g_mutex_ab);
    for (uint16_t i = 0; i < length; i++) {
        uint16_t next_head = (g_shared_buffer_ab_head + 1) % SHARED_BUFFER_SIZE;
        if (next_head != g_shared_buffer_ab_tail) {
            g_shared_buffer_ab[g_shared_buffer_ab_head] = data[i];
            g_shared_buffer_ab_head = next_head;
        } else {
            printf("Device A: Buffer overflow\n");
            break;
        }
    }
    pthread_mutex_unlock(&g_mutex_ab);
    printf("Device A sent: ");
    print_hex(data, length, "");
}

// 设备 A 总线接收函数（从 A-B 共享缓冲区接收）
uint16_t device_a_bus_receive(uint8_t* buffer, uint16_t size) {
    uint16_t count = 0;
    pthread_mutex_lock(&g_mutex_ab);
    while (g_shared_buffer_ab_tail != g_shared_buffer_ab_head && count < size) {
        buffer[count++] = g_shared_buffer_ab[g_shared_buffer_ab_tail];
        g_shared_buffer_ab_tail = (g_shared_buffer_ab_tail + 1) % SHARED_BUFFER_SIZE;
    }
    pthread_mutex_unlock(&g_mutex_ab);
    if (count > 0) {
        printf("Device A received %d bytes: ", count);
        print_hex(buffer, count, "");
    }
    return count;
}

// 设备 B 总线 1 发送函数（发送到 A-B 共享缓冲区）
void device_b_bus1_send(uint8_t* data, uint16_t length) {
    pthread_mutex_lock(&g_mutex_ab);
    for (uint16_t i = 0; i < length; i++) {
        uint16_t next_head = (g_shared_buffer_ab_head + 1) % SHARED_BUFFER_SIZE;
        if (next_head != g_shared_buffer_ab_tail) {
            g_shared_buffer_ab[g_shared_buffer_ab_head] = data[i];
            g_shared_buffer_ab_head = next_head;
        } else {
            printf("Device B (bus 1): Buffer overflow\n");
            break;
        }
    }
    pthread_mutex_unlock(&g_mutex_ab);
    printf("Device B (bus 1) sent: ");
    print_hex(data, length, "");
}

// 设备 B 总线 1 接收函数（从 A-B 共享缓冲区接收）
uint16_t device_b_bus1_receive(uint8_t* buffer, uint16_t size) {
    uint16_t count = 0;
    pthread_mutex_lock(&g_mutex_ab);
    while (g_shared_buffer_ab_tail != g_shared_buffer_ab_head && count < size) {
        buffer[count++] = g_shared_buffer_ab[g_shared_buffer_ab_tail];
        g_shared_buffer_ab_tail = (g_shared_buffer_ab_tail + 1) % SHARED_BUFFER_SIZE;
    }
    pthread_mutex_unlock(&g_mutex_ab);
    if (count > 0) {
        printf("Device B (bus 1) received %d bytes: ", count);
        print_hex(buffer, count, "");
    }
    return count;
}

// 设备 B 总线 2 发送函数（发送到 B-C 共享缓冲区）
void device_b_bus2_send(uint8_t* data, uint16_t length) {
    pthread_mutex_lock(&g_mutex_bc);
    for (uint16_t i = 0; i < length; i++) {
        uint16_t next_head = (g_shared_buffer_bc_head + 1) % SHARED_BUFFER_SIZE;
        if (next_head != g_shared_buffer_bc_tail) {
            g_shared_buffer_bc[g_shared_buffer_bc_head] = data[i];
            g_shared_buffer_bc_head = next_head;
        } else {
            printf("Device B (bus 2): Buffer overflow\n");
            break;
        }
    }
    pthread_mutex_unlock(&g_mutex_bc);
    printf("Device B (bus 2) sent: ");
    print_hex(data, length, "");
}

// 设备 B 总线 2 接收函数（从 B-C 共享缓冲区接收）
uint16_t device_b_bus2_receive(uint8_t* buffer, uint16_t size) {
    uint16_t count = 0;
    pthread_mutex_lock(&g_mutex_bc);
    while (g_shared_buffer_bc_tail != g_shared_buffer_bc_head && count < size) {
        buffer[count++] = g_shared_buffer_bc[g_shared_buffer_bc_tail];
        g_shared_buffer_bc_tail = (g_shared_buffer_bc_tail + 1) % SHARED_BUFFER_SIZE;
    }
    pthread_mutex_unlock(&g_mutex_bc);
    if (count > 0) {
        printf("Device B (bus 2) received %d bytes: ", count);
        print_hex(buffer, count, "");
    }
    return count;
}

// 设备 C 总线发送函数（发送到 B-C 共享缓冲区）
void device_c_bus_send(uint8_t* data, uint16_t length) {
    pthread_mutex_lock(&g_mutex_bc);
    for (uint16_t i = 0; i < length; i++) {
        uint16_t next_head = (g_shared_buffer_bc_head + 1) % SHARED_BUFFER_SIZE;
        if (next_head != g_shared_buffer_bc_tail) {
            g_shared_buffer_bc[g_shared_buffer_bc_head] = data[i];
            g_shared_buffer_bc_head = next_head;
        } else {
            printf("Device C: Buffer overflow\n");
            break;
        }
    }
    pthread_mutex_unlock(&g_mutex_bc);
    printf("Device C sent: ");
    print_hex(data, length, "");
}

// 设备 C 总线接收函数（从 B-C 共享缓冲区接收）
uint16_t device_c_bus_receive(uint8_t* buffer, uint16_t size) {
    uint16_t count = 0;
    pthread_mutex_lock(&g_mutex_bc);
    while (g_shared_buffer_bc_tail != g_shared_buffer_bc_head && count < size) {
        buffer[count++] = g_shared_buffer_bc[g_shared_buffer_bc_tail];
        g_shared_buffer_bc_tail = (g_shared_buffer_bc_tail + 1) % SHARED_BUFFER_SIZE;
    }
    pthread_mutex_unlock(&g_mutex_bc);
    if (count > 0) {
        printf("Device C received %d bytes: ", count);
        print_hex(buffer, count, "");
    }
    return count;
}

// 加密函数
uint8_t* encrypt_data(uint8_t* data, uint16_t length, uint8_t key) {
    uint8_t* encrypted = (uint8_t*)malloc(length);
    if (!encrypted) {
        return NULL;
    }
    
    for (int i = 0; i < length; i++) {
        encrypted[i] = data[i] ^ key;
    }
    
    return encrypted;
}

// 解密函数
uint8_t* decrypt_data(uint8_t* data, uint16_t length, uint8_t key) {
    // 异或操作是对称的，所以解密和加密使用相同的函数
    return encrypt_data(data, length, key);
}

// 密钥获取函数
uint8_t get_key_for_bus(uint8_t bus_address) {
    switch (bus_address) {
        case BUS_A_B_ADDRESS:
            return KEY_BUS_A_B;
        case BUS_B_C_ADDRESS:
            return KEY_BUS_B_C;
        default:
            return 0;
    }
}

// 转发后处理回调函数
void device_b_forward_post_func(uint8_t source_addr, uint8_t dest_addr, 
                              uint8_t* out_data, uint16_t out_length,
                              void* private_data) {
    (void)source_addr;
    (void)dest_addr;
    (void)out_length;
    (void)private_data;
    if (out_data) {
        free(out_data);
    }
}

// 转发回调函数
eproto_error_t device_b_forward_callback(uint8_t source_addr, uint8_t dest_addr, 
                                       uint8_t* data, uint16_t length, 
                                       uint8_t** out_data, uint16_t* out_length,
                                       eproto_forward_post_func_t* post_func,
                                       void** private_data) {
    (void)private_data;
    printf("Device B: Forward callback called, source bus: 0x%02X, dest bus: 0x%02X, length: %d\n", source_addr, dest_addr, length);
    
    // 从 source_addr 解密数据
    uint8_t source_key = get_key_for_bus(source_addr);
    uint8_t* decrypted_data = decrypt_data(data, length, source_key);
    
    if (!decrypted_data) {
        return EPROTO_ERROR_BUFFER_FULL;
    }
    
    printf("Device B: Decrypted data: ");
    print_hex(decrypted_data, length, "");
    
    // 用 dest_addr 的密钥加密数据
    uint8_t dest_key = get_key_for_bus(dest_addr);
    *out_data = encrypt_data(decrypted_data, length, dest_key);
    *out_length = length;
    
    if (!*out_data) {
        free(decrypted_data);
        return EPROTO_ERROR_BUFFER_FULL;
    }
    
    printf("Device B: Re-encrypted data: ");
    print_hex(*out_data, length, "");
    
    // 设置后处理回调
    *post_func = device_b_forward_post_func;
    
    // 释放解密后的数据
    free(decrypted_data);
    
    return EPROTO_OK;
}

// 内存分配函数
void* mock_malloc(size_t size) {
    return malloc(size);
}

// 内存释放函数
void mock_free(void* ptr) {
    free(ptr);
}

// 信号等待函数
eproto_signal_result_t mock_signal_wait(uint32_t timestamp) {
    if (!g_current_thread_data) {
        return EPROTO_SIGNAL_TIMEOUT;
    }

    if (!g_current_thread_data->semaphore_initialized) {
        if (sem_init(&g_current_thread_data->semaphore, 0, 0) != 0) {
            printf("%s: Failed to initialize semaphore\n", g_current_thread_data->device_name);
            return EPROTO_SIGNAL_TIMEOUT;
        }
        g_current_thread_data->semaphore_initialized = 1;
    }

    uint32_t current_time = mock_get_timestamp();
    uint32_t timeout_ms = 0;
    if (timestamp > current_time) {
        timeout_ms = timestamp - current_time;
    }
    
    // 限制最大超时时间为 100ms，避免长时间阻塞
    if (timeout_ms > 100) {
        timeout_ms = 100;
    }

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
    }

    int result = sem_timedwait(&g_current_thread_data->semaphore, &ts);
    if (result == 0) {
        return EPROTO_SIGNAL_DATA;
    } else {
        return EPROTO_SIGNAL_TIMEOUT;
    }
}

// 信号发送函数
void mock_signal_send(void) {
    if (g_current_thread_data && g_current_thread_data->semaphore_initialized) {
        sem_post(&g_current_thread_data->semaphore);
    }
}

// 锁函数
void mock_lock(void) {
    pthread_mutex_lock(&g_eproto_lock);
}

// 解锁函数
void mock_unlock(void) {
    pthread_mutex_unlock(&g_eproto_lock);
}

// 获取时间戳函数
uint32_t mock_get_timestamp(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

// 打印十六进制数据
void print_hex(uint8_t* data, uint16_t length, const char* prefix) {
    if (prefix && *prefix) {
        printf("%s ", prefix);
    }
    
    for (int i = 0; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
}
