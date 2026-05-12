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

// 通过编译时宏SLAVE_ADDR指定设备地址
#ifndef SLAVE_ADDR
#define SLAVE_ADDR 0x02
#endif

#define MASTER_ADDR 0x01
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8888

static eproto_t g_eproto;
static network_channel_t g_network;
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
            printf("[S%d] Failed to initialize semaphore\n", SLAVE_ADDR);
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
            printf("[S%d] Status: CRC error\n", SLAVE_ADDR);
            break;
        case EPROTO_STATUS_MULTIPLE_CRC_ERRORS:
            printf("[S%d] Status: Multiple CRC errors\n", SLAVE_ADDR);
            break;
#if EPROTO_ENABLE_HANDSHAKE
        case EPROTO_STATUS_HANDSHAKE_IN_PROGRESS:
            printf("[S%d] Status: Handshake in progress\n", SLAVE_ADDR);
            break;
        case EPROTO_STATUS_HANDSHAKE_SUCCESS:
            printf("[S%d] Status: Handshake success\n", SLAVE_ADDR);
            break;
#endif
    }
}

void slave_receive_callback(eproto_bus_t* bus, uint8_t source_address, uint16_t packet_id, uint8_t* data, uint16_t length) {
    (void)bus;
    printf("[S%d] Received from master 0x%02X, packet ID: %d, data: ", SLAVE_ADDR, source_address, packet_id);
    for (uint16_t i = 0; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");

    uint8_t reply_data[3];
    reply_data[0] = (SLAVE_ADDR * 0x10) & 0xFF;
    reply_data[1] = ((SLAVE_ADDR + 1) * 0x10) & 0xFF;
    reply_data[2] = ((SLAVE_ADDR + 2) * 0x10) & 0xFF;
    printf("[S%d] Sending reply to master: %02X %02X %02X\n", SLAVE_ADDR, reply_data[0], reply_data[1], reply_data[2]);
    eproto_send_user_reply(&g_eproto, source_address, packet_id, reply_data, sizeof(reply_data));
}

void slave_bus_send(eproto_bus_t* bus, uint8_t* data, uint16_t length) {
    (void)bus;
    printf("[S%d] Bus sending: ", SLAVE_ADDR);
    for (uint16_t i = 0; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");

    network_send_data(&g_network, data, length);
}

void* network_receive_thread(void* arg) {
    (void)arg;
    uint8_t rx_buffer[MAX_DATA_SIZE];

    printf("[S%d] Network receive thread started\n", SLAVE_ADDR);

    while (1) {
        int received = network_receive_data(&g_network, rx_buffer, sizeof(rx_buffer));

        if (received > 0) {
            // 所有设备都能收到所有消息，通过eProto协议地址过滤
            eproto_receive_data(&g_eproto, SLAVE_ADDR, rx_buffer, received);
        } else if (received < 0) {
            printf("[S%d] Network receive error, exiting\n", SLAVE_ADDR);
            break;
        }

        usleep(10000);
    }

    return NULL;
}

void* protocol_thread(void* arg) {
    (void)arg;
    printf("[S%d] Protocol thread started (passive mode)\n", SLAVE_ADDR);

    while (1) {
        eproto_process(&g_eproto);
    }
    return NULL;
}

int main(void) {
    setbuf(stdout, NULL);

    printf("=== Slave Device S%d ===\n", SLAVE_ADDR);
    printf("Network: UDP Multicast on %s:%d\n\n", MULTICAST_IP, SERVER_PORT);

    // 等待一小段时间，确保主设备先启动
    usleep(500000);  // 0.5秒

    if (network_init_channel(&g_network, MULTICAST_IP, SERVER_PORT) < 0) {
        printf("[S%d] Failed to initialize network\n", SLAVE_ADDR);
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
        printf("[S%d] Failed to initialize eProto\n", SLAVE_ADDR);
        return 1;
    }
    printf("[S%d] eProto initialized\n", SLAVE_ADDR);

    uint8_t slave_rx_buffer[256];
    eproto_bus_t bus1 = {
        .self_addr = SLAVE_ADDR,
        .send = slave_bus_send,
        .rx_buffer = slave_rx_buffer,
        .rx_buffer_size = sizeof(slave_rx_buffer),
        .name = "slave_bus1",
        .user_data = NULL,
        .status_callback = mock_status_callback,
        .receive_callback = slave_receive_callback,
#if EPROTO_ENABLE_FORWARD
        .forward_callback = NULL
#endif
    };
    error = eproto_add_bus(&g_eproto, &bus1);
    if (error != EPROTO_OK) {
        printf("[S%d] Failed to add bus 1\n", SLAVE_ADDR);
        return 1;
    }
    printf("[S%d] Bus 1 (connected to M1) added\n", SLAVE_ADDR);

    error = eproto_add_destination_device(&g_eproto, SLAVE_ADDR, MASTER_ADDR);
    printf("[S%d] Destination device M1 (0x%02X) added\n", SLAVE_ADDR, MASTER_ADDR);

    pthread_t recv_thread, proto_thread;
    pthread_create(&recv_thread, NULL, network_receive_thread, NULL);
    pthread_create(&proto_thread, NULL, protocol_thread, NULL);

    printf("\n[S%d] Ready! Passive mode - waiting for master requests.\n", SLAVE_ADDR);
    printf("[S%d] Use 'send <addr> 1 <data>' from master to request data.\n", SLAVE_ADDR);

    sleep(30);

    network_close_channel(&g_network);
    eproto_destroy(&g_eproto);

    printf("\n[S%d] Exiting\n", SLAVE_ADDR);
    return 0;
}
