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
void* device_c_receive_thread(void* arg) {
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
void* device_c_process_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    
    while (1) {
        eproto_process(&data->eproto_inst);
        usleep(1000); // 睡眠1ms
    }
    
    return NULL;
}

// 设备 C 线程函数
void* device_c_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    printf("Device C thread started\n");
    
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
        printf("Device C: Failed to initialize eProto\n");
        return NULL;
    }
    printf("Device C: eProto initialized successfully\n");
    
    // 设置全局指针
    g_device_c_data = data;
    
    // 添加总线
    error = eproto_add_bus(&data->eproto_inst, BUS_B_C_ADDRESS, device_c_bus_send, data->rx_buffer, sizeof(data->rx_buffer),
                          "device_c_bus", mock_status_callback, device_c_receive_callback, NULL);
    if (error != EPROTO_OK) {
        printf("Device C: Failed to add bus\n");
        return NULL;
    }
    printf("Device C: Bus added successfully\n");
    
    // 添加目标设备（Device A）
    error = eproto_add_destination_device(&data->eproto_inst, BUS_B_C_ADDRESS, DEVICE_A_ADDRESS);
    if (error != EPROTO_OK) {
        printf("Device C: Failed to add destination device 0x%02X\n", DEVICE_A_ADDRESS);
        return NULL;
    }
    printf("Device C: Destination device 0x%02X added successfully\n", DEVICE_A_ADDRESS);
    
    // 添加目标设备（Device B）
    error = eproto_add_destination_device(&data->eproto_inst, BUS_B_C_ADDRESS, DEVICE_B_ADDRESS_2);
    if (error != EPROTO_OK) {
        printf("Device C: Failed to add destination device 0x%02X\n", DEVICE_B_ADDRESS_2);
        return NULL;
    }
    printf("Device C: Destination device 0x%02X added successfully\n", DEVICE_B_ADDRESS_2);
    
    // 创建接收线程和处理线程
    pthread_t receive_thread, process_thread;
    
    if (pthread_create(&receive_thread, NULL, device_c_receive_thread, data) != 0) {
        printf("Device C: Failed to create receive thread\n");
        return NULL;
    }
    
    if (pthread_create(&process_thread, NULL, device_c_process_thread, data) != 0) {
        printf("Device C: Failed to create process thread\n");
        return NULL;
    }
    
    // 等待测试完成
    sleep(5);
    
    // 等待线程结束
    pthread_join(receive_thread, NULL);
    pthread_join(process_thread, NULL);
    
    return NULL;
}
