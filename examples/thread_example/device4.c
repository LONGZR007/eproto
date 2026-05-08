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

// 全局变量声明
eproto_t* g_device4_eproto = NULL;

// 设备4接收回调函数
void device4_receive_callback(eproto_bus_t* bus, uint8_t source_address, uint16_t packet_id, uint8_t* data, uint16_t length) {
    uint8_t encrypted = 0;
    uint8_t need_reply = 0;
    uint16_t payload_length = 0;
    uint8_t* payload = protocol_unwrap(data, length, &encrypted, &need_reply, &payload_length);

    printf("Device 4 received data from device 0x%02X, packet ID: %d: ", source_address, packet_id);
    for (uint16_t i = 0; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");

    if (encrypted && payload && payload_length > 0) {
        uint8_t key = get_key_for_bus(source_address, bus->self_addr);
        uint8_t* decrypted_payload = decrypt_data(payload, payload_length, key);
        if (decrypted_payload) {
            printf("Device 4: Decrypted payload: ");
            for (uint16_t i = 0; i < payload_length; i++) {
                printf("%02X ", decrypted_payload[i]);
            }
            printf("\n");

            if (memcmp(decrypted_payload, "\x11\x22\x33\x44\x55", 5) == 0) {
                printf("Device 4: TEST 4 VERIFIED - Encrypted data from device 1 via device 2 correctly decrypted!\n");
            }
            free(decrypted_payload);
        }
    } else if (payload && payload_length > 0) {
        printf("Device 4: Plain payload: ");
        for (uint16_t i = 0; i < payload_length; i++) {
            printf("%02X ", payload[i]);
        }
        printf("\n");

        if (memcmp(payload, "\x33\x44\x55\x66\x77", 5) == 0) {
            printf("Device 4: TEST 3 VERIFIED - Plain data from device 1 via device 2 correctly received!\n");
        }
    }

    if (payload && payload_length > 0 && need_reply) {
        printf("Device 4: Protocol header - encrypted=%d, need_reply=%d\n", encrypted, need_reply);
        printf("Device 4: Sending reply with payload: ");
        for (uint16_t i = 0; i < payload_length; i++) {
            printf("%02X ", payload[i]);
        }
        printf("\n");
        uint8_t* reply_data = NULL;
        uint16_t reply_length = 0;
        reply_data = protocol_wrap(0, 0, payload, payload_length, &reply_length);
        if (reply_data) {
            eproto_error_t error = eproto_send_user_reply(g_device4_eproto, source_address, packet_id, reply_data, reply_length);
            if (error != EPROTO_OK) {
                printf("Device 4: Failed to send reply\n");
            } else {
                printf("Device 4: Reply sent successfully\n");
            }
            free(reply_data);
        }
    }
}

// 设备4发送回调函数
void device4_send_callback(eproto_send_status_t status, uint16_t packet_id, uint8_t* data, uint16_t length,
                           void* private_data) {
    (void)data;
    (void)length;
    thread_data_t* thread_data = (thread_data_t*)private_data;
    switch (status) {
        case EPROTO_SEND_SUCCESS:
            printf("%s: Send success, packet ID: %d\n", thread_data->device_name, packet_id);
            break;
        case EPROTO_SEND_TIMEOUT:
            printf("%s: Send timeout, packet ID: %d\n", thread_data->device_name, packet_id);
            break;
        case EPROTO_SEND_ERROR:
            printf("%s: Send error, packet ID: %d\n", thread_data->device_name, packet_id);
            break;
        case EPROTO_SEND_BUSY:
            printf("%s: Send busy, packet ID: %d\n", thread_data->device_name, packet_id);
            break;
    }
}

// 设备4信号等待函数
eproto_signal_result_t device4_signal_wait(uint32_t timestamp) {
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

// 设备4信号发送函数
void device4_signal_send(void) {
    if (g_current_thread_data && g_current_thread_data->semaphore_initialized) {
        sem_post(&g_current_thread_data->semaphore);
    }
}

// 设备4接收线程
void* device4_receive_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    printf("%s receive thread started\n", data->device_name);

    g_current_thread_data = data;

    while (1) {
        uint8_t rx_buffer[256];
        uint16_t rx_count = device4_bus_receive(rx_buffer, sizeof(rx_buffer));
        if (rx_count > 0) {
            eproto_receive_data(&data->eproto_inst, 0x04, rx_buffer, rx_count);
        }
        usleep(10000);
    }

    printf("%s receive thread finished\n", data->device_name);
    pthread_exit(NULL);
}

// 设备4处理线程
void* device4_process_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    printf("%s process thread started\n", data->device_name);

    g_current_thread_data = data;

    while (1) {
        eproto_process(&data->eproto_inst);
        usleep(10000);
    }

    printf("%s process thread finished\n", data->device_name);
    pthread_exit(NULL);
}

// 设备4线程
void* device4_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    printf("%s thread started\n", data->device_name);

    data->semaphore_initialized = 0;
    data->signal_flag = 0;

    eproto_user_functions_t user_functions = {
        .malloc = mock_malloc,
        .free = mock_free,
        .signal_wait = device4_signal_wait,
        .signal_send = device4_signal_send,
        .lock = mock_lock,
        .unlock = mock_unlock,
        .get_timestamp = mock_get_timestamp,
        .timeout_timestamp = 0
    };

    eproto_error_t error = eproto_init(&data->eproto_inst, &user_functions);
    if (error != EPROTO_OK) {
        printf("%s: Failed to initialize eProto\n", data->device_name);
        pthread_exit(NULL);
    }
    printf("%s: eProto initialized successfully\n", data->device_name);

    g_device4_eproto = &data->eproto_inst;

    eproto_bus_t bus = {
        .self_addr = 0x04,
        .send = device4_bus_send,
        .rx_buffer = data->rx_buffer,
        .rx_buffer_size = sizeof(data->rx_buffer),
        .name = "device4_bus",
        .user_data = NULL,
        .status_callback = mock_status_callback,
        .receive_callback = device4_receive_callback,
        .forward_callback = NULL
    };
    error = eproto_add_bus(&data->eproto_inst, &bus);
    if (error != EPROTO_OK) {
        printf("%s: Failed to add bus\n", data->device_name);
        pthread_exit(NULL);
    }
    printf("%s: Bus added successfully\n", data->device_name);

    error = eproto_add_destination_device(&data->eproto_inst, 0x04, 0x03);
    if (error != EPROTO_OK) {
        printf("%s: Failed to add target device\n", data->device_name);
        pthread_exit(NULL);
    }
    printf("%s: Target device 0x03 added successfully\n", data->device_name);

    error = eproto_add_destination_device(&data->eproto_inst, 0x04, 0x01);
    if (error != EPROTO_OK) {
        printf("%s: Failed to add target device 0x01\n", data->device_name);
        pthread_exit(NULL);
    }
    printf("%s: Target device 0x01 added successfully\n", data->device_name);

    pthread_t receive_thread, process_thread;

    data->thread_type = THREAD_TYPE_RECEIVE;

    if (pthread_create(&receive_thread, NULL, device4_receive_thread, data) != 0) {
        printf("%s: Failed to create receive thread\n", data->device_name);
        pthread_exit(NULL);
    }

    data->thread_type = THREAD_TYPE_PROCESS;

    if (pthread_create(&process_thread, NULL, device4_process_thread, data) != 0) {
        printf("%s: Failed to create process thread\n", data->device_name);
        pthread_exit(NULL);
    }

    pthread_join(receive_thread, NULL);
    pthread_join(process_thread, NULL);

    eproto_destroy(&data->eproto_inst);

    printf("%s thread finished\n", data->device_name);
    pthread_exit(NULL);
}
