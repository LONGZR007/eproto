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

// 全局变量声明
eproto_t* g_device1_eproto = NULL;

// 设备1接收回调函数
void device1_receive_callback(eproto_bus_t* bus, uint8_t source_address, uint16_t packet_id, uint8_t* data, uint16_t length) {
    if (!bus || !data || length == 0) {
        printf("Device 1: Invalid input in receive callback\n");
        return;
    }
    
    uint8_t encrypted = 0;
    uint8_t need_reply = 0;
    uint16_t payload_length = 0;
    uint8_t* payload = protocol_unwrap(data, length, &encrypted, &need_reply, &payload_length);

    printf("Device 1 received data from device 0x%02X, packet ID: %d: ", source_address, packet_id);
    for (uint16_t i = 0; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");

    if (encrypted && payload && payload_length > 0) {
        uint8_t key = get_key_for_bus(source_address, bus->self_addr);
        uint8_t* decrypted_payload = decrypt_data(payload, payload_length, key);
        if (decrypted_payload) {
            printf("Device 1: Decrypted payload: ");
            for (uint16_t i = 0; i < payload_length; i++) {
                printf("%02X ", decrypted_payload[i]);
            }
            printf("\n");
            free(decrypted_payload);
        }
    }

    if (payload && payload_length > 0 && need_reply) {
        printf("Device 1: Protocol header - encrypted=%d, need_reply=%d\n", encrypted, need_reply);
        printf("Device 1: Sending reply with payload: ");
        for (uint16_t i = 0; i < payload_length; i++) {
            printf("%02X ", payload[i]);
        }
        printf("\n");
        uint8_t* reply_data = NULL;
        uint16_t reply_length = 0;
        reply_data = protocol_wrap(0, 0, payload, payload_length, &reply_length);
        if (reply_data) {
            eproto_error_t error = eproto_send_user_reply(g_device1_eproto, source_address, packet_id, reply_data, reply_length);
            if (error != EPROTO_OK) {
                printf("Device 1: Failed to send reply\n");
            } else {
                printf("Device 1: Reply sent successfully\n");
            }
            free(reply_data);
        }
    }
}

// 设备1发送回调函数
void device1_send_callback(eproto_send_status_t status, uint16_t packet_id, uint8_t* data, uint16_t length,
                           void* private_data) {
    (void)data;
    (void)length;
    thread_data_t* thread_data = (thread_data_t*)private_data;
    switch (status) {
        case EPROTO_SEND_SUCCESS:
            printf("%s: Send success, packet ID: %d\n", thread_data->device_name, packet_id);
            break;
        case EPROTO_SEND_TIMEOUT:
            printf("%s: Send timeout, packet ID: %d\n", thread_data->device_name, packet_id);
            break;
        case EPROTO_SEND_ERROR:
            printf("%s: Send error, packet ID: %d\n", thread_data->device_name, packet_id);
            break;
        case EPROTO_SEND_BUSY:
            printf("%s: Send busy, packet ID: %d\n", thread_data->device_name, packet_id);
            break;
    }
}

// 设备1信号等待函数
eproto_signal_result_t device1_signal_wait(uint32_t timestamp) {
    if (!g_current_thread_data) {
        return EPROTO_SIGNAL_TIMEOUT;
    }

    if (!g_current_thread_data->semaphore_initialized) {
        // 初始化信号量
        if (sem_init(&g_current_thread_data->semaphore, 0, 0) != 0) {
            printf("%s: Failed to initialize semaphore\n", g_current_thread_data->device_name);
            return EPROTO_SIGNAL_TIMEOUT;
        }
        g_current_thread_data->semaphore_initialized = 1;
    }

    // 计算超时时间（毫秒）
    uint32_t current_time = mock_get_timestamp();
    uint32_t timeout_ms = 0;
    if (timestamp > current_time) {
        timeout_ms = timestamp - current_time;
    }

    // 等待信号量，使用超时
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
        // 收到信号
        return EPROTO_SIGNAL_DATA;
    } else {
        // 超时
        return EPROTO_SIGNAL_TIMEOUT;
    }
}

// 设备1信号发送函数
void device1_signal_send(void) {
    if (g_current_thread_data && g_current_thread_data->semaphore_initialized) {
        sem_post(&g_current_thread_data->semaphore);
    }
}

// 设备1接收线程
void* device1_receive_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    printf("%s receive thread started\n", data->device_name);

    // 设置当前线程数据
    g_current_thread_data = data;

    // 定期接收数据
    while (1) {
        // 模拟从总线接收数据
        uint8_t rx_buffer[256];
        uint16_t rx_count = device1_bus_receive(rx_buffer, sizeof(rx_buffer));
        if (rx_count > 0) {
            // 使用设备1自己的总线地址0x01
            eproto_receive_data(&data->eproto_inst, 0x01, rx_buffer, rx_count);
        }
        usleep(10000);
    }

    printf("%s receive thread finished\n", data->device_name);
    pthread_exit(NULL);
}

// 设备1处理线程
void* device1_process_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    printf("%s process thread started\n", data->device_name);

    // 设置当前线程数据
    g_current_thread_data = data;

    // 等待设备2初始化
    usleep(1000);

    // 测试1: 非加密数据从设备1到设备2的总线2
    uint8_t test_data_plain[] = {0x11, 0x22, 0x33, 0x44, 0x55};
    uint16_t wrapped_length = 0;
    uint8_t* wrapped_data = protocol_wrap(0, 1, test_data_plain, sizeof(test_data_plain), &wrapped_length);
    if (wrapped_data) {
        printf("%s: [TEST 1] Sending PLAIN data to device 2 (bus 2)...\n", data->device_name);
        printf("%s: Wrapped data: ", data->device_name);
        for (uint16_t i = 0; i < wrapped_length; i++) {
            printf("%02X ", wrapped_data[i]);
        }
        printf("\n");
        eproto_error_t error = eproto_send(&data->eproto_inst, 0x02, wrapped_data, wrapped_length, device1_send_callback, data, 1);
        free(wrapped_data);
        if (error != EPROTO_OK) {
            printf("%s: Failed to send plain data\n", data->device_name);
        } else {
            printf("%s: Plain data sent successfully\n", data->device_name);
        }
    }

    // 等待一段时间，确保前面的数据包处理完成
    usleep(100000);

    // 测试2: 加密数据从设备1到设备2的总线2
    uint8_t test_data_encrypted[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
    uint8_t* encrypted_payload = encrypt_data(test_data_encrypted, sizeof(test_data_encrypted), KEY_BUS_1_2);
    if (encrypted_payload) {
        wrapped_data = protocol_wrap(1, 1, encrypted_payload, sizeof(test_data_encrypted), &wrapped_length);
        free(encrypted_payload);
        if (wrapped_data) {
            printf("%s: [TEST 2] Sending ENCRYPTED data to device 2 (bus 2)...\n", data->device_name);
            printf("%s: Wrapped data: ", data->device_name);
            for (uint16_t i = 0; i < wrapped_length; i++) {
                printf("%02X ", wrapped_data[i]);
            }
            printf("\n");
            eproto_error_t error = eproto_send(&data->eproto_inst, 0x02, wrapped_data, wrapped_length, device1_send_callback, data, 1);
            free(wrapped_data);
            if (error != EPROTO_OK) {
                printf("%s: Failed to send encrypted data\n", data->device_name);
            } else {
                printf("%s: Encrypted data sent successfully\n", data->device_name);
            }
        }
    }

    // 等待一段时间，确保前面的数据包处理完成
    usleep(100000);

    // 测试3: 非加密数据从设备1到设备3（通过设备2转发）
    uint8_t test_data_to_3_plain[] = {0x33, 0x44, 0x55, 0x66, 0x77};
    wrapped_data = protocol_wrap(0, 1, test_data_to_3_plain, sizeof(test_data_to_3_plain), &wrapped_length);
    if (wrapped_data) {
        printf("%s: [TEST 3] Sending PLAIN data to device 3 (via device 2 forwarding)...\n", data->device_name);
        printf("%s: Wrapped data: ", data->device_name);
        for (uint16_t i = 0; i < wrapped_length; i++) {
            printf("%02X ", wrapped_data[i]);
        }
        printf("\n");
        eproto_error_t error = eproto_send(&data->eproto_inst, 0x04, wrapped_data, wrapped_length, device1_send_callback, data, 1);
        free(wrapped_data);
        if (error != EPROTO_OK) {
            printf("%s: Failed to send plain data to device 3\n", data->device_name);
        } else {
            printf("%s: Plain data to device 3 sent successfully\n", data->device_name);
        }
    }

    // 等待一段时间，确保前面的数据包处理完成
    usleep(100000);

    // 测试4: 加密数据从设备1到设备3（通过设备2转发）
    uint8_t test_data_to_3_encrypted[] = {0x11, 0x22, 0x33, 0x44, 0x55};
    encrypted_payload = encrypt_data(test_data_to_3_encrypted, sizeof(test_data_to_3_encrypted), KEY_BUS_1_2);
    if (encrypted_payload) {
        wrapped_data = protocol_wrap(1, 1, encrypted_payload, sizeof(test_data_to_3_encrypted), &wrapped_length);
        free(encrypted_payload);
        if (wrapped_data) {
            printf("%s: [TEST 4] Sending ENCRYPTED data to device 3 (via device 2 forwarding)...\n", data->device_name);
            printf("%s: Wrapped data: ", data->device_name);
            for (uint16_t i = 0; i < wrapped_length; i++) {
                printf("%02X ", wrapped_data[i]);
            }
            printf("\n");
            eproto_error_t error = eproto_send(&data->eproto_inst, 0x04, wrapped_data, wrapped_length, device1_send_callback, data, 1);
            free(wrapped_data);
            if (error != EPROTO_OK) {
                printf("%s: Failed to send encrypted data to device 3\n", data->device_name);
            } else {
                printf("%s: Encrypted data to device 3 sent successfully\n", data->device_name);
            }
        }
    }

    // 等待一段时间，确保所有的数据包处理完成
    usleep(100000);

    // 发送广播数据
    uint8_t broadcast_data[] = {0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    wrapped_data = protocol_wrap(0, 0, broadcast_data, sizeof(broadcast_data), &wrapped_length);
    if (wrapped_data) {
        printf("%s: Sending broadcast data to all devices...\n", data->device_name);
        printf("%s: Wrapped data: ", data->device_name);
        for (uint16_t i = 0; i < wrapped_length; i++) {
            printf("%02X ", wrapped_data[i]);
        }
        printf("\n");
        eproto_error_t error = eproto_send_ex(&data->eproto_inst, 0xFF, wrapped_data, wrapped_length, device1_send_callback, data, 0, 0, 0);
        free(wrapped_data);
        if (error != EPROTO_OK) {
            printf("%s: Failed to send broadcast data\n", data->device_name);
        } else {
            printf("%s: Broadcast data sent successfully\n", data->device_name);
        }
    }

    // 定期处理协议
    while (1) {
        eproto_process(&data->eproto_inst);
        usleep(10000);
    }

    // 注意：不要在这里销毁eProto，因为data是device1_data的副本
    // eproto_destroy应该在主线程中调用，或者使用指针而不是副本

    printf("%s process thread finished\n", data->device_name);
    pthread_exit(NULL);
}

// 设备1线程
void* device1_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    printf("%s thread started\n", data->device_name);

    // 初始化信号量
    data->semaphore_initialized = 0;
    data->signal_flag = 0;

    // 初始化用户函数结构体
    eproto_user_functions_t user_functions = {.malloc = mock_malloc,
                                              .free = mock_free,
                                              .signal_wait = device1_signal_wait,
                                              .signal_send = device1_signal_send,
                                              .lock = mock_lock,
                                              .unlock = mock_unlock,
                                              .get_timestamp = mock_get_timestamp,
                                              .timeout_timestamp = 0};

    // 初始化eProto
    eproto_error_t error = eproto_init(&data->eproto_inst, &user_functions);
    if (error != EPROTO_OK) {
        printf("%s: Failed to initialize eProto\n", data->device_name);
        pthread_exit(NULL);
    }
    printf("%s: eProto initialized successfully\n", data->device_name);

    // 设置全局eproto实例指针
    g_device1_eproto = &data->eproto_inst;

    // 添加路由（使用设备1自己的总线地址0x01）
    eproto_bus_t bus = {
        .self_addr = 0x01,
        .send = device1_bus_send,
        .rx_buffer = data->rx_buffer,
        .rx_buffer_size = sizeof(data->rx_buffer),
        .name = "device1_bus",
        .user_data = NULL,
        .status_callback = mock_status_callback,
        .receive_callback = device1_receive_callback,
#if EPROTO_ENABLE_FORWARD
        .forward_callback = NULL
#endif
    };
    error = eproto_add_bus(&data->eproto_inst, &bus);
    if (error != EPROTO_OK) {
        printf("%s: Failed to add route\n", data->device_name);
        pthread_exit(NULL);
    }
    printf("%s: Route added successfully\n", data->device_name);

    // 添加目标设备地址（设备2的总线2地址0x02）
    error = eproto_add_destination_device(&data->eproto_inst, 0x01, 0x02);
    if (error != EPROTO_OK) {
        printf("%s: Failed to add target device\n", data->device_name);
        pthread_exit(NULL);
    }
    printf("%s: Target device 0x02 added successfully\n", data->device_name);

    // 添加目标设备地址（设备3的总线4地址0x04）
    error = eproto_add_destination_device(&data->eproto_inst, 0x01, 0x04);
    if (error != EPROTO_OK) {
        printf("%s: Failed to add target device 0x04\n", data->device_name);
        pthread_exit(NULL);
    }
    printf("%s: Target device 0x04 added successfully\n", data->device_name);

    // 创建接收线程和处理线程
    pthread_t receive_thread, process_thread;

    // 直接使用 data 参数，设置线程类型
    data->thread_type = THREAD_TYPE_RECEIVE;

    // 创建接收线程
    if (pthread_create(&receive_thread, NULL, device1_receive_thread, data) != 0) {
        printf("%s: Failed to create receive thread\n", data->device_name);
        pthread_exit(NULL);
    }

    // 设置线程类型为处理线程
    data->thread_type = THREAD_TYPE_PROCESS;

    // 创建处理线程
    if (pthread_create(&process_thread, NULL, device1_process_thread, data) != 0) {
        printf("%s: Failed to create process thread\n", data->device_name);
        pthread_exit(NULL);
    }

    // 等待线程完成
    pthread_join(receive_thread, NULL);
    pthread_join(process_thread, NULL);

    // 销毁 eProto 实例
    eproto_destroy(&data->eproto_inst);

    printf("%s thread finished\n", data->device_name);
    pthread_exit(NULL);
}
