#include "eproto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include <semaphore.h>

// 共享缓冲区用于模拟总线通信
#define SHARED_BUFFER_SIZE 1024
static uint8_t g_shared_buffer1[SHARED_BUFFER_SIZE];  // 设备1 -> 设备2
static uint8_t g_shared_buffer2[SHARED_BUFFER_SIZE];  // 设备2 -> 设备1
static uint8_t g_shared_buffer3[SHARED_BUFFER_SIZE];  // 设备2 -> 设备3
static uint8_t g_shared_buffer4[SHARED_BUFFER_SIZE];  // 设备3 -> 设备2
static uint16_t g_shared_buffer1_head = 0;
static uint16_t g_shared_buffer1_tail = 0;
static uint16_t g_shared_buffer2_head = 0;
static uint16_t g_shared_buffer2_tail = 0;
static uint16_t g_shared_buffer3_head = 0;
static uint16_t g_shared_buffer3_tail = 0;
static uint16_t g_shared_buffer4_head = 0;
static uint16_t g_shared_buffer4_tail = 0;
static pthread_mutex_t g_mutex1 = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_mutex2 = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_mutex3 = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_mutex4 = PTHREAD_MUTEX_INITIALIZER;

// 线程类型枚举
typedef enum {
    THREAD_TYPE_RECEIVE,  // 接收线程
    THREAD_TYPE_PROCESS   // 处理线程
} thread_type_t;

// 线程数据结构体
typedef struct {
    uint8_t device_address;
    eproto_t eproto_inst;
    char* device_name;
    uint32_t timestamp;
    pthread_mutex_t timestamp_mutex;
    thread_type_t thread_type;  // 线程类型
    sem_t semaphore;            // 信号量
    int semaphore_initialized;  // 信号量初始化状态
    int signal_flag;            // 信号标志，用于模拟裸机情况
} thread_data_t;

// 设备1的总线发送函数（写入共享缓冲区1）
void device1_bus_send(uint8_t* data, uint16_t length) {
    pthread_mutex_lock(&g_mutex1);
    for (uint16_t i = 0; i < length; i++) {
        uint16_t next_head = (g_shared_buffer1_head + 1) % SHARED_BUFFER_SIZE;
        if (next_head != g_shared_buffer1_tail) {
            g_shared_buffer1[g_shared_buffer1_head] = data[i];
            g_shared_buffer1_head = next_head;
        } else {
            printf("Device 1: Buffer overflow, dropping data\n");
            break;
        }
    }
    pthread_mutex_unlock(&g_mutex1);
    printf("Device 1 sent: ");
    for (uint16_t i = 0; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
}

// 设备1的总线接收函数（从共享缓冲区2读取）
uint16_t device1_bus_receive(uint8_t* buffer, uint16_t size) {
    uint16_t count = 0;
    pthread_mutex_lock(&g_mutex2);
    while (g_shared_buffer2_tail != g_shared_buffer2_head && count < size) {
        buffer[count++] = g_shared_buffer2[g_shared_buffer2_tail];
        g_shared_buffer2_tail = (g_shared_buffer2_tail + 1) % SHARED_BUFFER_SIZE;
    }
    pthread_mutex_unlock(&g_mutex2);
    if (count > 0) {
        printf("Device 1 received %d bytes: ", count);
        for (uint16_t i = 0; i < count; i++) {
            printf("%02X ", buffer[i]);
        }
        printf("\n");
    }
    return count;
}

// 设备2的总线发送函数（写入共享缓冲区2）
void device2_bus_send(uint8_t* data, uint16_t length) {
    pthread_mutex_lock(&g_mutex2);
    for (uint16_t i = 0; i < length; i++) {
        uint16_t next_head = (g_shared_buffer2_head + 1) % SHARED_BUFFER_SIZE;
        if (next_head != g_shared_buffer2_tail) {
            g_shared_buffer2[g_shared_buffer2_head] = data[i];
            g_shared_buffer2_head = next_head;
        } else {
            printf("Device 2: Buffer overflow, dropping data\n");
            break;
        }
    }
    pthread_mutex_unlock(&g_mutex2);
    printf("Device 2 sent: ");
    for (uint16_t i = 0; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
}

// 设备2的总线接收函数（从共享缓冲区1读取）
uint16_t device2_bus_receive(uint8_t* buffer, uint16_t size) {
    uint16_t count = 0;
    pthread_mutex_lock(&g_mutex1);
    while (g_shared_buffer1_tail != g_shared_buffer1_head && count < size) {
        buffer[count++] = g_shared_buffer1[g_shared_buffer1_tail];
        g_shared_buffer1_tail = (g_shared_buffer1_tail + 1) % SHARED_BUFFER_SIZE;
    }
    pthread_mutex_unlock(&g_mutex1);
    if (count > 0) {
        printf("Device 2 (Bus 1) received %d bytes: ", count);
        for (uint16_t i = 0; i < count; i++) {
            printf("%02X ", buffer[i]);
        }
        printf("\n");
    }
    return count;
}

// 设备2的第二条总线发送函数（写入共享缓冲区3，设备2 -> 设备3）
void device2_bus2_send(uint8_t* data, uint16_t length) {
    pthread_mutex_lock(&g_mutex3);
    for (uint16_t i = 0; i < length; i++) {
        uint16_t next_head = (g_shared_buffer3_head + 1) % SHARED_BUFFER_SIZE;
        if (next_head != g_shared_buffer3_tail) {
            g_shared_buffer3[g_shared_buffer3_head] = data[i];
            g_shared_buffer3_head = next_head;
        } else {
            printf("Device 2 (Bus 2): Buffer overflow, dropping data\n");
            break;
        }
    }
    pthread_mutex_unlock(&g_mutex3);
    printf("Device 2 (Bus 2) sent: ");
    for (uint16_t i = 0; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
}

// 设备2的第二条总线接收函数（从共享缓冲区4读取，设备3 -> 设备2）
uint16_t device2_bus2_receive(uint8_t* buffer, uint16_t size) {
    uint16_t count = 0;
    pthread_mutex_lock(&g_mutex4);
    while (g_shared_buffer4_tail != g_shared_buffer4_head && count < size) {
        buffer[count++] = g_shared_buffer4[g_shared_buffer4_tail];
        g_shared_buffer4_tail = (g_shared_buffer4_tail + 1) % SHARED_BUFFER_SIZE;
    }
    pthread_mutex_unlock(&g_mutex4);
    if (count > 0) {
        printf("Device 2 (Bus 2) received %d bytes: ", count);
        for (uint16_t i = 0; i < count; i++) {
            printf("%02X ", buffer[i]);
        }
        printf("\n");
    }
    return count;
}

// 设备3的总线发送函数（写入共享缓冲区4，设备3 -> 设备2）
void device3_bus_send(uint8_t* data, uint16_t length) {
    pthread_mutex_lock(&g_mutex4);
    for (uint16_t i = 0; i < length; i++) {
        uint16_t next_head = (g_shared_buffer4_head + 1) % SHARED_BUFFER_SIZE;
        if (next_head != g_shared_buffer4_tail) {
            g_shared_buffer4[g_shared_buffer4_head] = data[i];
            g_shared_buffer4_head = next_head;
        } else {
            printf("Device 3: Buffer overflow, dropping data\n");
            break;
        }
    }
    pthread_mutex_unlock(&g_mutex4);
    printf("Device 3 sent: ");
    for (uint16_t i = 0; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
}

// 设备3的总线接收函数（从共享缓冲区3读取，设备2 -> 设备3）
uint16_t device3_bus_receive(uint8_t* buffer, uint16_t size) {
    uint16_t count = 0;
    pthread_mutex_lock(&g_mutex3);
    while (g_shared_buffer3_tail != g_shared_buffer3_head && count < size) {
        buffer[count++] = g_shared_buffer3[g_shared_buffer3_tail];
        g_shared_buffer3_tail = (g_shared_buffer3_tail + 1) % SHARED_BUFFER_SIZE;
    }
    pthread_mutex_unlock(&g_mutex3);
    if (count > 0) {
        printf("Device 3 received %d bytes: ", count);
        for (uint16_t i = 0; i < count; i++) {
            printf("%02X ", buffer[i]);
        }
        printf("\n");
    }
    return count;
}

// 内存分配函数
void* mock_malloc(size_t size) {
    return malloc(size);
}

// 内存释放函数
void mock_free(void* ptr) {
    free(ptr);
}

// 时间戳函数声明
uint32_t mock_get_timestamp(void);

// 全局线程数据指针，用于信号函数访问
// 使用线程局部存储来存储当前线程数据，避免线程安全问题
static __thread thread_data_t* g_current_thread_data = NULL;

// 设备1信号等待函数
static eproto_signal_result_t device1_signal_wait(uint32_t timestamp) {
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
static void device1_signal_send(void) {
    if (g_current_thread_data && g_current_thread_data->semaphore_initialized) {
        sem_post(&g_current_thread_data->semaphore);
    }
}

// 设备2信号等待函数（模拟裸机环境）
static eproto_signal_result_t device2_signal_wait(uint32_t timestamp) {
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
static void device2_signal_send(void) {
    if (g_current_thread_data) {
        // 设置信号标志
        g_current_thread_data->signal_flag = 1;
    }
}

// 设备3信号等待函数
static eproto_signal_result_t device3_signal_wait(uint32_t timestamp) {
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
static void device3_signal_send(void) {
    if (g_current_thread_data && g_current_thread_data->semaphore_initialized) {
        sem_post(&g_current_thread_data->semaphore);
    }
}

// 唤醒函数
void mock_wakeup(void) {
    printf("Waking up...\n");
}

// 状态回调函数
void mock_status_callback(eproto_status_t status, uint8_t* data, uint16_t length) {
    (void)data;
    (void)length;
    switch (status) {
        case EPROTO_STATUS_CRC_ERROR:
            printf("Status: CRC error\n");
            break;
        case EPROTO_STATUS_SLEEP_SUCCESS:
            printf("Status: Sleep success\n");
            break;
        case EPROTO_STATUS_SLEEP_FAILED:
            printf("Status: Sleep failed\n");
            break;
        case EPROTO_STATUS_WAKEUP_SUCCESS:
            printf("Status: Wakeup success\n");
            break;
        case EPROTO_STATUS_HANDSHAKE_SUCCESS:
            printf("Status: Handshake success\n");
            break;
        case EPROTO_STATUS_WAKEUP_FAILED:
            printf("Status: Wakeup failed\n");
            break;
        case EPROTO_STATUS_MULTIPLE_CRC_ERRORS:
            printf("Status: Multiple CRC errors\n");
            break;
    }
}

// 设备1的接收回调函数
void device1_receive_callback(uint8_t source_address, uint16_t packet_id, uint8_t* data, uint16_t length) {
    printf("Device 1: Received data from device %02X, packet ID: %d: ", source_address, packet_id);
    for (uint16_t i = 0; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
}

// 全局变量用于存储设备2的eproto实例
static eproto_t* g_device2_eproto = NULL;

// 设备2的接收回调函数
void device2_receive_callback(uint8_t source_address, uint16_t packet_id, uint8_t* data, uint16_t length) {
    printf("Device 2: Received data from device %02X, packet ID: %d: ", source_address, packet_id);
    for (uint16_t i = 0; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");

    // 发送回复数据给设备1
    uint8_t reply_data[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
    printf("Device 2: Sending reply data to device 1 with packet ID: %d...\n", packet_id);

    // 使用全局变量获取eproto实例
    if (g_device2_eproto) {
        eproto_error_t error =
            eproto_send_user_reply(g_device2_eproto, source_address, packet_id, reply_data, sizeof(reply_data));
        if (error != EPROTO_OK) {
            printf("Device 2: Failed to send reply\n");
        } else {
            printf("Device 2: Reply sent successfully\n");
        }
    } else {
        printf("Device 2: eproto instance not initialized\n");
    }
}

// 锁函数
void mock_lock(void) {
}

// 解锁函数
void mock_unlock(void) {
}

// 获取时间戳函数（线程安全）
uint32_t mock_get_timestamp(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

// 设备1的发送回调函数
void device1_send_callback(eproto_send_status_t status, uint16_t packet_id, uint8_t* data, uint16_t length,
                           void* private_data) {
    (void)private_data;
    switch (status) {
        case EPROTO_SEND_SUCCESS:
            printf("Device 1: Send success, packet ID: %d\n", packet_id);
            if (data && length > 0) {
                printf("Device 1: Received response: ");
                for (uint16_t i = 0; i < length; i++) {
                    printf("%02X ", data[i]);
                }
                printf("\n");
            }
            break;
        case EPROTO_SEND_TIMEOUT:
            printf("Device 1: Send timeout, packet ID: %d\n", packet_id);
            break;
        case EPROTO_SEND_ERROR:
            printf("Device 1: Send error, packet ID: %d\n", packet_id);
            break;
        case EPROTO_SEND_BUSY:
            printf("Device 1: Send busy, packet ID: %d\n", packet_id);
            break;
    }
}

// 设备2的发送回调函数
void device2_send_callback(eproto_send_status_t status, uint16_t packet_id, uint8_t* data, uint16_t length,
                           void* private_data) {
    (void)private_data;
    switch (status) {
        case EPROTO_SEND_SUCCESS:
            printf("Device 2: Send success, packet ID: %d\n", packet_id);
            if (data && length > 0) {
                printf("Device 2: Received response: ");
                for (uint16_t i = 0; i < length; i++) {
                    printf("%02X ", data[i]);
                }
                printf("\n");
            }
            break;
        case EPROTO_SEND_TIMEOUT:
            printf("Device 2: Send timeout, packet ID: %d\n", packet_id);
            break;
        case EPROTO_SEND_ERROR:
            printf("Device 2: Send error, packet ID: %d\n", packet_id);
            break;
        case EPROTO_SEND_BUSY:
            printf("Device 2: Send busy, packet ID: %d\n", packet_id);
            break;
    }
}

// 全局变量用于存储设备3的eproto实例
static eproto_t* g_device3_eproto = NULL;

// 设备3的接收回调函数
void device3_receive_callback(uint8_t source_address, uint16_t packet_id, uint8_t* data, uint16_t length) {
    printf("Device 3: Received data from device %02X, packet ID: %d: ", source_address, packet_id);
    for (uint16_t i = 0; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");

    // 发送回复数据给设备1（通过设备2转发）
    uint8_t reply_data[] = {0xFF, 0xEE, 0xDD, 0xCC, 0xBB};
    printf("Device 3: Sending reply data to device %02X with packet ID: %d...\n", source_address, packet_id);

    // 使用全局变量获取eproto实例
    if (g_device3_eproto) {
        eproto_error_t error =
            eproto_send_user_reply(g_device3_eproto, source_address, packet_id, reply_data, sizeof(reply_data));
        if (error != EPROTO_OK) {
            printf("Device 3: Failed to send reply\n");
        } else {
            printf("Device 3: Reply sent successfully\n");
        }
    } else {
        printf("Device 3: eproto instance not initialized\n");
    }
}

// 设备3的发送回调函数
void device3_send_callback(eproto_send_status_t status, uint16_t packet_id, uint8_t* data, uint16_t length,
                           void* private_data) {
    (void)private_data;
    switch (status) {
        case EPROTO_SEND_SUCCESS:
            printf("Device 3: Send success, packet ID: %d\n", packet_id);
            if (data && length > 0) {
                printf("Device 3: Received response: ");
                for (uint16_t i = 0; i < length; i++) {
                    printf("%02X ", data[i]);
                }
                printf("\n");
            }
            break;
        case EPROTO_SEND_TIMEOUT:
            printf("Device 3: Send timeout, packet ID: %d\n", packet_id);
            break;
        case EPROTO_SEND_ERROR:
            printf("Device 3: Send error, packet ID: %d\n", packet_id);
            break;
        case EPROTO_SEND_BUSY:
            printf("Device 3: Send busy, packet ID: %d\n", packet_id);
            break;
    }
}

// 设备1接收线程
void* device1_receive_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    printf("%s receive thread started\n", data->device_name);

    // 设置当前线程数据
    g_current_thread_data = data;

    // 定期接收数据
    for (int i = 0; i < 50; i++) {
        // 模拟从总线接收数据
        uint8_t rx_buffer[256];
        uint16_t rx_count = device1_bus_receive(rx_buffer, sizeof(rx_buffer));
        for (uint16_t j = 0; j < rx_count; j++) {
            eproto_receive_byte(&data->eproto_inst, data->device_address, rx_buffer[j]);
        }
        usleep(50000);
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
    usleep(100000);

    // 发送需要回复的测试数据（no_wait=0）
    uint8_t test_data[] = {0x11, 0x22, 0x33, 0x44, 0x55};
    printf("%s: Sending test data (needs reply)...\n", data->device_name);
    eproto_error_t error =
        eproto_send(&data->eproto_inst, 0x02, test_data, sizeof(test_data), device1_send_callback, data, 0);
    if (error != EPROTO_OK) {
        printf("%s: Failed to send data\n", data->device_name);
        pthread_exit(NULL);
    }
    printf("%s: Data sent successfully\n", data->device_name);

    // 等待一段时间，确保前面的数据包处理完成
    usleep(100000);

    // 发送不需要回复的测试数据（no_wait=1）
    uint8_t test_data_no_wait[] = {0x66, 0x77, 0x88, 0x99, 0xAA};
    printf("%s: Sending test data (no reply needed)...\n", data->device_name);
    error = eproto_send(&data->eproto_inst, 0x02, test_data_no_wait, sizeof(test_data_no_wait), device1_send_callback,
                        data, 1);
    if (error != EPROTO_OK) {
        printf("%s: Failed to send data\n", data->device_name);
        pthread_exit(NULL);
    }
    printf("%s: Data sent successfully\n", data->device_name);

    // 发送数据给设备3（需要通过设备2转发）
    uint8_t test_data_to_3[] = {0x33, 0x44, 0x55, 0x66, 0x77};
    printf("%s: Sending test data to device 3 (via device 2 forwarding)...\n", data->device_name);
    error =
        eproto_send(&data->eproto_inst, 0x03, test_data_to_3, sizeof(test_data_to_3), device1_send_callback, data, 0);
    if (error != EPROTO_OK) {
        printf("%s: Failed to send data to device 3\n", data->device_name);
        pthread_exit(NULL);
    }
    printf("%s: Data sent to device 3 successfully\n", data->device_name);

    // 定期处理协议
    for (int i = 0; i < 50; i++) {
        eproto_tick(&data->eproto_inst);
        usleep(50000);
    }

    // 销毁eProto
    eproto_destroy(&data->eproto_inst);

    printf("%s process thread finished\n", data->device_name);
    pthread_exit(NULL);
}

// 设备1线程
void* device1_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    printf("%s thread started\n", data->device_name);

    // 初始化信号量
    data->semaphore_initialized = 0;

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

    // 定义总线接口
    eproto_bus_t device1_bus = {.send = device1_bus_send, .receive = device1_bus_receive};

    // 创建接收缓冲区
    uint8_t device1_rx_buffer[256];
    // 添加路由
    error = eproto_add_bus(&data->eproto_inst, data->device_address, &device1_bus, device1_rx_buffer,
                           sizeof(device1_rx_buffer), "device1_bus", mock_wakeup, mock_status_callback,
                           device1_receive_callback);
    if (error != EPROTO_OK) {
        printf("%s: Failed to add route\n", data->device_name);
        pthread_exit(NULL);
    }
    printf("%s: Route added successfully\n", data->device_name);

    // 添加目标设备地址
    error = eproto_add_destination_device(&data->eproto_inst, data->device_address, 0x02);
    if (error != EPROTO_OK) {
        printf("%s: Failed to add target device\n", data->device_name);
        pthread_exit(NULL);
    }
    printf("%s: Target device 0x02 added successfully\n", data->device_name);

    // 添加设备3作为目标设备
    error = eproto_add_destination_device(&data->eproto_inst, data->device_address, 0x03);
    if (error != EPROTO_OK) {
        printf("%s: Failed to add target device 0x03\n", data->device_name);
        pthread_exit(NULL);
    }
    printf("%s: Target device 0x03 added successfully\n", data->device_name);

    // 创建接收线程和处理线程
    pthread_t receive_thread, process_thread;
    thread_data_t receive_data = *data;
    receive_data.thread_type = THREAD_TYPE_RECEIVE;
    thread_data_t process_data = *data;
    process_data.thread_type = THREAD_TYPE_PROCESS;

    // 创建接收线程
    if (pthread_create(&receive_thread, NULL, device1_receive_thread, &receive_data) != 0) {
        printf("%s: Failed to create receive thread\n", data->device_name);
        pthread_exit(NULL);
    }

    // 创建处理线程
    if (pthread_create(&process_thread, NULL, device1_process_thread, &process_data) != 0) {
        printf("%s: Failed to create process thread\n", data->device_name);
        pthread_exit(NULL);
    }

    // 等待线程完成
    pthread_join(receive_thread, NULL);
    pthread_join(process_thread, NULL);

    printf("%s thread finished\n", data->device_name);
    pthread_exit(NULL);
}

// 设备2接收线程
void* device2_receive_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    printf("%s receive thread started\n", data->device_name);

    // 设置当前线程数据
    g_current_thread_data = data;

    // 定期接收数据
    for (int i = 0; i < 50; i++) {
        // 模拟从第一条总线接收数据
        uint8_t rx_buffer1[256];
        uint16_t rx_count1 = device2_bus_receive(rx_buffer1, sizeof(rx_buffer1));
        for (uint16_t j = 0; j < rx_count1; j++) {
            eproto_receive_byte(&data->eproto_inst, 0x02, rx_buffer1[j]);
        }

        // 模拟从第二条总线接收数据
        uint8_t rx_buffer2[256];
        uint16_t rx_count2 = device2_bus2_receive(rx_buffer2, sizeof(rx_buffer2));
        for (uint16_t j = 0; j < rx_count2; j++) {
            eproto_receive_byte(&data->eproto_inst, 0x04, rx_buffer2[j]);
        }
        usleep(50000);
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
    for (int i = 0; i < 50; i++) {
        eproto_tick(&data->eproto_inst);
        usleep(50000);
    }

    // 销毁eProto
    eproto_destroy(&data->eproto_inst);

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

    // 定义总线接口
    eproto_bus_t device2_bus = {.send = device2_bus_send, .receive = device2_bus_receive};

    // 创建接收缓冲区
    uint8_t device2_rx_buffer[256];
    // 添加路由
    error = eproto_add_bus(&data->eproto_inst, data->device_address, &device2_bus, device2_rx_buffer,
                           sizeof(device2_rx_buffer), "device2_bus", mock_wakeup, mock_status_callback,
                           device2_receive_callback);
    if (error != EPROTO_OK) {
        printf("%s: Failed to add route\n", data->device_name);
        pthread_exit(NULL);
    }
    printf("%s: Route added successfully\n", data->device_name);

    // 添加目标设备地址
    error = eproto_add_destination_device(&data->eproto_inst, data->device_address, 0x01);
    if (error != EPROTO_OK) {
        printf("%s: Failed to add target device\n", data->device_name);
        pthread_exit(NULL);
    }
    printf("%s: Target device 0x01 added successfully\n", data->device_name);

    // 定义第二条总线接口（设备2 -> 设备3）
    eproto_bus_t device2_bus2 = {.send = device2_bus2_send, .receive = device2_bus2_receive};

    // 创建第二条总线的接收缓冲区
    uint8_t device2_bus2_rx_buffer[256];
    // 添加第二条总线（使用不同的self_address 0x04）
    error =
        eproto_add_bus(&data->eproto_inst, 0x04, &device2_bus2, device2_bus2_rx_buffer, sizeof(device2_bus2_rx_buffer),
                       "device2_bus2", mock_wakeup, mock_status_callback, device2_receive_callback);
    if (error != EPROTO_OK) {
        printf("%s: Failed to add second bus\n", data->device_name);
        pthread_exit(NULL);
    }
    printf("%s: Second bus added successfully\n", data->device_name);

    // 添加设备3作为目标设备到第二条总线
    error = eproto_add_destination_device(&data->eproto_inst, 0x04, 0x03);
    if (error != EPROTO_OK) {
        printf("%s: Failed to add target device 0x03\n", data->device_name);
        pthread_exit(NULL);
    }
    printf("%s: Target device 0x03 added successfully\n", data->device_name);

    // 创建接收线程和处理线程
    pthread_t receive_thread, process_thread;
    thread_data_t receive_data = *data;
    receive_data.thread_type = THREAD_TYPE_RECEIVE;
    thread_data_t process_data = *data;
    process_data.thread_type = THREAD_TYPE_PROCESS;

    // 创建接收线程
    if (pthread_create(&receive_thread, NULL, device2_receive_thread, &receive_data) != 0) {
        printf("%s: Failed to create receive thread\n", data->device_name);
        pthread_exit(NULL);
    }

    // 创建处理线程
    if (pthread_create(&process_thread, NULL, device2_process_thread, &process_data) != 0) {
        printf("%s: Failed to create process thread\n", data->device_name);
        pthread_exit(NULL);
    }

    // 等待线程完成
    pthread_join(receive_thread, NULL);
    pthread_join(process_thread, NULL);

    printf("%s thread finished\n", data->device_name);
    pthread_exit(NULL);
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
        for (uint16_t j = 0; j < rx_count; j++) {
            eproto_receive_byte(&data->eproto_inst, data->device_address, rx_buffer[j]);
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
        eproto_tick(&data->eproto_inst);
        usleep(50000);
    }

    // 销毁eProto
    eproto_destroy(&data->eproto_inst);

    printf("%s process thread finished\n", data->device_name);
    pthread_exit(NULL);
}

// 设备3线程
void* device3_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    printf("%s thread started\n", data->device_name);

    // 初始化信号量
    data->semaphore_initialized = 0;

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

    // 创建接收缓冲区
    uint8_t device3_rx_buffer[256];
    // 添加路由
    error = eproto_add_bus(&data->eproto_inst, data->device_address, &device3_bus, device3_rx_buffer,
                           sizeof(device3_rx_buffer), "device3_bus", mock_wakeup, mock_status_callback,
                           device3_receive_callback);
    if (error != EPROTO_OK) {
        printf("%s: Failed to add route\n", data->device_name);
        pthread_exit(NULL);
    }
    printf("%s: Route added successfully\n", data->device_name);

    // 添加目标设备地址
    error = eproto_add_destination_device(&data->eproto_inst, data->device_address, 0x02);
    if (error != EPROTO_OK) {
        printf("%s: Failed to add target device\n", data->device_name);
        pthread_exit(NULL);
    }
    printf("%s: Target device 0x02 added successfully\n", data->device_name);

    // 添加设备1作为目标设备
    error = eproto_add_destination_device(&data->eproto_inst, data->device_address, 0x01);
    if (error != EPROTO_OK) {
        printf("%s: Failed to add target device 0x01\n", data->device_name);
        pthread_exit(NULL);
    }
    printf("%s: Target device 0x01 added successfully\n", data->device_name);

    // 创建接收线程和处理线程
    pthread_t receive_thread, process_thread;
    thread_data_t receive_data = *data;
    receive_data.thread_type = THREAD_TYPE_RECEIVE;
    thread_data_t process_data = *data;
    process_data.thread_type = THREAD_TYPE_PROCESS;

    // 创建接收线程
    if (pthread_create(&receive_thread, NULL, device3_receive_thread, &receive_data) != 0) {
        printf("%s: Failed to create receive thread\n", data->device_name);
        pthread_exit(NULL);
    }

    // 创建处理线程
    if (pthread_create(&process_thread, NULL, device3_process_thread, &process_data) != 0) {
        printf("%s: Failed to create process thread\n", data->device_name);
        pthread_exit(NULL);
    }

    // 等待线程完成
    pthread_join(receive_thread, NULL);
    pthread_join(process_thread, NULL);

    printf("%s thread finished\n", data->device_name);
    pthread_exit(NULL);
}

int main(void) {
    printf("eProto Thread Communication Example\n");
    printf("===================================\n\n");

    pthread_t thread1, thread2, thread3;

    // 初始化共享缓冲区
    memset(g_shared_buffer1, 0, SHARED_BUFFER_SIZE);
    memset(g_shared_buffer2, 0, SHARED_BUFFER_SIZE);
    memset(g_shared_buffer3, 0, SHARED_BUFFER_SIZE);
    memset(g_shared_buffer4, 0, SHARED_BUFFER_SIZE);
    g_shared_buffer1_head = 0;
    g_shared_buffer1_tail = 0;
    g_shared_buffer2_head = 0;
    g_shared_buffer2_tail = 0;
    g_shared_buffer3_head = 0;
    g_shared_buffer3_tail = 0;
    g_shared_buffer4_head = 0;
    g_shared_buffer4_tail = 0;

    // 初始化线程数据
    thread_data_t device1_data = {.device_address = 0x01, .device_name = "Device 1", .timestamp = 0};
    thread_data_t device2_data = {.device_address = 0x02, .device_name = "Device 2", .timestamp = 0};
    thread_data_t device3_data = {.device_address = 0x03, .device_name = "Device 3", .timestamp = 0};

    // 初始化互斥锁
    pthread_mutex_init(&device1_data.timestamp_mutex, NULL);
    pthread_mutex_init(&device2_data.timestamp_mutex, NULL);
    pthread_mutex_init(&device3_data.timestamp_mutex, NULL);

    printf("Shared buffers initialized\n");

    // 创建线程
    printf("Creating device 1 thread...\n");
    if (pthread_create(&thread1, NULL, device1_thread, &device1_data) != 0) {
        printf("Failed to create device 1 thread\n");
        return 1;
    }
    printf("Device 1 thread created\n");

    printf("Creating device 2 thread...\n");
    if (pthread_create(&thread2, NULL, device2_thread, &device2_data) != 0) {
        printf("Failed to create device 2 thread\n");
        return 1;
    }
    printf("Device 2 thread created\n");

    printf("Creating device 3 thread...\n");
    if (pthread_create(&thread3, NULL, device3_thread, &device3_data) != 0) {
        printf("Failed to create device 3 thread\n");
        return 1;
    }
    printf("Device 3 thread created\n");

    // 等待线程完成
    printf("Waiting for threads to complete...\n");
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    pthread_join(thread3, NULL);

    // 销毁互斥锁
    pthread_mutex_destroy(&device1_data.timestamp_mutex);
    pthread_mutex_destroy(&device2_data.timestamp_mutex);
    pthread_mutex_destroy(&device3_data.timestamp_mutex);

    printf("\neProto example completed\n");
    return 0;
}
