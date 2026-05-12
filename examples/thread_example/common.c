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

// 协议包装函数：在数据前添加2字节协议头
uint8_t* protocol_wrap(uint8_t encrypted, uint8_t need_reply, const uint8_t* data, uint16_t length, uint16_t* out_length) {
    uint16_t total_length = PROTOCOL_HEADER_SIZE + length;
    uint8_t* wrapped = (uint8_t*)malloc(total_length);
    if (!wrapped) {
        *out_length = 0;
        return NULL;
    }
    wrapped[0] = encrypted ? PROTOCOL_FLAG_ENCRYPTED : 0;
    wrapped[1] = need_reply ? PROTOCOL_FLAG_NEED_REPLY : 0;
    if (data && length > 0) {
        memcpy(wrapped + PROTOCOL_HEADER_SIZE, data, length);
    }
    *out_length = total_length;
    return wrapped;
}

// 协议解包函数：从数据中解析协议头
uint8_t* protocol_unwrap(const uint8_t* data, uint16_t length, uint8_t* out_encrypted, uint8_t* out_need_reply, uint16_t* out_data_length) {
    if (length < PROTOCOL_HEADER_SIZE) {
        *out_data_length = 0;
        return NULL;
    }
    *out_encrypted = (data[0] & PROTOCOL_FLAG_ENCRYPTED) ? 1 : 0;
    *out_need_reply = (data[1] & PROTOCOL_FLAG_NEED_REPLY) ? 1 : 0;
    *out_data_length = length - PROTOCOL_HEADER_SIZE;
    return (uint8_t*)(data + PROTOCOL_HEADER_SIZE);
}

// 设备1的总线发送函数（写入共享缓冲区1）
void device1_bus_send(eproto_bus_t* bus, uint8_t* data, uint16_t length) {
    (void)bus;
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
void device2_bus_send(eproto_bus_t* bus, uint8_t* data, uint16_t length) {
    (void)bus;
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
void device2_bus2_send(eproto_bus_t* bus, uint8_t* data, uint16_t length) {
    (void)bus;
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
void device3_bus_send(eproto_bus_t* bus, uint8_t* data, uint16_t length) {
    (void)bus;
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

// 加密函数
uint8_t* encrypt_data(uint8_t* data, uint16_t length, uint8_t key) {
    uint8_t* encrypted = (uint8_t*)malloc(length);
    if (!encrypted) {
        return NULL;
    }
    for (uint16_t i = 0; i < length; i++) {
        encrypted[i] = data[i] ^ key;
    }
    return encrypted;
}

// 解密函数
uint8_t* decrypt_data(uint8_t* data, uint16_t length, uint8_t key) {
    return encrypt_data(data, length, key);
}

// 根据源地址和目标地址获取密钥
uint8_t get_key_for_bus(uint8_t source_addr, uint8_t dest_addr) {
    if ((source_addr == 0x01 && dest_addr == 0x02) ||
        (source_addr == 0x02 && dest_addr == 0x01)) {
        return KEY_BUS_1_2;
    }
    if ((source_addr == 0x03 && dest_addr == 0x04) ||
        (source_addr == 0x04 && dest_addr == 0x03)) {
        return KEY_BUS_3_4;
    }
    return 0;
}

#if EPROTO_ENABLE_FORWARD
// 转发后处理回调函数
void device2_forward_post_func(eproto_bus_t* bus, uint8_t source_addr, uint8_t dest_addr,
                              uint8_t* out_data, uint16_t out_length,
                              void* private_data) {
    (void)bus;
    (void)source_addr;
    (void)dest_addr;
    (void)out_length;
    (void)private_data;
    if (out_data) {
        free(out_data);
    }
}

// 转发回调函数
eproto_error_t device2_forward_callback(eproto_bus_t* bus, uint8_t source_addr, uint8_t dest_addr,
                                       uint8_t* data, uint16_t length,
                                       uint8_t** out_data, uint16_t* out_length,
                                       eproto_forward_post_func_t* post_func,
                                       void** private_data) {
    (void)bus;
    (void)private_data;
    
    if (!data || length < PROTOCOL_HEADER_SIZE) {
        printf("Device 2: Invalid input data in forward callback\n");
        return EPROTO_ERROR_INVALID_ARGUMENT;
    }
    
    if (!out_data || !out_length) {
        printf("Device 2: Invalid output parameters in forward callback\n");
        return EPROTO_ERROR_INVALID_ARGUMENT;
    }
    
    uint8_t encrypted = (data[0] & PROTOCOL_FLAG_ENCRYPTED) ? 1 : 0;
    
    printf("Device 2: Forward callback called, source bus: 0x%02X, dest bus: 0x%02X, encrypted: %d, length: %d\n", 
           source_addr, dest_addr, encrypted, length);
    
    if (encrypted) {
        uint8_t key = get_key_for_bus(source_addr, dest_addr);
        uint8_t* decrypted_data = decrypt_data(data + PROTOCOL_HEADER_SIZE, length - PROTOCOL_HEADER_SIZE, key);
        if (!decrypted_data) {
            return EPROTO_ERROR_BUFFER_FULL;
        }
        
        printf("Device 2: Decrypted payload data: ");
        for (uint16_t i = 0; i < length - PROTOCOL_HEADER_SIZE; i++) {
            printf("%02X ", decrypted_data[i]);
        }
        printf("\n");
        
        uint8_t* re_encrypted_data = encrypt_data(decrypted_data, length - PROTOCOL_HEADER_SIZE, key);
        free(decrypted_data);
        
        if (!re_encrypted_data) {
            return EPROTO_ERROR_BUFFER_FULL;
        }
        
        *out_data = (uint8_t*)malloc(length);
        if (!*out_data) {
            free(re_encrypted_data);
            return EPROTO_ERROR_BUFFER_FULL;
        }
        
        (*out_data)[0] = data[0];
        (*out_data)[1] = data[1];
        memcpy(*out_data + PROTOCOL_HEADER_SIZE, re_encrypted_data, length - PROTOCOL_HEADER_SIZE);
        *out_length = length;
        
        printf("Device 2: Re-encrypted payload data: ");
        for (uint16_t i = 0; i < length - PROTOCOL_HEADER_SIZE; i++) {
            printf("%02X ", re_encrypted_data[i]);
        }
        printf("\n");
        
        free(re_encrypted_data);
    } else {
        *out_data = (uint8_t*)malloc(length);
        if (!*out_data) {
            return EPROTO_ERROR_BUFFER_FULL;
        }
        memcpy(*out_data, data, length);
        *out_length = length;
        
        printf("Device 2: Non-encrypted data passed through\n");
    }
    
    *post_func = device2_forward_post_func;
    
    return EPROTO_OK;
}
#endif
