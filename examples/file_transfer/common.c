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

uint8_t g_shared_buffer1[SHARED_BUFFER_SIZE];
uint8_t g_shared_buffer2[SHARED_BUFFER_SIZE];
uint16_t g_shared_buffer1_head = 0;
uint16_t g_shared_buffer1_tail = 0;
uint16_t g_shared_buffer2_head = 0;
uint16_t g_shared_buffer2_tail = 0;
pthread_mutex_t g_mutex1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t g_mutex2 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t g_eproto_lock = PTHREAD_MUTEX_INITIALIZER;

__thread thread_data_t* g_current_thread_data = NULL;

eproto_t* g_device1_eproto = NULL;
eproto_t* g_device2_eproto = NULL;
eproto_upper_context_t* g_device1_upper_ctx = NULL;
eproto_upper_context_t* g_device2_upper_ctx = NULL;

uint8_t* g_received_file_data = NULL;
uint32_t g_received_file_size = 0;
uint32_t g_received_file_capacity = 0;
pthread_mutex_t g_file_mutex = PTHREAD_MUTEX_INITIALIZER;
int g_transfer_complete = 0;

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

void mock_status_callback(eproto_bus_t* bus, eproto_status_t status, uint8_t* data, uint16_t length) {
    (void)bus;
    (void)data;
    (void)length;
    printf("Status callback: status = %d\n", status);
}

eproto_signal_result_t device_signal_wait(uint32_t timestamp) {
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

void device_signal_send(void) {
    if (g_current_thread_data && g_current_thread_data->semaphore_initialized) {
        sem_post(&g_current_thread_data->semaphore);
    }
}

void device1_bus_send(eproto_bus_t* bus, uint8_t* data, uint16_t length) {
    (void)bus;
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
    printf("Device 1 sent %d bytes\n", length);
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
        printf("Device 1 received %d bytes\n", count);
    }
    return count;
}

void device2_bus_send(eproto_bus_t* bus, uint8_t* data, uint16_t length) {
    (void)bus;
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
    printf("Device 2 sent %d bytes\n", length);
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
        printf("Device 2 received %d bytes\n", count);
    }
    return count;
}

void device_send_callback(eproto_send_status_t status, uint16_t packet_id, uint8_t* data, uint16_t length, void* private_data) {
    (void)data;
    (void)length;
    (void)private_data;
    switch (status) {
        case EPROTO_SEND_SUCCESS:
            printf("  [Send Success] packet ID: %d\n", packet_id);
            break;
        case EPROTO_SEND_TIMEOUT:
            printf("  [Send Timeout] packet ID: %d\n", packet_id);
            break;
        case EPROTO_SEND_ERROR:
            printf("  [Send Error] packet ID: %d\n", packet_id);
            break;
        case EPROTO_SEND_BUSY:
            printf("  [Send Busy] packet ID: %d\n", packet_id);
            break;
    }
}

void upper_file_start_req_callback(uint16_t session_id, uint8_t status, void* user_data) {
    (void)user_data;
    printf("[Callback] File Start Request - Session: %u, Status: %u\n", session_id, status);
}

void upper_file_start_rsp_callback(uint16_t session_id, uint8_t status, void* user_data) {
    (void)user_data;
    printf("[Callback] File Start Response - Session: %u, Status: %u\n", session_id, status);
}

void upper_file_end_callback(uint16_t session_id, uint8_t status, void* user_data) {
    (void)user_data;
    printf("[Callback] File End - Session: %u, Status: %u\n", session_id, status);

    pthread_mutex_lock(&g_file_mutex);
    g_transfer_complete = 1;
    pthread_mutex_unlock(&g_file_mutex);
}

void upper_file_data_callback(uint16_t session_id, uint32_t offset, uint8_t* data, uint16_t length, void* user_data) {
    (void)user_data;
    printf("[Callback] File Data - Session: %u, Offset: %u, Length: %u\n", session_id, offset, length);

    pthread_mutex_lock(&g_file_mutex);

    if (g_received_file_data == NULL) {
        g_received_file_capacity = 8192;
        g_received_file_data = (uint8_t*)malloc(g_received_file_capacity);
        if (!g_received_file_data) {
            pthread_mutex_unlock(&g_file_mutex);
            return;
        }
        g_received_file_size = 0;
    }

    if (offset + length > g_received_file_capacity) {
        uint32_t new_capacity = offset + length + 1024;
        uint8_t* new_data = (uint8_t*)realloc(g_received_file_data, new_capacity);
        if (new_data) {
            g_received_file_data = new_data;
            g_received_file_capacity = new_capacity;
        } else {
            pthread_mutex_unlock(&g_file_mutex);
            return;
        }
    }

    memcpy(&g_received_file_data[offset], data, length);
    if (offset + length > g_received_file_size) {
        g_received_file_size = offset + length;
    }

    pthread_mutex_unlock(&g_file_mutex);
}

void upper_progress_callback(uint16_t session_id, uint32_t transferred, uint32_t total, void* user_data) {
    (void)user_data;
    uint32_t percent = (total > 0) ? (transferred * 100) / total : 0;
    printf("[Progress] Session: %u, %u / %u bytes (%u%%)\n", session_id, transferred, total, percent);
}

void device1_receive_callback(eproto_bus_t* bus, uint8_t source_address, uint16_t packet_id, uint8_t* data, uint16_t length) {
    (void)bus;
    (void)packet_id;
    printf("Device 1 received data from device 0x%02X, length: %d\n", source_address, length);

    if (g_device1_upper_ctx) {
        eproto_upper_packet_t upper_packet;
        eproto_upper_error_t err = eproto_upper_parse_packet(g_device1_upper_ctx, data, length, &upper_packet);
        if (err == EPROTO_UPPER_OK) {
            printf("  Parsed upper protocol: func_code=0x%02X, session_id=%u, seq=%u\n",
                   upper_packet.func_code, upper_packet.session_id, upper_packet.seq_num);
            eproto_upper_handle_packet(g_device1_upper_ctx, source_address, &upper_packet);
        } else {
            printf("  Failed to parse upper protocol: %d\n", err);
        }
    }
}

void device2_receive_callback(eproto_bus_t* bus, uint8_t source_address, uint16_t packet_id, uint8_t* data, uint16_t length) {
    (void)bus;
    (void)packet_id;
    printf("Device 2 received data from device 0x%02X, length: %d\n", source_address, length);

    if (g_device2_upper_ctx) {
        eproto_upper_packet_t upper_packet;
        eproto_upper_error_t err = eproto_upper_parse_packet(g_device2_upper_ctx, data, length, &upper_packet);
        if (err == EPROTO_UPPER_OK) {
            printf("  Parsed upper protocol: func_code=0x%02X, session_id=%u, seq=%u\n",
                   upper_packet.func_code, upper_packet.session_id, upper_packet.seq_num);
            eproto_upper_handle_packet(g_device2_upper_ctx, source_address, &upper_packet);
        } else {
            printf("  Failed to parse upper protocol: %d\n", err);
        }
    }
}
