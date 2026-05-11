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
#include "network_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <semaphore.h>
#include <time.h>

#define SELF_ADDR 0x02
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8888

static eproto_t g_eproto;
static network_channel_t g_network;
static uint8_t g_last_source_address = 0;
static uint16_t g_last_packet_id = 0;
static int g_needs_reply = 0;
static sem_t g_semaphore;
static int g_semaphore_initialized = 0;

void* mock_malloc(size_t size) {
    return malloc(size);
}

void mock_free(void* ptr) {
    free(ptr);
}

void mock_lock(void) {
}

void mock_unlock(void) {
}

uint32_t mock_get_timestamp(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

static eproto_signal_result_t mock_signal_wait(uint32_t timestamp) {
    if (!g_semaphore_initialized) {
        if (sem_init(&g_semaphore, 0, 0) != 0) {
            printf("[S2] Failed to initialize semaphore\n");
            return EPROTO_SIGNAL_TIMEOUT;
        }
        g_semaphore_initialized = 1;
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

    int result = sem_timedwait(&g_semaphore, &ts);
    return (result == 0) ? EPROTO_SIGNAL_DATA : EPROTO_SIGNAL_TIMEOUT;
}

static void mock_signal_send(void) {
    if (g_semaphore_initialized) {
        sem_post(&g_semaphore);
    }
}

void mock_status_callback(eproto_bus_t* bus, eproto_status_t status, uint8_t* data, uint16_t length) {
    (void)bus;
    (void)data;
    (void)length;
    switch (status) {
        case EPROTO_STATUS_CRC_ERROR:
            printf("[S2] Status: CRC error\n");
            break;
        case EPROTO_STATUS_MULTIPLE_CRC_ERRORS:
            printf("[S2] Status: Multiple CRC errors\n");
            break;
        case EPROTO_STATUS_HANDSHAKE_IN_PROGRESS:
            printf("[S2] Status: Handshake in progress\n");
            break;
        case EPROTO_STATUS_HANDSHAKE_SUCCESS:
            printf("[S2] Status: Handshake success\n");
            break;
    }
}

void s2_receive_callback(eproto_bus_t* bus, uint8_t source_address, uint16_t packet_id, uint8_t* data, uint16_t length) {
    (void)bus;
    printf("[S2] Received from device 0x%02X, packet ID: %d, data: ", source_address, packet_id);
    for (uint16_t i = 0; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");

    g_last_source_address = source_address;
    g_last_packet_id = packet_id;
    g_needs_reply = 1;

    uint8_t reply_data[] = {0xAA, 0xBB, 0xCC};
    eproto_send_user_reply(&g_eproto, source_address, packet_id, reply_data, sizeof(reply_data));
    printf("[S2] Auto-reply sent to master\n");
}

void s2_send_callback(eproto_send_status_t status, uint16_t packet_id, uint8_t* data, uint16_t length, void* private_data) {
    (void)private_data;
    switch (status) {
        case EPROTO_SEND_SUCCESS:
            printf("[S2] Send success, packet ID: %d\n", packet_id);
            if (data && length > 0) {
                printf("[S2] Received response: ");
                for (uint16_t i = 0; i < length; i++) {
                    printf("%02X ", data[i]);
                }
                printf("\n");
            }
            break;
        case EPROTO_SEND_TIMEOUT:
            printf("[S2] Send timeout, packet ID: %d\n", packet_id);
            break;
        case EPROTO_SEND_ERROR:
            printf("[S2] Send error, packet ID: %d\n", packet_id);
            break;
        case EPROTO_SEND_BUSY:
            printf("[S2] Send busy, packet ID: %d\n", packet_id);
            break;
    }
}

void s2_bus1_send(eproto_bus_t* bus, uint8_t* data, uint16_t length) {
    (void)bus;
    printf("[S2] Bus1 sending to master: ");
    for (uint16_t i = 0; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");

    if (g_network.sockfd >= 0) {
        network_send_data(&g_network, data, length);
    }
}

void* network_receive_thread(void* arg) {
    (void)arg;
    uint8_t rx_buffer[MAX_DATA_SIZE];

    printf("[S2] Network receive thread started\n");

    while (1) {
        int client_fd = -1;
        int received = network_receive_data(&g_network, rx_buffer, sizeof(rx_buffer), &client_fd);

        if (received > 0) {
            printf("[S2] Received %d bytes from network\n", received);
            eproto_receive_data(&g_eproto, 0x01, rx_buffer, received);
        } else if (received < 0) {
            printf("[S2] Network receive error, exiting\n");
            break;
        }

        usleep(10000);
    }

    return NULL;
}

void* protocol_thread(void* arg) {
    (void)arg;
    printf("[S2] Protocol thread started\n");

    while (1) {
        eproto_process(&g_eproto);
    }
    return NULL;
}

void* auto_send_thread(void* arg) {
    (void)arg;
    sleep(2);

    printf("[S2] Sending test data to master...\n");
    uint8_t test_data[] = {0x11, 0x22, 0x33, 0x44, 0x55};
    eproto_send(&g_eproto, 0x01, test_data, sizeof(test_data), s2_send_callback, NULL, 1);

    sleep(1);
    printf("[S2] Auto-send test completed\n");

    return NULL;
}

int main(void) {
    printf("=== Slave Device S2 ===\n");
    printf("Network: TCP Client connecting to %s:%d\n\n", SERVER_IP, SERVER_PORT);

    int retry_count = 0;
    while (retry_count < 10) {
        if (network_init_channel(&g_network, PROTOCOL_TCP, SERVER_IP, SERVER_PORT, 0) == 0) {
            printf("[S2] Connected to master server\n");
            break;
        }
        printf("[S2] Connection failed, retrying... (%d/10)\n", retry_count + 1);
        sleep(1);
        retry_count++;
    }

    if (g_network.sockfd < 0) {
        printf("[S2] Failed to connect to master server\n");
        return 1;
    }

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

    eproto_error_t error = eproto_init(&g_eproto, &user_functions);
    if (error != EPROTO_OK) {
        printf("[S2] Failed to initialize eProto\n");
        return 1;
    }
    printf("[S2] eProto initialized\n");

    uint8_t s2_rx_buffer1[256];
    eproto_bus_t bus1 = {
        .self_addr = 0x01,
        .send = s2_bus1_send,
        .rx_buffer = s2_rx_buffer1,
        .rx_buffer_size = sizeof(s2_rx_buffer1),
        .name = "s2_bus1",
        .user_data = NULL,
        .status_callback = mock_status_callback,
        .receive_callback = s2_receive_callback,
        .forward_callback = NULL
    };
    error = eproto_add_bus(&g_eproto, &bus1);
    if (error != EPROTO_OK) {
        printf("[S2] Failed to add bus 1\n");
        return 1;
    }
    printf("[S2] Bus 1 (connected to M1) added\n");

    error = eproto_add_destination_device(&g_eproto, 0x01, 0x01);
    printf("[S2] Destination device M1 (0x01) added\n");

    pthread_t recv_thread, proto_thread, send_thread;
    pthread_create(&recv_thread, NULL, network_receive_thread, NULL);
    pthread_create(&proto_thread, NULL, protocol_thread, NULL);
    pthread_create(&send_thread, NULL, auto_send_thread, NULL);

    printf("\n[S2] Ready! Connected to master.\n");
    printf("[S2] Will automatically send test data after startup.\n");

    pthread_join(send_thread, NULL);

    sleep(5);

    network_close_channel(&g_network);
    eproto_destroy(&g_eproto);

    printf("\n[S2] Exiting\n");
    return 0;
}
