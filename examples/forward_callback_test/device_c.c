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

// 设备 C 接收线程
void* device_c_receive_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    printf("%s receive thread started\n", data->device_name);
    fflush(stdout);

    g_current_thread_data = data;

    for (int i = 0; i < 100; i++) {
        uint8_t rx_buffer[256];
        uint16_t rx_count = device_c_bus_receive(rx_buffer, sizeof(rx_buffer));
        if (rx_count > 0) {
            eproto_receive_data(&data->eproto_inst, DEVICE_C_ADDRESS, rx_buffer, rx_count);
        }
        usleep(50000);
    }

    printf("%s receive thread finished\n", data->device_name);
    fflush(stdout);
    pthread_exit(NULL);
}

// 设备 C 处理线程
void* device_c_process_thread(void* arg) {
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

void* device_c_thread(void* arg) {
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
    g_device_c_data = data;

    eproto_bus_t bus = {
        .self_addr = DEVICE_C_ADDRESS,
        .send = device_c_bus_send,
        .rx_buffer = data->rx_buffer,
        .rx_buffer_size = sizeof(data->rx_buffer),
        .name = "device_c_bus",
        .user_data = NULL,
        .status_callback = mock_status_callback,
        .receive_callback = device_c_receive_callback,
        .forward_callback = NULL
    };
    error = eproto_add_bus(&data->eproto_inst, &bus);
    if (error != EPROTO_OK) {
        printf("%s: Failed to add bus\n", data->device_name);
        fflush(stdout);
        pthread_exit(NULL);
    }
    printf("%s: Bus added successfully\n", data->device_name);
    fflush(stdout);

    // 添加目标设备（Device B3 和 A1）
    error = eproto_add_destination_device(&data->eproto_inst, DEVICE_C_ADDRESS, DEVICE_B_ADDRESS_2);
    if (error != EPROTO_OK) {
        printf("%s: Failed to add destination device B3\n", data->device_name);
        fflush(stdout);
        pthread_exit(NULL);
    }
    printf("%s: Destination device B3 (0x%02X) added successfully\n", data->device_name, DEVICE_B_ADDRESS_2);
    fflush(stdout);
    
    error = eproto_add_destination_device(&data->eproto_inst, DEVICE_C_ADDRESS, DEVICE_A_ADDRESS);
    if (error != EPROTO_OK) {
        printf("%s: Failed to add destination device A1\n", data->device_name);
        fflush(stdout);
        pthread_exit(NULL);
    }
    printf("%s: Destination device A1 (0x%02X) added successfully\n", data->device_name, DEVICE_A_ADDRESS);
    fflush(stdout);

    pthread_t receive_thread, process_thread;

    data->thread_type = THREAD_TYPE_RECEIVE;

    if (pthread_create(&receive_thread, NULL, device_c_receive_thread, data) != 0) {
        printf("%s: Failed to create receive thread\n", data->device_name);
        pthread_exit(NULL);
    }

    data->thread_type = THREAD_TYPE_PROCESS;

    if (pthread_create(&process_thread, NULL, device_c_process_thread, data) != 0) {
        printf("%s: Failed to create process thread\n", data->device_name);
        pthread_exit(NULL);
    }

    pthread_join(receive_thread, NULL);
    pthread_join(process_thread, NULL);

    eproto_destroy(&data->eproto_inst);

    printf("%s thread finished\n", data->device_name);
    pthread_exit(NULL);
}
