#include "eproto.h"
#include "ipc_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/time.h>

static eproto_t g_eproto;
static ipc_channel_t g_ipc_tx_channel;
static ipc_channel_t g_ipc_rx_channel;
static uint8_t g_last_source_address = 0;
static uint16_t g_last_packet_id = 0;
static int g_needs_reply = 0;

void* mock_malloc(size_t size) {
    return malloc(size);
}

void mock_free(void* ptr) {
    free(ptr);
}

static eproto_signal_result_t mock_signal_wait(uint32_t timeout_ms) {
    usleep(timeout_ms * 1000);
    return EPROTO_SIGNAL_DATA;
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

void a_receive_callback(uint8_t source_address, uint16_t packet_id, uint8_t* data, uint16_t length) {
    printf("Process A: Received data from device %02X, packet ID: %d: ", source_address, packet_id);
    for (uint16_t i = 0; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");

    g_last_source_address = source_address;
    g_last_packet_id = packet_id;
    g_needs_reply = 1;
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

void a_send_callback(eproto_send_status_t status, uint16_t packet_id, uint8_t* data, uint16_t length,
                     void* private_data) {
    (void)private_data;
    switch (status) {
        case EPROTO_SEND_SUCCESS:
            printf("Process A: Send success, packet ID: %d\n", packet_id);
            if (data && length > 0) {
                printf("Process A: Received response: ");
                for (uint16_t i = 0; i < length; i++) {
                    printf("%02X ", data[i]);
                }
                printf("\n");
            }
            break;
        case EPROTO_SEND_TIMEOUT:
            printf("Process A: Send timeout, packet ID: %d\n", packet_id);
            break;
        case EPROTO_SEND_ERROR:
            printf("Process A: Send error, packet ID: %d\n", packet_id);
            break;
        case EPROTO_SEND_BUSY:
            printf("Process A: Send busy, packet ID: %d\n", packet_id);
            break;
    }
}

void a_bus_send(uint8_t* data, uint16_t length) {
    printf("Process A sending: ");
    for (uint16_t i = 0; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");

    ipc_send_data(&g_ipc_tx_channel, data, length);
}

void* receive_thread(void* arg) {
    (void)arg;
    uint8_t rx_buffer[1024];

    while (1) {
        int received = ipc_receive_data(&g_ipc_rx_channel, rx_buffer, sizeof(rx_buffer));
        if (received > 0) {
            printf("Process A received %d bytes: ", received);
            for (int i = 0; i < received; i++) {
                printf("%02X ", rx_buffer[i]);
            }
            printf("\n");

            for (int i = 0; i < received; i++) {
                eproto_receive_byte(&g_eproto, 0x01, rx_buffer[i]);
            }
        }
        usleep(10000);
    }
    return NULL;
}

void print_help(void) {
    printf("\nAvailable commands:\n");
    printf(
        "  send <device_addr> <need_reply> <data...> - Send data to device "
        "(e.g., send 3 1 11 22 33)\n");
    printf(
        "  send_reply <data...> - Send reply to last received message (e.g., "
        "send_reply AA BB CC)\n");
    printf("  help - Show this help message\n");
    printf("  quit - Exit the program\n");
    printf("\n");
}

int main(void) {
    printf("=== Process A ===\n\n");

    ipc_init_channel(&g_ipc_tx_channel);
    ipc_init_channel(&g_ipc_rx_channel);

    eproto_user_functions_t user_functions = {.malloc = mock_malloc,
                                              .free = mock_free,
                                              .signal_wait = mock_signal_wait,
                                              .lock = mock_lock,
                                              .unlock = mock_unlock,
                                              .get_timestamp = mock_get_timestamp};

    eproto_error_t error = eproto_init(&g_eproto, &user_functions);
    if (error != EPROTO_OK) {
        printf("Failed to initialize eProto\n");
        return 1;
    }
    printf("eProto initialized successfully\n");

    eproto_bus_t a_bus = {.send = a_bus_send, .receive = NULL};

    uint8_t a_rx_buffer[256];
    error = eproto_add_bus(&g_eproto, 0x01, &a_bus, a_rx_buffer, sizeof(a_rx_buffer), "a_bus", mock_wakeup,
                           mock_status_callback, a_receive_callback);
    if (error != EPROTO_OK) {
        printf("Failed to add bus\n");
        return 1;
    }
    printf("Bus added successfully\n");

    error = eproto_add_destination_device(&g_eproto, 0x01, 0x02);
    if (error != EPROTO_OK) {
        printf("Failed to add target device 0x02\n");
        return 1;
    }
    printf("Target device 0x02 added successfully\n");

    error = eproto_add_destination_device(&g_eproto, 0x01, 0x04);
    if (error != EPROTO_OK) {
        printf("Failed to add target device 0x04\n");
        return 1;
    }
    printf("Target device 0x04 added successfully\n");

    printf("Opening FIFO channels...\n");
    if (ipc_open_fifo(&g_ipc_tx_channel, FIFO_PATH_A_TO_B) < 0) {
        printf("Failed to open TX FIFO\n");
        return 1;
    }

    if (ipc_open_fifo(&g_ipc_rx_channel, FIFO_PATH_B_TO_A) < 0) {
        printf("Failed to open RX FIFO\n");
        return 1;
    }

    pthread_t recv_thread;
    pthread_create(&recv_thread, NULL, receive_thread, NULL);

    printf("\nProcess A ready!\n");
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

                                error = eproto_send(&g_eproto, device_addr, data, data_len, a_send_callback, NULL,
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

        eproto_tick(&g_eproto);
        usleep(10000);
    }

    ipc_close_channel(&g_ipc_tx_channel);
    ipc_close_channel(&g_ipc_rx_channel);
    ipc_cleanup_fifos();
    eproto_destroy(&g_eproto);

    printf("\nProcess A exiting\n");
    return 0;
}
