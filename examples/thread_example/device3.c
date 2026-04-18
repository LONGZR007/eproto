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
eproto_t* g_device3_eproto = NULL;

// 设备3接收回调函数
void device3_receive_callback(uint8_t source_address, uint16_t packet_id, uint8_t* data, uint16_t length) {
    printf("Device 3 received data from device 0x%02X, packet ID: %d: ", source_address, packet_id);
    for (uint16_t i = 0; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");

    // 回复接收到的数据
    printf("Device 3: Sending reply...\n");
    eproto_error_t error = eproto_send_user_reply(g_device3_eproto, 0x03, packet_id, data, length);
    if (error != EPROTO_OK) {
        printf("Device 3: Failed to send reply\n");
    } else {
        printf("Device 3: Reply sent successfully\n");
    }
}

// 设备3发送回调函数
void device3_send_callback(eproto_send_status_t status, uint16_t packet_id, uint8_t* data, uint16_t length,
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

// 设备3信号等待函数
eproto_signal_result_t device3_signal_wait(uint32_t timestamp) {
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

// 设备3信号发送函数
void device3_signal_send(void) {
    if (g_current_thread_data && g_current_thread_data->semaphore_initialized) {
        sem_post(&g_current_thread_data->semaphore);
    }
}

// 设备3接收线程
void* device3_receive_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    printf("%s receive thread started\n", data->device_name);

    // 设置当前线程数据
    g_current_thread_data = data;

    // 定期接收数据
    for (int i = 0; i < 50; i++) {
        // 模拟从总线接收数据
        uint8_t rx_buffer[256];
        uint16_t rx_count = device3_bus_receive(rx_buffer, sizeof(rx_buffer));
        if (rx_count > 0) {
            // 使用设备3自己的总线4地址0x04
            eproto_receive_data(&data->eproto_inst, 0x04, rx_buffer, rx_count);
        }
        usleep(50000);
    }

    printf("%s receive thread finished\n", data->device_name);
    pthread_exit(NULL);
}

// 设备3处理线程
void* device3_process_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    printf("%s process thread started\n", data->device_name);

    // 设置当前线程数据
    g_current_thread_data = data;

    // 定期处理协议
    for (int i = 0; i < 50; i++) {
        eproto_process(&data->eproto_inst);
        usleep(50000);
    }

    // 注意：不要在这里销毁eProto，因为data是device3_data的副本
    // eproto_destroy应该在主线程中调用，或者使用指针而不是副本

    printf("%s process thread finished\n", data->device_name);
    pthread_exit(NULL);
}

// 设备3线程
void* device3_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    printf("%s thread started\n", data->device_name);

    // 初始化信号量
    data->semaphore_initialized = 0;
    data->signal_flag = 0;

    // 初始化用户函数结构体
    eproto_user_functions_t user_functions = {.malloc = mock_malloc,
                                              .free = mock_free,
                                              .signal_wait = device3_signal_wait,
                                              .signal_send = device3_signal_send,
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
    g_device3_eproto = &data->eproto_inst;

    // 定义总线接口
    eproto_bus_t device3_bus = {.send = device3_bus_send, .receive = device3_bus_receive};

    // 添加路由（使用设备3自己的总线4地址0x04）
    error = eproto_add_bus(&data->eproto_inst, 0x04, &device3_bus, data->rx_buffer, sizeof(data->rx_buffer),
                           "device3_bus", mock_wakeup, mock_status_callback, device3_receive_callback);
    if (error != EPROTO_OK) {
        printf("%s: Failed to add route\n", data->device_name);
        pthread_exit(NULL);
    }
    printf("%s: Route added successfully\n", data->device_name);

    // 添加目标设备地址（设备2的总线3地址0x03）
    error = eproto_add_destination_device(&data->eproto_inst, 0x04, 0x03);
    if (error != EPROTO_OK) {
        printf("%s: Failed to add target device\n", data->device_name);
        pthread_exit(NULL);
    }
    printf("%s: Target device 0x03 added successfully\n", data->device_name);

    // 目标设备1（通过设备2转发）
    error = eproto_add_destination_device(&data->eproto_inst, 0x04, 0x01);
    if (error != EPROTO_OK) {
        printf("%s: Failed to add target device 0x01\n", data->device_name);
        pthread_exit(NULL);
    }
    printf("%s: Target device 0x01 added successfully\n", data->device_name);

    // 创建接收线程和处理线程
    pthread_t receive_thread, process_thread;

    // 直接使用 data 参数，设置线程类型
    data->thread_type = THREAD_TYPE_RECEIVE;

    // 创建接收线程
    if (pthread_create(&receive_thread, NULL, device3_receive_thread, data) != 0) {
        printf("%s: Failed to create receive thread\n", data->device_name);
        pthread_exit(NULL);
    }

    // 设置线程类型为处理线程
    data->thread_type = THREAD_TYPE_PROCESS;

    // 创建处理线程
    if (pthread_create(&process_thread, NULL, device3_process_thread, data) != 0) {
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
