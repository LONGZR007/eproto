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
static ipc_channel_t g_ipc_tx_channel_b;
static ipc_channel_t g_ipc_rx_channel_b;
static ipc_channel_t g_ipc_tx_channel_d;
static ipc_channel_t g_ipc_rx_channel_d;
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
        case EPROTO_STATUS_SLEEP_SUCCESS:
            printf("Status: Sleep success\n");
            break;
        case EPROTO_STATUS_SLEEP_FAILED:
            printf("Status: Sleep failed\n");
            break;
        case EPROTO_STATUS_WAKEUP_SUCCESS:
            printf("Status: Wakeup success\n");
            break;
        case EPROTO_STATUS_WAKEUP_FAILED:
            printf("Status: Wakeup failed\n");
            break;
        case EPROTO_STATUS_MULTIPLE_CRC_ERRORS:
            printf("Status: Multiple CRC errors\n");
            break;
        case EPROTO_STATUS_HANDSHAKE_SUCCESS:
            printf("Status: Handshake success\n");
            break;
    }
}

void e_receive_callback(uint8_t source_address, uint16_t packet_id, uint8_t* data, uint16_t length) {
    printf("Process E: Received data from device %02X, packet ID: %d: ", source_address, packet_id);
    for (uint16_t i = 0; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");

    g_last_source_address = source_address;
    g_last_packet_id = packet_id;
    g_needs_reply = 1;
}

void e_send_callback(eproto_send_status_t status, uint16_t packet_id, uint8_t* data, uint16_t length,
                     void* private_data) {
    (void)private_data;
    switch (status) {
        case EPROTO_SEND_SUCCESS:
            printf("Process E: Send success, packet ID: %d\n", packet_id);
            if (data && length > 0) {
                printf("Process E: Received response: ");
                for (uint16_t i = 0; i < length; i++) {
                    printf("%02X ", data[i]);
                }
                printf("\n");
            }
            break;
        case EPROTO_SEND_TIMEOUT:
            printf("Process E: Send timeout, packet ID: %d\n", packet_id);
            break;
        case EPROTO_SEND_ERROR:
            printf("Process E: Send error, packet ID: %d\n", packet_id);
            break;
        case EPROTO_SEND_BUSY:
            printf("Process E: Send busy, packet ID: %d\n", packet_id);
            break;
    }
}

void e_bus11_send(uint8_t* data, uint16_t length) {
    printf("Process E bus11 sending: ");
    for (uint16_t i = 0; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");

    // 总线11发送到B
    ipc_send_data(&g_ipc_tx_channel_b, data, length);
}

void e_bus12_send(uint8_t* data, uint16_t length) {
    printf("Process E bus12 sending: ");
    for (uint16_t i = 0; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");

    // 总线12发送到D
    ipc_send_data(&g_ipc_tx_channel_d, data, length);
}

void* receive_thread_b(void* arg) {
    (void)arg;
    uint8_t rx_buffer[1024];

    while (1) {
        int received = ipc_receive_data(&g_ipc_rx_channel_b, rx_buffer, sizeof(rx_buffer));
        if (received > 0) {
            printf("Process E received from B %d bytes: ", received);
            for (int i = 0; i < received; i++) {
                printf("%02X ", rx_buffer[i]);
            }
            printf("\n");

            eproto_receive_data(&g_eproto, 0x0B, rx_buffer, received);  // 总线11接收来自B的数据
        }
        usleep(10000);
    }
    return NULL;
}

void* receive_thread_d(void* arg) {
    (void)arg;
    uint8_t rx_buffer[1024];

    while (1) {
        int received = ipc_receive_data(&g_ipc_rx_channel_d, rx_buffer, sizeof(rx_buffer));
        if (received > 0) {
            printf("Process E received from D %d bytes: ", received);
            for (int i = 0; i < received; i++) {
                printf("%02X ", rx_buffer[i]);
            }
            printf("\n");

            eproto_receive_data(&g_eproto, 0x0C, rx_buffer, received);  // 总线12接收来自D的数据
        }
        usleep(10000);
    }
    return NULL;
}

void* protocol_thread(void* arg) {
    (void)arg;
    printf("Protocol thread started\n");

    while (1) {
        eproto_tick(&g_eproto);
    }
    return NULL;
}

void print_help(void) {
    printf("\nAvailable commands:\n");
    printf(
        "  send <device_addr> <need_reply> <data...> - Send data to device "
        "(e.g., send 2 1 11 22 33)\n");
    printf(
        "  send_reply <data...> - Send reply to last received message (e.g., "
        "send_reply AA BB CC)\n");
    printf("  help - Show this help message\n");
    printf("  quit - Exit the program\n");
    printf("\n");
}

int main(void) {
    printf("=== Process E ===\n\n");

    ipc_init_channel(&g_ipc_tx_channel_b);
    ipc_init_channel(&g_ipc_rx_channel_b);
    ipc_init_channel(&g_ipc_tx_channel_d);
    ipc_init_channel(&g_ipc_rx_channel_d);

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

    // 总线11（连接到B5、A1、C6）
    eproto_bus_t e_bus11 = {.send = e_bus11_send, .receive = NULL};

    uint8_t e_rx_buffer11[256];
    error = eproto_add_bus(&g_eproto, 0x0B, &e_bus11, e_rx_buffer11, sizeof(e_rx_buffer11), "e_bus11", mock_wakeup,
                           mock_status_callback);
    if (error != EPROTO_OK) {
        printf("Failed to add bus 11\n");
        return 1;
    }
    printf("Bus 11 added successfully\n");

    // 总线12（连接到D10）
    eproto_bus_t e_bus12 = {.send = e_bus12_send, .receive = NULL};

    uint8_t e_rx_buffer12[256];
    error = eproto_add_bus(&g_eproto, 0x0C, &e_bus12, e_rx_buffer12, sizeof(e_rx_buffer12), "e_bus12", mock_wakeup,
                           mock_status_callback);
    if (error != EPROTO_OK) {
        printf("Failed to add bus 12\n");
        return 1;
    }
    printf("Bus 12 added successfully\n");

    // 添加目标设备
    // 总线11挂载到B5、A1、C6
    error = eproto_add_destination_device(&g_eproto, 0x0B, 0x05);  // B
    if (error != EPROTO_OK) {
        printf("Failed to add target device 0x05\n");
        return 1;
    }
    printf("Target device 0x05 added successfully\n");

    error = eproto_add_destination_device(&g_eproto, 0x0B, 0x01);  // A
    if (error != EPROTO_OK) {
        printf("Failed to add target device 0x01\n");
        return 1;
    }
    printf("Target device 0x01 added successfully\n");

    error = eproto_add_destination_device(&g_eproto, 0x0B, 0x06);  // C
    if (error != EPROTO_OK) {
        printf("Failed to add target device 0x06\n");
        return 1;
    }
    printf("Target device 0x06 added successfully\n");

    // 总线12挂载到D10
    error = eproto_add_destination_device(&g_eproto, 0x0C, 0x0A);  // D
    if (error != EPROTO_OK) {
        printf("Failed to add target device 0x0A\n");
        return 1;
    }
    printf("Target device 0x0A added successfully\n");

    printf("Opening FIFO channels...\n");
    if (ipc_open_fifo(&g_ipc_tx_channel_b, FIFO_PATH_E_TO_B) < 0) {
        printf("Failed to open TX FIFO to B\n");
        return 1;
    }

    if (ipc_open_fifo(&g_ipc_rx_channel_b, FIFO_PATH_B_TO_E) < 0) {
        printf("Failed to open RX FIFO from B\n");
        return 1;
    }

    if (ipc_open_fifo(&g_ipc_tx_channel_d, FIFO_PATH_E_TO_D) < 0) {
        printf("Failed to open TX FIFO to D\n");
        return 1;
    }

    if (ipc_open_fifo(&g_ipc_rx_channel_d, FIFO_PATH_D_TO_E) < 0) {
        printf("Failed to open RX FIFO from D\n");
        return 1;
    }

    pthread_t recv_thread_b, recv_thread_d, proto_thread;
    pthread_create(&recv_thread_b, NULL, receive_thread_b, NULL);
    pthread_create(&recv_thread_d, NULL, receive_thread_d, NULL);
    pthread_create(&proto_thread, NULL, protocol_thread, NULL);

    printf("\nProcess E ready!\n");
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

                                error = eproto_send(&g_eproto, device_addr, data, data_len, e_send_callback, NULL,
                                                    need_reply ? 0 : 1);
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

    ipc_close_channel(&g_ipc_tx_channel_b);
    ipc_close_channel(&g_ipc_rx_channel_b);
    ipc_close_channel(&g_ipc_tx_channel_d);
    ipc_close_channel(&g_ipc_rx_channel_d);
    eproto_destroy(&g_eproto);

    printf("\nProcess E exiting\n");
    return 0;
}