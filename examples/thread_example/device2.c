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
eproto_t* g_device2_eproto = NULL;

// 设备2接收回调函数
void device2_receive_callback(eproto_bus_t* bus, uint8_t source_address, uint16_t packet_id, uint8_t* data, uint16_t length) {
    uint8_t encrypted = 0;
    uint8_t need_reply = 0;
    uint16_t payload_length = 0;
    uint8_t* payload = protocol_unwrap(data, length, &encrypted, &need_reply, &payload_length);

    printf("Device 2 received data from device 0x%02X, packet ID: %d: ", source_address, packet_id);
    for (uint16_t i = 0; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");

    if (encrypted && payload && payload_length > 0) {
        uint8_t key = get_key_for_bus(source_address, bus->self_addr);
        uint8_t* decrypted_payload = decrypt_data(payload, payload_length, key);
        if (decrypted_payload) {
            printf("Device 2: Decrypted payload: ");
            for (uint16_t i = 0; i < payload_length; i++) {
                printf("%02X ", decrypted_payload[i]);
            }
            printf("\n");
            free(decrypted_payload);
        }
    }

    if (payload && payload_length > 0 && need_reply) {
        printf("Device 2: Protocol header - encrypted=%d, need_reply=%d\n", encrypted, need_reply);
        printf("Device 2: Sending reply with payload: ");
        for (uint16_t i = 0; i < payload_length; i++) {
            printf("%02X ", payload[i]);
        }
        printf("\n");
        uint8_t* reply_data = NULL;
        uint16_t reply_length = 0;
        reply_data = protocol_wrap(0, 0, payload, payload_length, &reply_length);
        if (reply_data) {
            eproto_error_t error = eproto_send_user_reply(g_device2_eproto, source_address, packet_id, reply_data, reply_length);
            if (error != EPROTO_OK) {
                printf("Device 2: Failed to send reply\n");
            } else {
                printf("Device 2: Reply sent successfully\n");
            }
            free(reply_data);
        }
    }
}

// 设备2发送回调函数
void device2_send_callback(eproto_send_status_t status, uint16_t packet_id, uint8_t* data, uint16_t length,
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

// 设备2信号等待函数（模拟裸机环境）
eproto_signal_result_t device2_signal_wait(uint32_t timestamp) {
    if (!g_current_thread_data) {
        return EPROTO_SIGNAL_TIMEOUT;
    }

    // 获取当前时间
    uint32_t current_time = mock_get_timestamp();

    // 模拟裸机环境：使用标志检查和超时检查

    // 检查信号标志
    if (g_current_thread_data->signal_flag) {
        // 收到信号，重置标志
        g_current_thread_data->signal_flag = 0;
        return EPROTO_SIGNAL_DATA;
    }

    // 检查超时
    if (current_time >= timestamp) {
        return EPROTO_SIGNAL_TIMEOUT;
    }

    // 没有数据也没有超时
    return EPROTO_SIGNAL_NO_PROGRESS;
}

// 设备2信号发送函数（模拟裸机环境）
void device2_signal_send(void) {
    if (g_current_thread_data) {
        // 设置信号标志
        g_current_thread_data->signal_flag = 1;
    }
}

// 设备2接收线程
void* device2_receive_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    printf("%s receive thread started\n", data->device_name);

    g_current_thread_data = data;

    while (1) {
        uint8_t rx_buffer1[256];
        uint16_t rx_count1 = device2_bus_receive(rx_buffer1, sizeof(rx_buffer1));
        if (rx_count1 > 0) {
            eproto_receive_data(&data->eproto_inst, 0x02, rx_buffer1, rx_count1);
        }

        uint8_t rx_buffer2[256];
        uint16_t rx_count2 = device2_bus2_receive(rx_buffer2, sizeof(rx_buffer2));
        if (rx_count2 > 0) {
            eproto_receive_data(&data->eproto_inst, 0x03, rx_buffer2, rx_count2);
        }
        usleep(10000);
    }

    printf("%s receive thread finished\n", data->device_name);
    pthread_exit(NULL);
}

// 设备2处理线程
void* device2_process_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    printf("%s process thread started\n", data->device_name);

    // 设置当前线程数据
    g_current_thread_data = data;

    // 定期处理协议
    while (1) {
        eproto_process(&data->eproto_inst);
        usleep(10000);
    }

    // 注意：不要在这里销毁eProto，因为data是device2_data的副本
    // eproto_destroy应该在主线程中调用，或者使用指针而不是副本

    printf("%s process thread finished\n", data->device_name);
    pthread_exit(NULL);
}

// 设备2线程
void* device2_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    printf("%s thread started\n", data->device_name);

    // 初始化信号量和信号标志
    data->semaphore_initialized = 0;
    data->signal_flag = 0;

    // 初始化用户函数结构体
    eproto_user_functions_t user_functions = {.malloc = mock_malloc,
                                              .free = mock_free,
                                              .signal_wait = device2_signal_wait,
                                              .signal_send = device2_signal_send,
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
    g_device2_eproto = &data->eproto_inst;

    // 添加路由（使用设备2自己的总线2地址0x02）
    eproto_bus_t bus1 = {
        .self_addr = 0x02,
        .send = device2_bus_send,
        .rx_buffer = data->rx_buffer,
        .rx_buffer_size = sizeof(data->rx_buffer),
        .name = "device2_bus",
        .user_data = NULL,
        .status_callback = mock_status_callback,
        .receive_callback = device2_receive_callback,
        .forward_callback = device2_forward_callback
    };
    error = eproto_add_bus(&data->eproto_inst, &bus1);
    if (error != EPROTO_OK) {
        printf("%s: Failed to add route\n", data->device_name);
        pthread_exit(NULL);
    }
    printf("%s: Route added successfully\n", data->device_name);

    // 添加目标设备地址（设备1的总线1地址0x01）
    error = eproto_add_destination_device(&data->eproto_inst, 0x02, 0x01);
    if (error != EPROTO_OK) {
        printf("%s: Failed to add target device\n", data->device_name);
        pthread_exit(NULL);
    }
    printf("%s: Target device 0x01 added successfully\n", data->device_name);

    // 添加第二条总线（使用设备2自己的总线3地址0x03）
    eproto_bus_t bus2 = {
        .self_addr = 0x03,
        .send = device2_bus2_send,
        .rx_buffer = data->rx_buffer2,
        .rx_buffer_size = sizeof(data->rx_buffer2),
        .name = "device2_bus2",
        .user_data = NULL,
        .status_callback = mock_status_callback,
        .receive_callback = device2_receive_callback,
        .forward_callback = device2_forward_callback
    };
    error = eproto_add_bus(&data->eproto_inst, &bus2);
    if (error != EPROTO_OK) {
        printf("%s: Failed to add second bus\n", data->device_name);
        pthread_exit(NULL);
    }
    printf("%s: Second bus added successfully\n", data->device_name);

    // 添加目标设备地址（设备3的总线4地址0x04）
    error = eproto_add_destination_device(&data->eproto_inst, 0x03, 0x04);
    if (error != EPROTO_OK) {
        printf("%s: Failed to add target device 0x04\n", data->device_name);
        pthread_exit(NULL);
    }
    printf("%s: Target device 0x04 added successfully\n", data->device_name);

    error = eproto_add_destination_device(&data->eproto_inst, 0x02, 0x04);
    if (error != EPROTO_OK) {
        printf("%s: Failed to add target device 0x04 to bus 2\n", data->device_name);
        pthread_exit(NULL);
    }
    printf("%s: Target device 0x04 added to bus 2 successfully\n", data->device_name);

    // 创建接收线程和处理线程
    pthread_t receive_thread, process_thread;

    // 直接使用 data 参数，设置线程类型
    data->thread_type = THREAD_TYPE_RECEIVE;

    // 创建接收线程
    if (pthread_create(&receive_thread, NULL, device2_receive_thread, data) != 0) {
        printf("%s: Failed to create receive thread\n", data->device_name);
        pthread_exit(NULL);
    }

    // 设置线程类型为处理线程
    data->thread_type = THREAD_TYPE_PROCESS;

    // 创建处理线程
    if (pthread_create(&process_thread, NULL, device2_process_thread, data) != 0) {
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
