#include "eproto.h"
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

static eproto_t* g_device1_eproto = NULL;
static eproto_t* g_device2_eproto = NULL;
static sem_t g_device1_semaphore;
static int g_device1_semaphore_initialized = 0;
static sem_t g_device2_semaphore;
static int g_device2_semaphore_initialized = 0;

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
}

void mock_unlock(void) {
}

void mock_wakeup(void) {
}

void mock_status_callback(eproto_status_t status, uint8_t* data, uint16_t length) {
    (void)data;
    (void)length;
    printf("Status callback: status = %d\n", status);
}

eproto_signal_result_t device1_signal_wait(uint32_t timestamp) {
    if (!g_device1_semaphore_initialized) {
        if (sem_init(&g_device1_semaphore, 0, 0) != 0) {
            printf("Device 1: Failed to initialize semaphore\n");
            return EPROTO_SIGNAL_TIMEOUT;
        }
        g_device1_semaphore_initialized = 1;
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

    int result = sem_timedwait(&g_device1_semaphore, &ts);
    if (result == 0) {
        return EPROTO_SIGNAL_DATA;
    } else {
        return EPROTO_SIGNAL_TIMEOUT;
    }
}

void device1_signal_send(void) {
    if (g_device1_semaphore_initialized) {
        sem_post(&g_device1_semaphore);
    }
}

eproto_signal_result_t device2_signal_wait(uint32_t timestamp) {
    if (!g_device2_semaphore_initialized) {
        if (sem_init(&g_device2_semaphore, 0, 0) != 0) {
            printf("Device 2: Failed to initialize semaphore\n");
            return EPROTO_SIGNAL_TIMEOUT;
        }
        g_device2_semaphore_initialized = 1;
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

    int result = sem_timedwait(&g_device2_semaphore, &ts);
    if (result == 0) {
        return EPROTO_SIGNAL_DATA;
    } else {
        return EPROTO_SIGNAL_TIMEOUT;
    }
}

void device2_signal_send(void) {
    if (g_device2_semaphore_initialized) {
        sem_post(&g_device2_semaphore);
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
    eproto_error_t error = eproto_send_user_reply(g_device1_eproto, 0x02, packet_id, data, length);
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
    eproto_error_t error = eproto_send_user_reply(g_device2_eproto, 0x01, packet_id, data, length);
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

void* device1_thread(void* arg) {
    (void)arg;
    printf("Device 1 thread started\n");

    eproto_user_functions_t user_functions = {.malloc = NULL,  // 不提供内存分配接口，使用内部固定块内存分配器
                                              .free = NULL,  // 不提供内存释放接口，使用内部固定块内存分配器
                                              .signal_wait = device1_signal_wait,
                                              .signal_send = device1_signal_send,
                                              .lock = mock_lock,
                                              .unlock = mock_unlock,
                                              .get_timestamp = mock_get_timestamp,
                                              .timeout_timestamp = 0};

    static eproto_t device1_eproto;
    eproto_error_t error = eproto_init(&device1_eproto, &user_functions);
    if (error != EPROTO_OK) {
        printf("Device 1: Failed to initialize eProto\n");
        pthread_exit(NULL);
    }
    printf("Device 1: eProto initialized successfully\n");
    g_device1_eproto = &device1_eproto;

    eproto_bus_t device1_bus = {.send = device1_bus_send, .receive = device1_bus_receive};

    static uint8_t device1_rx_buffer[256];
    error = eproto_add_bus(&device1_eproto, 0x01, &device1_bus, device1_rx_buffer, sizeof(device1_rx_buffer),
                           "device1_bus", mock_wakeup, mock_status_callback, device1_receive_callback);
    if (error != EPROTO_OK) {
        printf("Device 1: Failed to add bus\n");
        pthread_exit(NULL);
    }
    printf("Device 1: Bus added successfully\n");

    error = eproto_add_destination_device(&device1_eproto, 0x01, 0x02);
    if (error != EPROTO_OK) {
        printf("Device 1: Failed to add destination device\n");
        pthread_exit(NULL);
    }
    printf("Device 1: Destination device 0x02 added successfully\n");

    usleep(200000);

    uint8_t test_data[] = {0x11, 0x22, 0x33, 0x44, 0x55};
    printf("Device 1: Sending test data (needs reply)...\n");
    error = eproto_send(&device1_eproto, 0x02, test_data, sizeof(test_data), device1_send_callback, NULL, 0);
    if (error != EPROTO_OK) {
        printf("Device 1: Failed to send data\n");
        pthread_exit(NULL);
    }
    printf("Device 1: Data sent successfully\n");

    for (int i = 0; i < 30; i++) {
        uint8_t rx_buffer[256];
        uint16_t rx_count = device1_bus_receive(rx_buffer, sizeof(rx_buffer));
        for (uint16_t j = 0; j < rx_count; j++) {
            eproto_receive_byte(&device1_eproto, 0x01, rx_buffer[j]);
        }
        eproto_tick(&device1_eproto);
        usleep(50000);
    }

    printf("Device 1 thread finished\n");
    pthread_exit(NULL);
}

void* device2_thread(void* arg) {
    (void)arg;
    printf("Device 2 thread started\n");

    eproto_user_functions_t user_functions = {.malloc = mock_malloc,  // 提供内存分配接口，使用外部内存分配器
                                              .free = mock_free,  // 提供内存释放接口，使用外部内存分配器
                                              .signal_wait = device2_signal_wait,
                                              .signal_send = device2_signal_send,
                                              .lock = mock_lock,
                                              .unlock = mock_unlock,
                                              .get_timestamp = mock_get_timestamp,
                                              .timeout_timestamp = 0};

    static eproto_t device2_eproto;
    eproto_error_t error = eproto_init(&device2_eproto, &user_functions);
    if (error != EPROTO_OK) {
        printf("Device 2: Failed to initialize eProto\n");
        pthread_exit(NULL);
    }
    printf("Device 2: eProto initialized successfully\n");
    g_device2_eproto = &device2_eproto;

    eproto_bus_t device2_bus = {.send = device2_bus_send, .receive = device2_bus_receive};

    static uint8_t device2_rx_buffer[256];
    error = eproto_add_bus(&device2_eproto, 0x02, &device2_bus, device2_rx_buffer, sizeof(device2_rx_buffer),
                           "device2_bus", mock_wakeup, mock_status_callback, device2_receive_callback);
    if (error != EPROTO_OK) {
        printf("Device 2: Failed to add bus\n");
        pthread_exit(NULL);
    }
    printf("Device 2: Bus added successfully\n");

    error = eproto_add_destination_device(&device2_eproto, 0x02, 0x01);
    if (error != EPROTO_OK) {
        printf("Device 2: Failed to add destination device\n");
        pthread_exit(NULL);
    }
    printf("Device 2: Destination device 0x01 added successfully\n");

    for (int i = 0; i < 30; i++) {
        uint8_t rx_buffer[256];
        uint16_t rx_count = device2_bus_receive(rx_buffer, sizeof(rx_buffer));
        for (uint16_t j = 0; j < rx_count; j++) {
            eproto_receive_byte(&device2_eproto, 0x02, rx_buffer[j]);
        }
        eproto_tick(&device2_eproto);
        usleep(50000);
    }

    printf("Device 2 thread finished\n");
    pthread_exit(NULL);
}

int main() {
    printf("Simple Test: Two devices direct communication\n");
    printf("=============================================\n\n");

    pthread_t thread1, thread2;

    if (pthread_create(&thread1, NULL, device1_thread, NULL) != 0) {
        printf("Failed to create device 1 thread\n");
        return 1;
    }

    if (pthread_create(&thread2, NULL, device2_thread, NULL) != 0) {
        printf("Failed to create device 2 thread\n");
        return 1;
    }

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    printf("\nAll tests completed\n");
    return 0;
}
