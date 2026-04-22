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

// 设备 A 接收线程
void* device_a_receive_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    printf("%s receive thread started\n", data->device_name);
    fflush(stdout);

    g_current_thread_data = data;

    for (int i = 0; i < 100; i++) {
        uint8_t rx_buffer[256];
        uint16_t rx_count = device_a_bus_receive(rx_buffer, sizeof(rx_buffer));
        if (rx_count > 0) {
            eproto_receive_data(&data->eproto_inst, DEVICE_A_ADDRESS, rx_buffer, rx_count);
        }
        usleep(50000);
    }

    printf("%s receive thread finished\n", data->device_name);
    fflush(stdout);
    pthread_exit(NULL);
}

// 设备 A 处理线程
void* device_a_process_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    printf("%s process thread started\n", data->device_name);
    fflush(stdout);

    g_current_thread_data = data;

    usleep(100000);

    // 发送测试数据
    uint8_t original_data[] = "Hello";
    uint16_t data_length = sizeof(original_data) - 1;
    printf("%s: Original data: ", data->device_name);
    print_hex(original_data, data_length, "");
    
    // 加密数据
    uint8_t* encrypted_data = encrypt_data(original_data, data_length, KEY_BUS_A_B);
    if (!encrypted_data) {
        printf("%s: Failed to encrypt data\n", data->device_name);
        pthread_exit(NULL);
    }
    
    printf("%s: Encrypted data: ", data->device_name);
    print_hex(encrypted_data, data_length, "");
    
    // 发送加密数据
    eproto_error_t error = eproto_send(&data->eproto_inst, DEVICE_C_ADDRESS, encrypted_data, data_length, NULL, NULL, 0);
    if (error != EPROTO_OK) {
        printf("%s: Failed to send data\n", data->device_name);
        free(encrypted_data);
        pthread_exit(NULL);
    }
    printf("%s: Data sent successfully\n", data->device_name);
    
    free(encrypted_data);

    for (int i = 0; i < 100; i++) {
        eproto_process(&data->eproto_inst);
        usleep(50000);
    }

    printf("%s process thread finished\n", data->device_name);
    fflush(stdout);
    pthread_exit(NULL);
}

void* device_a_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    printf("%s thread started\n", data->device_name);
    fflush(stdout);

    fixed_block_allocator_init();

    eproto_user_functions_t user_functions = {
        .malloc = fixed_block_alloc,
        .free = fixed_block_free,
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
    g_device_a_data = data;

    error = eproto_add_bus(&data->eproto_inst, DEVICE_A_ADDRESS, device_a_bus_send, data->rx_buffer, sizeof(data->rx_buffer),
                          "device_a_bus", mock_status_callback, device_a_receive_callback, NULL);
    if (error != EPROTO_OK) {
        printf("%s: Failed to add bus\n", data->device_name);
        fflush(stdout);
        pthread_exit(NULL);
    }
    printf("%s: Bus added successfully\n", data->device_name);
    fflush(stdout);

    // 添加目标设备（Device B2 和 C4）
    error = eproto_add_destination_device(&data->eproto_inst, DEVICE_A_ADDRESS, DEVICE_B_ADDRESS_1);
    if (error != EPROTO_OK) {
        printf("%s: Failed to add destination device B2\n", data->device_name);
        fflush(stdout);
        pthread_exit(NULL);
    }
    printf("%s: Destination device B2 (0x%02X) added successfully\n", data->device_name, DEVICE_B_ADDRESS_1);
    fflush(stdout);
    
    error = eproto_add_destination_device(&data->eproto_inst, DEVICE_A_ADDRESS, DEVICE_C_ADDRESS);
    if (error != EPROTO_OK) {
        printf("%s: Failed to add destination device C4\n", data->device_name);
        fflush(stdout);
        pthread_exit(NULL);
    }
    printf("%s: Destination device C4 (0x%02X) added successfully\n", data->device_name, DEVICE_C_ADDRESS);
    fflush(stdout);

    pthread_t receive_thread, process_thread;

    data->thread_type = THREAD_TYPE_RECEIVE;

    if (pthread_create(&receive_thread, NULL, device_a_receive_thread, data) != 0) {
        printf("%s: Failed to create receive thread\n", data->device_name);
        pthread_exit(NULL);
    }

    data->thread_type = THREAD_TYPE_PROCESS;

    if (pthread_create(&process_thread, NULL, device_a_process_thread, data) != 0) {
        printf("%s: Failed to create process thread\n", data->device_name);
        pthread_exit(NULL);
    }

    pthread_join(receive_thread, NULL);
    pthread_join(process_thread, NULL);

    eproto_destroy(&data->eproto_inst);

    printf("%s thread finished\n", data->device_name);
    pthread_exit(NULL);
}
