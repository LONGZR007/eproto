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
#include "ipc_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <semaphore.h>

static eproto_t g_eproto;
static ipc_channel_t g_ipc_tx_channel_a;
static ipc_channel_t g_ipc_rx_channel_a;
static ipc_channel_t g_ipc_tx_channel_c;
static ipc_channel_t g_ipc_rx_channel_c;
static ipc_channel_t g_ipc_tx_channel_e;
static ipc_channel_t g_ipc_rx_channel_e;
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
            printf("Failed to initialize semaphore\n");
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
    if (result == 0) {
        return EPROTO_SIGNAL_DATA;
    } else {
        return EPROTO_SIGNAL_TIMEOUT;
    }
}

static void mock_signal_send(void) {
    if (g_semaphore_initialized) {
        sem_post(&g_semaphore);
    }
}

void mock_wakeup(void) {
    printf("Waking up...\n");
}

void mock_status_callback(eproto_status_t status, uint8_t* data, uint16_t length) {
    (void)data;
    (void)length;
    switch (status) {
        case EPROTO_STATUS_CRC_ERROR:
            printf("Status: CRC error\n");
            break;

        case EPROTO_STATUS_MULTIPLE_CRC_ERRORS:
            printf("Status: Multiple CRC errors\n");
            break;
        case EPROTO_STATUS_HANDSHAKE_IN_PROGRESS:
            printf("Status: Handshake in progress\n");
            break;
        case EPROTO_STATUS_HANDSHAKE_SUCCESS:
            printf("Status: Handshake success\n");
            break;
    }
}

void b_receive_callback(uint8_t source_address, uint16_t packet_id, uint8_t* data, uint16_t length) {
    printf("Process B: Received data from device %02X, packet ID: %d: ", source_address, packet_id);
    for (uint16_t i = 0; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");

    g_last_source_address = source_address;
    g_last_packet_id = packet_id;
    g_needs_reply = 1;
}

void b_send_callback(eproto_send_status_t status, uint16_t packet_id, uint8_t* data, uint16_t length,
                     void* private_data) {
    (void)private_data;
    switch (status) {
        case EPROTO_SEND_SUCCESS:
            printf("Process B: Send success, packet ID: %d\n", packet_id);
            if (data && length > 0) {
                printf("Process B: Received response: ");
                for (uint16_t i = 0; i < length; i++) {
                    printf("%02X ", data[i]);
                }
                printf("\n");
            }
            break;
        case EPROTO_SEND_TIMEOUT:
            printf("Process B: Send timeout, packet ID: %d\n", packet_id);
            break;
        case EPROTO_SEND_ERROR:
            printf("Process B: Send error, packet ID: %d\n", packet_id);
            break;
        case EPROTO_SEND_BUSY:
            printf("Process B: Send busy, packet ID: %d\n", packet_id);
            break;
    }
}

void b_bus3_send(uint8_t* data, uint16_t length) {
    printf("Process B bus3 sending: ");
    for (uint16_t i = 0; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");

    // 总线3发送到A
    ipc_send_data(&g_ipc_tx_channel_a, data, length);
}

void b_bus4_send(uint8_t* data, uint16_t length) {
    printf("Process B bus4 sending: ");
    for (uint16_t i = 0; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");

    // 总线4发送到C
    ipc_send_data(&g_ipc_tx_channel_c, data, length);
}

void b_bus5_send(uint8_t* data, uint16_t length) {
    printf("Process B bus5 sending: ");
    for (uint16_t i = 0; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");

    // 总线5发送到E
    ipc_send_data(&g_ipc_tx_channel_e, data, length);
}

void* receive_thread_a(void* arg) {
    (void)arg;
    uint8_t rx_buffer[1024];

    while (1) {
        int received = ipc_receive_data(&g_ipc_rx_channel_a, rx_buffer, sizeof(rx_buffer));
        if (received > 0) {
            printf("Process B received from A %d bytes: ", received);
            for (int i = 0; i < received; i++) {
                printf("%02X ", rx_buffer[i]);
            }
            printf("\n");

            eproto_receive_data(&g_eproto, 0x03, rx_buffer, received);  // 总线3接收来自A的数据
        }
        usleep(10000);
    }
    return NULL;
}

void* receive_thread_c(void* arg) {
    (void)arg;
    uint8_t rx_buffer[1024];

    while (1) {
        int received = ipc_receive_data(&g_ipc_rx_channel_c, rx_buffer, sizeof(rx_buffer));
        if (received > 0) {
            printf("Process B received from C %d bytes: ", received);
            for (int i = 0; i < received; i++) {
                printf("%02X ", rx_buffer[i]);
            }
            printf("\n");

            eproto_receive_data(&g_eproto, 0x04, rx_buffer, received);  // 总线4接收来自C的数据
        }
        usleep(10000);
    }
    return NULL;
}

void* receive_thread_e(void* arg) {
    (void)arg;
    uint8_t rx_buffer[1024];

    while (1) {
        int received = ipc_receive_data(&g_ipc_rx_channel_e, rx_buffer, sizeof(rx_buffer));
        if (received > 0) {
            printf("Process B received from E %d bytes: ", received);
            for (int i = 0; i < received; i++) {
                printf("%02X ", rx_buffer[i]);
            }
            printf("\n");

            eproto_receive_data(&g_eproto, 0x05, rx_buffer, received);  // 总线5接收来自E的数据
        }
        usleep(10000);
    }
    return NULL;
}

void* protocol_thread(void* arg) {
    (void)arg;
    printf("Protocol thread started\n");

    while (1) {
        eproto_process(&g_eproto);
    }
    return NULL;
}

void print_help(void) {
    printf("\nAvailable commands:\n");
    printf(
        "  send <device_addr> <need_reply> <data...> - Send data to device "
        "(e.g., send 1 1 11 22 33)\n");
    printf(
        "  send_reply <data...> - Send reply to last received message (e.g., "
        "send_reply AA BB CC)\n");
    printf("  help - Show this help message\n");
    printf("  quit - Exit the program\n");
    printf("\n");
}

int main(void) {
    printf("=== Process B ===\n\n");

    ipc_init_channel(&g_ipc_tx_channel_a);
    ipc_init_channel(&g_ipc_rx_channel_a);
    ipc_init_channel(&g_ipc_tx_channel_c);
    ipc_init_channel(&g_ipc_rx_channel_c);
    ipc_init_channel(&g_ipc_tx_channel_e);
    ipc_init_channel(&g_ipc_rx_channel_e);

    eproto_user_functions_t user_functions = {.malloc = mock_malloc,
                                              .free = mock_free,
                                              .signal_wait = mock_signal_wait,
                                              .signal_send = mock_signal_send,
                                              .lock = mock_lock,
                                              .unlock = mock_unlock,
                                              .get_timestamp = mock_get_timestamp,
                                              .timeout_timestamp = 0};

    eproto_error_t error = eproto_init(&g_eproto, &user_functions);
    if (error != EPROTO_OK) {
        printf("Failed to initialize eProto\n");
        return 1;
    }
    printf("eProto initialized successfully\n");

    // 总线3（连接到A1）
    uint8_t b_rx_buffer3[256];
    error = eproto_add_bus(&g_eproto, 0x03, b_bus3_send, b_rx_buffer3, sizeof(b_rx_buffer3), "b_bus3",
                           mock_status_callback, b_receive_callback, NULL);
    if (error != EPROTO_OK) {
        printf("Failed to add bus 3\n");
        return 1;
    }
    printf("Bus 3 added successfully\n");

    // 总线4（连接到C6）
    uint8_t b_rx_buffer4[256];
    error = eproto_add_bus(&g_eproto, 0x04, b_bus4_send, b_rx_buffer4, sizeof(b_rx_buffer4), "b_bus4",
                           mock_status_callback, b_receive_callback, NULL);
    if (error != EPROTO_OK) {
        printf("Failed to add bus 4\n");
        return 1;
    }
    printf("Bus 4 added successfully\n");

    // 总线5（连接到E11）
    uint8_t b_rx_buffer5[256];
    error = eproto_add_bus(&g_eproto, 0x05, b_bus5_send, b_rx_buffer5, sizeof(b_rx_buffer5), "b_bus5",
                           mock_status_callback, b_receive_callback, NULL);
    if (error != EPROTO_OK) {
        printf("Failed to add bus 5\n");
        return 1;
    }
    printf("Bus 5 added successfully\n");

    // 添加目标设备
    // 总线3挂载到A1、D8
    error = eproto_add_destination_device(&g_eproto, 0x03, 0x01);  // A
    if (error != EPROTO_OK) {
        printf("Failed to add target device 0x01\n");
        return 1;
    }
    printf("Target device 0x01 added successfully\n");

    error = eproto_add_destination_device(&g_eproto, 0x03, 0x08);  // D
    if (error != EPROTO_OK) {
        printf("Failed to add target device 0x08\n");
        return 1;
    }
    printf("Target device 0x08 added successfully\n");

    // 总线4挂载到C6
    error = eproto_add_destination_device(&g_eproto, 0x04, 0x06);  // C
    if (error != EPROTO_OK) {
        printf("Failed to add target device 0x06\n");
        return 1;
    }
    printf("Target device 0x06 added successfully\n");

    // 总线5挂载到E11
    error = eproto_add_destination_device(&g_eproto, 0x05, 0x0B);  // E
    if (error != EPROTO_OK) {
        printf("Failed to add target device 0x0B\n");
        return 1;
    }
    printf("Target device 0x0B added successfully\n");

    printf("Opening FIFO channels...\n");
    if (ipc_open_fifo(&g_ipc_tx_channel_a, FIFO_PATH_B_TO_A) < 0) {
        printf("Failed to open TX FIFO to A\n");
        return 1;
    }

    if (ipc_open_fifo(&g_ipc_rx_channel_a, FIFO_PATH_A_TO_B) < 0) {
        printf("Failed to open RX FIFO from A\n");
        return 1;
    }

    if (ipc_open_fifo(&g_ipc_tx_channel_c, FIFO_PATH_B_TO_C) < 0) {
        printf("Failed to open TX FIFO to C\n");
        return 1;
    }

    if (ipc_open_fifo(&g_ipc_rx_channel_c, FIFO_PATH_C_TO_B) < 0) {
        printf("Failed to open RX FIFO from C\n");
        return 1;
    }

    if (ipc_open_fifo(&g_ipc_tx_channel_e, FIFO_PATH_B_TO_E) < 0) {
        printf("Failed to open TX FIFO to E\n");
        return 1;
    }

    if (ipc_open_fifo(&g_ipc_rx_channel_e, FIFO_PATH_E_TO_B) < 0) {
        printf("Failed to open RX FIFO from E\n");
        return 1;
    }

    pthread_t recv_thread_a, recv_thread_c, recv_thread_e, proto_thread;
    pthread_create(&recv_thread_a, NULL, receive_thread_a, NULL);
    pthread_create(&recv_thread_c, NULL, receive_thread_c, NULL);
    pthread_create(&recv_thread_e, NULL, receive_thread_e, NULL);
    pthread_create(&proto_thread, NULL, protocol_thread, NULL);

    printf("\nProcess B ready!\n");
    print_help();

    char command[256];
    int stdin_fd = fileno(stdin);
    int flags = fcntl(stdin_fd, F_GETFL, 0);
    fcntl(stdin_fd, F_SETFL, flags | O_NONBLOCK);

    while (1) {
        // 尝试读取命令
        if (fgets(command, sizeof(command), stdin) != NULL) {
            printf("> ");
            fflush(stdout);
            command[strcspn(command, "\n")] = 0;

            if (strcmp(command, "quit") == 0) {
                break;
            } else if (strcmp(command, "help") == 0) {
                print_help();
            } else if (strncmp(command, "send ", 5) == 0) {
                char* rest = command + 5;
                uint8_t device_addr;
                uint8_t need_reply;
                if (sscanf(rest, "%hhu %hhu", &device_addr, &need_reply) != 2) {
                    printf("Invalid device address or need_reply flag\n");
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
                                printf("Sending to device %02X: ", device_addr);
                                for (int i = 0; i < data_len; i++) {
                                    printf("%02X ", data[i]);
                                }
                                printf("\n");

                                error = eproto_send(&g_eproto, device_addr, data, data_len, b_send_callback, NULL,
                                                    need_reply);
                                if (error != EPROTO_OK) {
                                    printf("Failed to send data\n");
                                }
                            }
                        }
                    }
                }
            } else if (strncmp(command, "send_reply ", 11) == 0) {
                if (!g_needs_reply) {
                    printf("No pending reply needed\n");
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
                        printf("Sending reply to device %02X, packet ID %d: ", g_last_source_address, g_last_packet_id);
                        for (int i = 0; i < data_len; i++) {
                            printf("%02X ", data[i]);
                        }
                        printf("\n");

                        error =
                            eproto_send_user_reply(&g_eproto, g_last_source_address, g_last_packet_id, data, data_len);
                        if (error != EPROTO_OK) {
                            printf("Failed to send reply\n");
                        } else {
                            g_needs_reply = 0;
                        }
                    }
                }
            } else if (strlen(command) > 0) {
                printf("Unknown command: %s\n", command);
                print_help();
            }
        }
        usleep(10000);
    }

    ipc_close_channel(&g_ipc_tx_channel_a);
    ipc_close_channel(&g_ipc_rx_channel_a);
    ipc_close_channel(&g_ipc_tx_channel_c);
    ipc_close_channel(&g_ipc_rx_channel_c);
    ipc_close_channel(&g_ipc_tx_channel_e);
    ipc_close_channel(&g_ipc_rx_channel_e);
    eproto_destroy(&g_eproto);

    printf("\nProcess B exiting\n");
    return 0;
}
