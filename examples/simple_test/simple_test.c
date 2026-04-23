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

#include "eproto.h"
#include "fixed_block_allocator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include <semaphore.h>

#define SHARED_BUFFER_SIZE 1024

static uint8_t g_shared_buffer1[SHARED_BUFFER_SIZE];
static uint8_t g_shared_buffer2[SHARED_BUFFER_SIZE];
static uint16_t g_shared_buffer1_head = 0;
static uint16_t g_shared_buffer1_tail = 0;
static uint16_t g_shared_buffer2_head = 0;
static uint16_t g_shared_buffer2_tail = 0;
static pthread_mutex_t g_mutex1 = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_mutex2 = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_eproto_lock = PTHREAD_MUTEX_INITIALIZER;

static eproto_t* g_device1_eproto = NULL;
static eproto_t* g_device2_eproto = NULL;

// 线程类型枚举
typedef enum {
    THREAD_TYPE_RECEIVE,
    THREAD_TYPE_PROCESS
} thread_type_t;

// 线程数据结构
typedef struct {
    uint8_t device_address;
    eproto_t eproto_inst;
    char* device_name;
    uint32_t timestamp;
    pthread_mutex_t timestamp_mutex;
    thread_type_t thread_type;
    sem_t semaphore;
    int semaphore_initialized;
    int signal_flag;
    uint8_t rx_buffer[256];
    uint8_t rx_buffer2[256];
} thread_data_t;

__thread thread_data_t* g_current_thread_data = NULL;

void* mock_malloc(size_t size) {
    return malloc(size);
}

void mock_free(void* ptr) {
    free(ptr);
}

uint32_t mock_get_timestamp(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

void mock_lock(void) {
    pthread_mutex_lock(&g_eproto_lock);
}

void mock_unlock(void) {
    pthread_mutex_unlock(&g_eproto_lock);
}

void mock_wakeup(void) {
}

void mock_status_callback(eproto_status_t status, uint8_t* data, uint16_t length) {
    (void)data;
    (void)length;
    printf("Status callback: status = %d\n", status);
}



eproto_signal_result_t device1_signal_wait(uint32_t timestamp) {
    if (!g_current_thread_data) {
        return EPROTO_SIGNAL_TIMEOUT;
    }

    if (!g_current_thread_data->semaphore_initialized) {
        if (sem_init(&g_current_thread_data->semaphore, 0, 0) != 0) {
            printf("%s: Failed to initialize semaphore\n", g_current_thread_data->device_name);
            return EPROTO_SIGNAL_TIMEOUT;
        }
        g_current_thread_data->semaphore_initialized = 1;
    }

    uint32_t current_time = mock_get_timestamp();
    uint32_t timeout_ms = 0;
    if (timestamp > current_time) {
        timeout_ms = timestamp - current_time;
    }

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
        return EPROTO_SIGNAL_DATA;
    } else {
        return EPROTO_SIGNAL_TIMEOUT;
    }
}

void device1_signal_send(void) {
    if (g_current_thread_data && g_current_thread_data->semaphore_initialized) {
        sem_post(&g_current_thread_data->semaphore);
    }
}

eproto_signal_result_t device2_signal_wait(uint32_t timestamp) {
    if (!g_current_thread_data) {
        return EPROTO_SIGNAL_TIMEOUT;
    }

    if (!g_current_thread_data->semaphore_initialized) {
        if (sem_init(&g_current_thread_data->semaphore, 0, 0) != 0) {
            printf("%s: Failed to initialize semaphore\n", g_current_thread_data->device_name);
            return EPROTO_SIGNAL_TIMEOUT;
        }
        g_current_thread_data->semaphore_initialized = 1;
    }

    uint32_t current_time = mock_get_timestamp();
    uint32_t timeout_ms = 0;
    if (timestamp > current_time) {
        timeout_ms = timestamp - current_time;
    }

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
        return EPROTO_SIGNAL_DATA;
    } else {
        return EPROTO_SIGNAL_TIMEOUT;
    }
}

void device2_signal_send(void) {
    if (g_current_thread_data && g_current_thread_data->semaphore_initialized) {
        sem_post(&g_current_thread_data->semaphore);
    }
}

void device1_bus_send(uint8_t* data, uint16_t length) {
    pthread_mutex_lock(&g_mutex1);
    for (uint16_t i = 0; i < length; i++) {
        uint16_t next_head = (g_shared_buffer1_head + 1) % SHARED_BUFFER_SIZE;
        if (next_head != g_shared_buffer1_tail) {
            g_shared_buffer1[g_shared_buffer1_head] = data[i];
            g_shared_buffer1_head = next_head;
        } else {
            printf("Device 1: Buffer overflow\n");
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

void device2_bus_send(uint8_t* data, uint16_t length) {
    pthread_mutex_lock(&g_mutex2);
    for (uint16_t i = 0; i < length; i++) {
        uint16_t next_head = (g_shared_buffer2_head + 1) % SHARED_BUFFER_SIZE;
        if (next_head != g_shared_buffer2_tail) {
            g_shared_buffer2[g_shared_buffer2_head] = data[i];
            g_shared_buffer2_head = next_head;
        } else {
            printf("Device 2: Buffer overflow\n");
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

uint16_t device2_bus_receive(uint8_t* buffer, uint16_t size) {
    uint16_t count = 0;
    pthread_mutex_lock(&g_mutex1);
    while (g_shared_buffer1_tail != g_shared_buffer1_head && count < size) {
        buffer[count++] = g_shared_buffer1[g_shared_buffer1_tail];
        g_shared_buffer1_tail = (g_shared_buffer1_tail + 1) % SHARED_BUFFER_SIZE;
    }
    pthread_mutex_unlock(&g_mutex1);
    if (count > 0) {
        printf("Device 2 received %d bytes: ", count);
        for (uint16_t i = 0; i < count; i++) {
            printf("%02X ", buffer[i]);
        }
        printf("\n");
    }
    return count;
}

void device1_receive_callback(uint8_t source_address, uint16_t packet_id, uint8_t* data, uint16_t length) {
    printf("Device 1 received data from device 0x%02X, packet ID: %d: ", source_address, packet_id);
    for (uint16_t i = 0; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");

    printf("Device 1: Sending reply...\n");
    eproto_error_t error = eproto_send_user_reply(g_device1_eproto, source_address, packet_id, data, length);
    if (error != EPROTO_OK) {
        printf("Device 1: Failed to send reply\n");
    } else {
        printf("Device 1: Reply sent successfully\n");
    }
}

void device2_receive_callback(uint8_t source_address, uint16_t packet_id, uint8_t* data, uint16_t length) {
    printf("Device 2 received data from device 0x%02X, packet ID: %d: ", source_address, packet_id);
    for (uint16_t i = 0; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");

    printf("Device 2: Sending reply...\n");
    eproto_error_t error = eproto_send_user_reply(g_device2_eproto, source_address, packet_id, data, length);
    if (error != EPROTO_OK) {
        printf("Device 2: Failed to send reply\n");
    } else {
        printf("Device 2: Reply sent successfully\n");
    }
}

void device1_send_callback(eproto_send_status_t status, uint16_t packet_id, uint8_t* data, uint16_t length,
                           void* private_data) {
    (void)data;
    (void)length;
    (void)private_data;
    switch (status) {
        case EPROTO_SEND_SUCCESS:
            printf("Device 1: Send success, packet ID: %d\n", packet_id);
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

// 设备1接收线程
void* device1_receive_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    printf("%s receive thread started\n", data->device_name);
    fflush(stdout);

    g_current_thread_data = data;

    for (int i = 0; i < 50; i++) {
        uint8_t rx_buffer[256];
        uint16_t rx_count = device1_bus_receive(rx_buffer, sizeof(rx_buffer));
        if (rx_count > 0) {
            eproto_receive_data(&data->eproto_inst, 0x01, rx_buffer, rx_count);
        }
        usleep(50000);
    }

    printf("%s receive thread finished\n", data->device_name);
    fflush(stdout);
    pthread_exit(NULL);
}

// 设备1处理线程
void* device1_process_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    printf("%s process thread started\n", data->device_name);
    fflush(stdout);

    g_current_thread_data = data;

    usleep(100000);

    uint8_t test_data[] = {0x11, 0x22, 0x33, 0x44, 0x55};
    printf("%s: Sending test data (needs reply)...\n", data->device_name);
    fflush(stdout);
    eproto_error_t error =
        eproto_send(&data->eproto_inst, 0x02, test_data, sizeof(test_data), device1_send_callback, NULL, 1);
    if (error != EPROTO_OK) {
        printf("%s: Failed to send data\n", data->device_name);
        fflush(stdout);
        pthread_exit(NULL);
    }
    printf("%s: Data sent successfully\n", data->device_name);
    fflush(stdout);

    usleep(100000);

    uint8_t broadcast_data[] = {0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    printf("%s: Sending broadcast data to all devices...\n", data->device_name);
    fflush(stdout);
    error = eproto_send_ex(&data->eproto_inst, 0xFF, broadcast_data, sizeof(broadcast_data), device1_send_callback, NULL, 0, 0, 0);
    if (error != EPROTO_OK) {
        printf("%s: Failed to send broadcast data\n", data->device_name);
        fflush(stdout);
        pthread_exit(NULL);
    }
    printf("%s: Broadcast data sent successfully\n", data->device_name);
    fflush(stdout);

    for (int i = 0; i < 50; i++) {
        eproto_process(&data->eproto_inst);
        usleep(50000);
    }

    printf("%s process thread finished\n", data->device_name);
    fflush(stdout);
    pthread_exit(NULL);
}

void* device1_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    printf("%s thread started\n", data->device_name);
    fflush(stdout);

    fixed_block_allocator_init();

    eproto_user_functions_t user_functions = {.malloc = fixed_block_alloc,
                                              .free = fixed_block_free,
                                              .signal_wait = device1_signal_wait,
                                              .signal_send = device1_signal_send,
                                              .lock = mock_lock,
                                              .unlock = mock_unlock,
                                              .get_timestamp = mock_get_timestamp,
                                              .timeout_timestamp = 0};

    eproto_error_t error = eproto_init(&data->eproto_inst, &user_functions);
    if (error != EPROTO_OK) {
        printf("%s: Failed to initialize eProto\n", data->device_name);
        fflush(stdout);
        pthread_exit(NULL);
    }
    printf("%s: eProto initialized successfully\n", data->device_name);
    fflush(stdout);
    g_device1_eproto = &data->eproto_inst;

    eproto_bus_config_t bus_config = {
        .self_addr = 0x01,
        .send = device1_bus_send,
        .rx_buffer = data->rx_buffer,
        .rx_buffer_size = sizeof(data->rx_buffer),
        .name = "device1_bus",
        .status_callback = mock_status_callback,
        .receive_callback = device1_receive_callback,
        .forward_callback = NULL
    };
    error = eproto_add_bus(&data->eproto_inst, &bus_config);
    if (error != EPROTO_OK) {
        printf("%s: Failed to add bus\n", data->device_name);
        fflush(stdout);
        pthread_exit(NULL);
    }
    printf("%s: Bus added successfully\n", data->device_name);
    fflush(stdout);

    error = eproto_add_destination_device(&data->eproto_inst, 0x01, 0x02);
    if (error != EPROTO_OK) {
        printf("%s: Failed to add destination device\n", data->device_name);
        fflush(stdout);
        pthread_exit(NULL);
    }
    printf("%s: Destination device 0x02 added successfully\n", data->device_name);
    fflush(stdout);

    pthread_t receive_thread, process_thread;

    data->thread_type = THREAD_TYPE_RECEIVE;

    if (pthread_create(&receive_thread, NULL, device1_receive_thread, data) != 0) {
        printf("%s: Failed to create receive thread\n", data->device_name);
        pthread_exit(NULL);
    }

    data->thread_type = THREAD_TYPE_PROCESS;

    if (pthread_create(&process_thread, NULL, device1_process_thread, data) != 0) {
        printf("%s: Failed to create process thread\n", data->device_name);
        pthread_exit(NULL);
    }

    pthread_join(receive_thread, NULL);
    pthread_join(process_thread, NULL);

    eproto_destroy(&data->eproto_inst);

    printf("%s thread finished\n", data->device_name);
    pthread_exit(NULL);
}

// 设备2接收线程
void* device2_receive_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    printf("%s receive thread started\n", data->device_name);
    fflush(stdout);

    g_current_thread_data = data;

    for (int i = 0; i < 50; i++) {
        uint8_t rx_buffer[256];
        uint16_t rx_count = device2_bus_receive(rx_buffer, sizeof(rx_buffer));
        if (rx_count > 0) {
            eproto_receive_data(&data->eproto_inst, 0x02, rx_buffer, rx_count);
        }
        usleep(50000);
    }

    printf("%s receive thread finished\n", data->device_name);
    fflush(stdout);
    pthread_exit(NULL);
}

// 设备2处理线程
void* device2_process_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    printf("%s process thread started\n", data->device_name);
    fflush(stdout);

    g_current_thread_data = data;

    for (int i = 0; i < 50; i++) {
        eproto_process(&data->eproto_inst);
        usleep(50000);
    }

    printf("%s process thread finished\n", data->device_name);
    fflush(stdout);
    pthread_exit(NULL);
}

void* device2_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    printf("%s thread started\n", data->device_name);
    fflush(stdout);

    eproto_user_functions_t user_functions = {.malloc = mock_malloc,
                                              .free = mock_free,
                                              .signal_wait = device2_signal_wait,
                                              .signal_send = device2_signal_send,
                                              .lock = mock_lock,
                                              .unlock = mock_unlock,
                                              .get_timestamp = mock_get_timestamp,
                                              .timeout_timestamp = 0};

    eproto_error_t error = eproto_init(&data->eproto_inst, &user_functions);
    if (error != EPROTO_OK) {
        printf("%s: Failed to initialize eProto\n", data->device_name);
        fflush(stdout);
        pthread_exit(NULL);
    }
    printf("%s: eProto initialized successfully\n", data->device_name);
    fflush(stdout);
    g_device2_eproto = &data->eproto_inst;

    eproto_bus_config_t bus_config = {
        .self_addr = 0x02,
        .send = device2_bus_send,
        .rx_buffer = data->rx_buffer,
        .rx_buffer_size = sizeof(data->rx_buffer),
        .name = "device2_bus",
        .status_callback = mock_status_callback,
        .receive_callback = device2_receive_callback,
        .forward_callback = NULL
    };
    error = eproto_add_bus(&data->eproto_inst, &bus_config);
    if (error != EPROTO_OK) {
        printf("%s: Failed to add bus\n", data->device_name);
        fflush(stdout);
        pthread_exit(NULL);
    }
    printf("%s: Bus added successfully\n", data->device_name);
    fflush(stdout);

    error = eproto_add_destination_device(&data->eproto_inst, 0x02, 0x01);
    if (error != EPROTO_OK) {
        printf("%s: Failed to add destination device\n", data->device_name);
        fflush(stdout);
        pthread_exit(NULL);
    }
    printf("%s: Destination device 0x01 added successfully\n", data->device_name);
    fflush(stdout);

    pthread_t receive_thread, process_thread;

    data->thread_type = THREAD_TYPE_RECEIVE;

    if (pthread_create(&receive_thread, NULL, device2_receive_thread, data) != 0) {
        printf("%s: Failed to create receive thread\n", data->device_name);
        pthread_exit(NULL);
    }

    data->thread_type = THREAD_TYPE_PROCESS;

    if (pthread_create(&process_thread, NULL, device2_process_thread, data) != 0) {
        printf("%s: Failed to create process thread\n", data->device_name);
        pthread_exit(NULL);
    }

    pthread_join(receive_thread, NULL);
    pthread_join(process_thread, NULL);

    eproto_destroy(&data->eproto_inst);

    printf("%s thread finished\n", data->device_name);
    pthread_exit(NULL);
}

int main() {
    printf("Simple Test: Two devices direct communication\n");
    printf("=============================================\n\n");
    fflush(stdout);

    thread_data_t* device1_data = (thread_data_t*)malloc(sizeof(thread_data_t));
    if (!device1_data) {
        printf("Failed to allocate memory for device 1 data\n");
        return 1;
    }
    memset(device1_data, 0, sizeof(thread_data_t));
    device1_data->device_address = 0x01;
    device1_data->device_name = "Device 1";

    thread_data_t* device2_data = (thread_data_t*)malloc(sizeof(thread_data_t));
    if (!device2_data) {
        printf("Failed to allocate memory for device 2 data\n");
        free(device1_data);
        return 1;
    }
    memset(device2_data, 0, sizeof(thread_data_t));
    device2_data->device_address = 0x02;
    device2_data->device_name = "Device 2";

    pthread_t thread1, thread2;

    if (pthread_create(&thread1, NULL, device1_thread, device1_data) != 0) {
        printf("Failed to create device 1 thread\n");
        free(device1_data);
        free(device2_data);
        return 1;
    }

    if (pthread_create(&thread2, NULL, device2_thread, device2_data) != 0) {
        printf("Failed to create device 2 thread\n");
        free(device1_data);
        free(device2_data);
        return 1;
    }

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    free(device1_data);
    free(device2_data);

    printf("\nAll tests completed\n");
    return 0;
}
