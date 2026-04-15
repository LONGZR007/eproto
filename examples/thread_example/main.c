#include "common.h"

int main() {
    // 创建设备1数据（动态分配）
    thread_data_t* device1_data = (thread_data_t*)malloc(sizeof(thread_data_t));
    if (!device1_data) {
        printf("Failed to allocate memory for device 1 data\n");
        return 1;
    }
    device1_data->device_address = 0x01;
    device1_data->device_name = "Device 1";

    // 创建设备2数据（动态分配）
    thread_data_t* device2_data = (thread_data_t*)malloc(sizeof(thread_data_t));
    if (!device2_data) {
        printf("Failed to allocate memory for device 2 data\n");
        free(device1_data);
        return 1;
    }
    device2_data->device_address = 0x02;
    device2_data->device_name = "Device 2";

    // 创建设备3数据（动态分配）
    thread_data_t* device3_data = (thread_data_t*)malloc(sizeof(thread_data_t));
    if (!device3_data) {
        printf("Failed to allocate memory for device 3 data\n");
        free(device1_data);
        free(device2_data);
        return 1;
    }
    device3_data->device_address = 0x03;
    device3_data->device_name = "Device 3";

    // 创建线程
    pthread_t thread1, thread2, thread3;

    // 启动设备1线程
    if (pthread_create(&thread1, NULL, device1_thread, device1_data) != 0) {
        printf("Failed to create device 1 thread\n");
        free(device1_data);
        free(device2_data);
        free(device3_data);
        return 1;
    }

    // 启动设备2线程
    if (pthread_create(&thread2, NULL, device2_thread, device2_data) != 0) {
        printf("Failed to create device 2 thread\n");
        free(device1_data);
        free(device2_data);
        free(device3_data);
        return 1;
    }

    // 启动设备3线程
    if (pthread_create(&thread3, NULL, device3_thread, device3_data) != 0) {
        printf("Failed to create device 3 thread\n");
        free(device1_data);
        free(device2_data);
        free(device3_data);
        return 1;
    }

    // 等待所有线程完成
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    pthread_join(thread3, NULL);

    // 释放动态分配的内存
    free(device1_data);
    free(device2_data);
    free(device3_data);

    printf("All threads finished\n");
    return 0;
}
