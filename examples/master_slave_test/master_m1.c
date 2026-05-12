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

#define SELF_ADDR 0x01
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
            printf("[M1] Failed to initialize semaphore\n");
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
            printf("[M1] Status: CRC error\n");
            break;
        case EPROTO_STATUS_MULTIPLE_CRC_ERRORS:
            printf("[M1] Status: Multiple CRC errors\n");
            break;
#if EPROTO_ENABLE_HANDSHAKE
        case EPROTO_STATUS_HANDSHAKE_IN_PROGRESS:
            printf("[M1] Status: Handshake in progress\n");
            break;
        case EPROTO_STATUS_HANDSHAKE_SUCCESS:
            printf("[M1] Status: Handshake success\n");
            break;
#endif
    }
}

void m1_receive_callback(eproto_bus_t* bus, uint8_t source_address, uint16_t packet_id, uint8_t* data, uint16_t length)
{
    (void)bus;
    printf("[M1] Received from device 0x%02X, packet ID: %d, data: ", source_address, packet_id);
    for (uint16_t i = 0; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");

    g_last_source_address = source_address;
    g_last_packet_id = packet_id;
    g_needs_reply = 1;
    
    // 自动回复
    uint8_t reply_data[] = {0xAA, 0xBB, 0xCC};
    printf("[M1] Sending auto-reply to device 0x%02X\n", source_address);
    eproto_send_user_reply(&g_eproto, source_address, packet_id, reply_data, sizeof(reply_data));
}

void m1_send_callback(eproto_send_status_t status, uint16_t packet_id, uint8_t* data, uint16_t length, void* private_data) {
    (void)private_data;
    switch (status) {
        case EPROTO_SEND_SUCCESS:
            printf("[M1] Send success, packet ID: %d\n", packet_id);
            if (data && length > 0) {
                printf("[M1] Received response: ");
                for (uint16_t i = 0; i < length; i++) {
                    printf("%02X ", data[i]);
                }
                printf("\n");
            }
            break;
        case EPROTO_SEND_TIMEOUT:
            printf("[M1] Send timeout, packet ID: %d\n", packet_id);
            break;
        case EPROTO_SEND_ERROR:
            printf("[M1] Send error, packet ID: %d\n", packet_id);
            break;
        case EPROTO_SEND_BUSY:
            printf("[M1] Send busy, packet ID: %d\n", packet_id);
            break;
    }
}

void m1_bus_send(eproto_bus_t* bus, uint8_t* data, uint16_t length) {
    (void)bus;
    printf("[M1] Bus sending: ");
    for (uint16_t i = 0; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");

    network_send_data(&g_network, data, length);
}

void* network_receive_thread(void* arg) {
    (void)arg;
    uint8_t rx_buffer[MAX_DATA_SIZE];

    printf("[M1] Network receive thread started\n");

    while (1) {
        int received = network_receive_data(&g_network, rx_buffer, sizeof(rx_buffer));

        if (received > 0) {
            // 所有设备都能收到所有消息，通过eProto协议地址过滤
            eproto_receive_data(&g_eproto, 0x01, rx_buffer, received);
        } else if (received < 0) {
            printf("[M1] Network receive error, exiting\n");
            break;
        }

        usleep(10000);
    }

    return NULL;
}

void* protocol_thread(void* arg) {
    (void)arg;
    printf("[M1] Protocol thread started\n");

    while (1) {
        eproto_process(&g_eproto);
    }
    return NULL;
}

void print_help(void) {
    printf("\n[M1] Available commands:\n");
    printf("  send <device_addr> <need_reply> <data...> - Send data to device (e.g., send 2 1 AA BB CC)\n");
    printf("  broadcast <need_reply> <data...> - Broadcast to all slaves\n");
    printf("  send_reply <data...> - Send reply to last received message\n");
    printf("  help - Show this help\n");
    printf("  quit - Exit\n\n");
}

int main(void) {
    setbuf(stdout, NULL);

    printf("=== Master Device M1 ===\n");
    printf("Network: UDP Multicast on %s:%d\n\n", MULTICAST_IP, SERVER_PORT);

    if (network_init_channel(&g_network, MULTICAST_IP, SERVER_PORT) < 0) {
        printf("[M1] Failed to initialize network\n");
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
        printf("[M1] Failed to initialize eProto\n");
        return 1;
    }
    printf("[M1] eProto initialized\n");

    // 总线1（连接所有从设备）
    uint8_t m1_rx_buffer1[256];
    eproto_bus_t bus1 = {
        .self_addr = 0x01,  // 主设备地址
        .send = m1_bus_send,
        .rx_buffer = m1_rx_buffer1,
        .rx_buffer_size = sizeof(m1_rx_buffer1),
        .name = "m1_bus1",
        .user_data = NULL,
        .status_callback = mock_status_callback,
        .receive_callback = m1_receive_callback,
#if EPROTO_ENABLE_FORWARD
        .forward_callback = NULL
#endif
    };
    error = eproto_add_bus(&g_eproto, &bus1);
    if (error != EPROTO_OK) {
        printf("[M1] Failed to add bus 1\n");
        return 1;
    }
    printf("[M1] Bus 1 (connected to all slaves) added\n");

    // 添加目标设备
    error = eproto_add_destination_device(&g_eproto, 0x01, 0x02);
    printf("[M1] Destination device S2 (0x02) added\n");
    error = eproto_add_destination_device(&g_eproto, 0x01, 0x03);
    printf("[M1] Destination device S3 (0x03) added\n");
    error = eproto_add_destination_device(&g_eproto, 0x01, 0x04);
    printf("[M1] Destination device S4 (0x04) added\n");

    pthread_t recv_thread, proto_thread;
    pthread_create(&recv_thread, NULL, network_receive_thread, NULL);
    pthread_create(&proto_thread, NULL, protocol_thread, NULL);

    printf("\n[M1] Ready! Waiting for slave connections...\n");
    print_help();

    char command[256];
    int stdin_fd = fileno(stdin);
    int flags = fcntl(stdin_fd, F_GETFL, 0);
    fcntl(stdin_fd, F_SETFL, flags | O_NONBLOCK);

    while (1) {
        if (fgets(command, sizeof(command), stdin) != NULL) {
            command[strcspn(command, "\n")] = 0;

            if (strcmp(command, "quit") == 0) {
                break;
            } else if (strcmp(command, "help") == 0) {
                print_help();
            } else if (strncmp(command, "broadcast ", 9) == 0) {
                char* rest = command + 9;
                uint8_t need_reply;
                if (sscanf(rest, "%hhu", &need_reply) != 1) {
                    printf("Invalid need_reply flag\n");
                } else {
                    rest = strchr(rest, ' ');
                    if (rest) {
                        rest++;
                        uint8_t data[256];
                        int data_len = 0;
                        char* token = strtok(rest, " ");
                        while (token && data_len < 256) {
                            unsigned int val;
                            if (sscanf(token, "%x", &val) == 1) {
                                data[data_len++] = (uint8_t)val;
                            }
                            token = strtok(NULL, " ");
                        }

                        if (data_len > 0) {
                            printf("[M1] Broadcasting to all slaves: ");
                            for (int i = 0; i < data_len; i++) {
                                printf("%02X ", data[i]);
                            }
                            printf("\n");

                            eproto_send(&g_eproto, 0x02, data, data_len, m1_send_callback, NULL, need_reply);
                            eproto_send(&g_eproto, 0x03, data, data_len, m1_send_callback, NULL, need_reply);
                            eproto_send(&g_eproto, 0x04, data, data_len, m1_send_callback, NULL, need_reply);
                        }
                    }
                }
            } else if (strncmp(command, "send ", 5) == 0) {
                char* rest = command + 5;
                uint8_t device_addr;
                uint8_t need_reply;
                if (sscanf(rest, "%hhu %hhu", &device_addr, &need_reply) != 2) {
                    printf("Invalid command format\n");
                } else {
                    rest = strchr(rest, ' ');
                    if (rest) {
                        rest++;
                        rest = strchr(rest, ' ');
                        if (rest) {
                            rest++;
                            uint8_t data[256];
                            int data_len = 0;
                            char* token = strtok(rest, " ");
                            while (token && data_len < 256) {
                                unsigned int val;
                                if (sscanf(token, "%x", &val) == 1) {
                                    data[data_len++] = (uint8_t)val;
                                }
                                token = strtok(NULL, " ");
                            }

                            if (data_len > 0) {
                                printf("[M1] Sending to device 0x%02X: ", device_addr);
                                for (int i = 0; i < data_len; i++) {
                                    printf("%02X ", data[i]);
                                }
                                printf("\n");

                                eproto_send(&g_eproto, device_addr, data, data_len, m1_send_callback, NULL, need_reply);
                            }
                        }
                    }
                }
            } else if (strncmp(command, "send_reply ", 11) == 0) {
                if (!g_needs_reply) {
                    printf("[M1] No pending reply needed\n");
                } else {
                    char* rest = command + 11;
                    uint8_t data[256];
                    int data_len = 0;
                    char* token = strtok(rest, " ");
                    while (token && data_len < 256) {
                        unsigned int val;
                        if (sscanf(token, "%x", &val) == 1) {
                            data[data_len++] = (uint8_t)val;
                        }
                        token = strtok(NULL, " ");
                    }

                    if (data_len > 0) {
                        printf("[M1] Sending reply to device 0x%02X, packet ID %d: ", g_last_source_address, g_last_packet_id);
                        for (int i = 0; i < data_len; i++) {
                            printf("%02X ", data[i]);
                        }
                        printf("\n");

                        eproto_send_user_reply(&g_eproto, g_last_source_address, g_last_packet_id, data, data_len);
                        g_needs_reply = 0;
                    }
                }
            } else if (strlen(command) > 0) {
                printf("[M1] Unknown command: %s\n", command);
                print_help();
            }
        }
        usleep(10000);
    }

    network_close_channel(&g_network);
    eproto_destroy(&g_eproto);

    printf("\n[M1] Exiting\n");
    return 0;
}
