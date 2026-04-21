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



// 接收线程函数
void* device_a_receive_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    
    while (1) {
        pthread_mutex_lock(&data->tx_mutex);
        pthread_cond_wait(&data->tx_cond, &data->tx_mutex);
        
        // 处理接收到的数据
        eproto_receive_data(&data->eproto_inst, data->tx_bus_address, data->tx_buffer, data->tx_length);
        
        pthread_mutex_unlock(&data->tx_mutex);
    }
    
    return NULL;
}

// 处理线程函数
void* device_a_process_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    
    while (1) {
        eproto_process(&data->eproto_inst);
        usleep(1000); // 睡眠1ms
    }
    
    return NULL;
}

// 设备 A 线程函数
void* device_a_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    printf("Device A thread started\n");
    
    // 初始化互斥锁和条件变量
    pthread_mutex_init(&data->tx_mutex, NULL);
    pthread_cond_init(&data->tx_cond, NULL);
    
    // 初始化用户函数
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
    
    // 初始化 eProto 实例
    eproto_error_t error = eproto_init(&data->eproto_inst, &user_functions);
    if (error != EPROTO_OK) {
        printf("Device A: Failed to initialize eProto\n");
        return NULL;
    }
    printf("Device A: eProto initialized successfully\n");
    
    // 设置全局指针
    g_device_a_data = data;
    
    // 添加总线
    error = eproto_add_bus(&data->eproto_inst, BUS_A_B_ADDRESS, device_a_bus_send, data->rx_buffer, sizeof(data->rx_buffer),
                          "device_a_bus", mock_status_callback, device_a_receive_callback, NULL);
    if (error != EPROTO_OK) {
        printf("Device A: Failed to add bus\n");
        return NULL;
    }
    printf("Device A: Bus added successfully\n");
    
    // 添加目标设备（Device C）
    error = eproto_add_destination_device(&data->eproto_inst, BUS_A_B_ADDRESS, DEVICE_C_ADDRESS);
    if (error != EPROTO_OK) {
        printf("Device A: Failed to add destination device\n");
        return NULL;
    }
    printf("Device A: Destination device 0x%02X added successfully\n", DEVICE_C_ADDRESS);
    
    // 创建接收线程和处理线程
    pthread_t receive_thread, process_thread;
    
    if (pthread_create(&receive_thread, NULL, device_a_receive_thread, data) != 0) {
        printf("Device A: Failed to create receive thread\n");
        return NULL;
    }
    
    if (pthread_create(&process_thread, NULL, device_a_process_thread, data) != 0) {
        printf("Device A: Failed to create process thread\n");
        return NULL;
    }
    
    // 等待一段时间，确保其他设备初始化完成
    sleep(1);
    
    // 发送测试数据
    printf("\nDevice A: Sending test data to Device C...\n");
    
    // 原始数据
    uint8_t original_data[] = "Hello";
    uint16_t data_length = sizeof(original_data) - 1; // 不包含终止符
    
    printf("Device A: Original data: ");
    print_hex(original_data, data_length, "");
    
    // 加密数据
    uint8_t* encrypted_data = encrypt_data(original_data, data_length, KEY_BUS_A_B);
    if (!encrypted_data) {
        printf("Device A: Failed to encrypt data\n");
        return NULL;
    }
    
    printf("Device A: Encrypted data: ");
    print_hex(encrypted_data, data_length, "");
    
    // 发送加密数据
    error = eproto_send(&data->eproto_inst, DEVICE_C_ADDRESS, encrypted_data, data_length, NULL, NULL, 0);
    if (error != EPROTO_OK) {
        printf("Device A: Failed to send data\n");
        free(encrypted_data);
        return NULL;
    }
    printf("Device A: Data sent successfully\n");
    
    // 释放加密数据
    free(encrypted_data);
    
    // 等待测试完成
    sleep(3);
    
    // 等待线程结束
    pthread_join(receive_thread, NULL);
    pthread_join(process_thread, NULL);
    
    return NULL;
}