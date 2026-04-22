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

int main() {
    printf("=== Forward Callback Test ===\n");
    
    // 创建设备数据结构
    thread_data_t device_a_data = {
        .device_address = DEVICE_A_ADDRESS,
        .device_name = "Device A"
    };
    
    thread_data_t device_b_data = {
        .device_address = DEVICE_B_ADDRESS_1,
        .device_name = "Device B"
    };
    
    thread_data_t device_c_data = {
        .device_address = DEVICE_C_ADDRESS,
        .device_name = "Device C"
    };
    
    // 创建线程
    pthread_t device_a_thread_id, device_b_thread_id, device_c_thread_id;
    
    // 启动设备 C 线程
    if (pthread_create(&device_c_thread_id, NULL, device_c_thread, &device_c_data) != 0) {
        printf("Failed to create device C thread\n");
        return 1;
    }
    
    // 启动设备 B 线程
    if (pthread_create(&device_b_thread_id, NULL, device_b_thread, &device_b_data) != 0) {
        printf("Failed to create device B thread\n");
        return 1;
    }
    
    // 启动设备 A 线程
    if (pthread_create(&device_a_thread_id, NULL, device_a_thread, &device_a_data) != 0) {
        printf("Failed to create device A thread\n");
        return 1;
    }
    
    // 等待一段时间让测试完成
    sleep(5);
    
    // 强制结束线程（在实际测试中，应该使用更优雅的退出机制）
    printf("=== Test completed ===\n");
    
    return 0;
}
