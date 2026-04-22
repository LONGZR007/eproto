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

// 设备 B 接收线程（接收两条总线的数据）
void* device_b_receive_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    printf("%s receive thread started\n", data->device_name);
    fflush(stdout);

    g_current_thread_data = data;

    for (int i = 0; i < 100; i++) {
        // 接收总线 1 (A-B) 的数据
        uint8_t rx_buffer1[256];
        uint16_t rx_count1 = device_b_bus1_receive(rx_buffer1, sizeof(rx_buffer1));
        if (rx_count1 > 0) {
            eproto_receive_data(&data->eproto_inst, BUS_A_B_ADDRESS, rx_buffer1, rx_count1);
        }
        
        // 接收总线 2 (B-C) 的数据
        uint8_t rx_buffer2[256];
        uint16_t rx_count2 = device_b_bus2_receive(rx_buffer2, sizeof(rx_buffer2));
        if (rx_count2 > 0) {
            eproto_receive_data(&data->eproto_inst, BUS_B_C_ADDRESS, rx_buffer2, rx_count2);
        }
        
        usleep(50000);
    }

    printf("%s receive thread finished\n", data->device_name);
    fflush(stdout);
    pthread_exit(NULL);
}

// 设备 B 处理线程
void* device_b_process_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    printf("%s process thread started\n", data->device_name);
    fflush(stdout);

    g_current_thread_data = data;

    for (int i = 0; i < 100; i++) {
        eproto_process(&data->eproto_inst);
        usleep(50000);
    }

    printf("%s process thread finished\n", data->device_name);
    fflush(stdout);
    pthread_exit(NULL);
}

void* device_b_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    printf("%s thread started\n", data->device_name);
    fflush(stdout);

    eproto_user_functions_t user_functions = {
        .malloc = mock_malloc,
        .free = mock_free,
        .signal_wait = mock_signal_wait,
        .signal_send = mock_signal_send,
        .lock = mock_lock,
        .unlock = mock_unlock,
        .get_timestamp = mock_get_timestamp,
        .timeout_timestamp = 0
    };

    eproto_error_t error = eproto_init(&data->eproto_inst, &user_functions);
    if (error != EPROTO_OK) {
        printf("%s: Failed to initialize eProto\n", data->device_name);
        fflush(stdout);
        pthread_exit(NULL);
    }
    printf("%s: eProto initialized successfully\n", data->device_name);
    fflush(stdout);
    g_device_b_data = data;

    // 添加第一条总线（连接到 Device A）
    error = eproto_add_bus(&data->eproto_inst, BUS_A_B_ADDRESS, device_b_bus1_send, data->rx_buffer, sizeof(data->rx_buffer),
                          "device_b_bus1", mock_status_callback, device_b_receive_callback, device_b_forward_callback);
    if (error != EPROTO_OK) {
        printf("%s: Failed to add bus 1\n", data->device_name);
        fflush(stdout);
        pthread_exit(NULL);
    }
    printf("%s: Bus 1 (0x%02X) added successfully\n", data->device_name, BUS_A_B_ADDRESS);
    fflush(stdout);

    // 添加第二条总线（连接到 Device C）
    error = eproto_add_bus(&data->eproto_inst, BUS_B_C_ADDRESS, device_b_bus2_send, data->rx_buffer2, sizeof(data->rx_buffer2),
                          "device_b_bus2", mock_status_callback, device_b_receive_callback, device_b_forward_callback);
    if (error != EPROTO_OK) {
        printf("%s: Failed to add bus 2\n", data->device_name);
        fflush(stdout);
        pthread_exit(NULL);
    }
    printf("%s: Bus 2 (0x%02X) added successfully\n", data->device_name, BUS_B_C_ADDRESS);
    fflush(stdout);

    // 添加目标设备（Device A1）到总线 1
    error = eproto_add_destination_device(&data->eproto_inst, BUS_A_B_ADDRESS, DEVICE_A_ADDRESS);
    if (error != EPROTO_OK) {
        printf("%s: Failed to add destination device A1\n", data->device_name);
        fflush(stdout);
        pthread_exit(NULL);
    }
    printf("%s: Destination device A1 (0x%02X) added successfully\n", data->device_name, DEVICE_A_ADDRESS);
    fflush(stdout);
    
    // 添加目标设备（Device C4）到总线 2
    error = eproto_add_destination_device(&data->eproto_inst, BUS_B_C_ADDRESS, DEVICE_C_ADDRESS);
    if (error != EPROTO_OK) {
        printf("%s: Failed to add destination device C4\n", data->device_name);
        fflush(stdout);
        pthread_exit(NULL);
    }
    printf("%s: Destination device C4 (0x%02X) added successfully\n", data->device_name, DEVICE_C_ADDRESS);
    fflush(stdout);

    pthread_t receive_thread, process_thread;

    data->thread_type = THREAD_TYPE_RECEIVE;

    if (pthread_create(&receive_thread, NULL, device_b_receive_thread, data) != 0) {
        printf("%s: Failed to create receive thread\n", data->device_name);
        pthread_exit(NULL);
    }

    data->thread_type = THREAD_TYPE_PROCESS;

    if (pthread_create(&process_thread, NULL, device_b_process_thread, data) != 0) {
        printf("%s: Failed to create process thread\n", data->device_name);
        pthread_exit(NULL);
    }

    pthread_join(receive_thread, NULL);
    pthread_join(process_thread, NULL);

    eproto_destroy(&data->eproto_inst);

    printf("%s thread finished\n", data->device_name);
    pthread_exit(NULL);
}
