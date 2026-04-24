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

void mock_status_callback(eproto_bus_t* bus, eproto_status_t status, uint8_t* data, uint16_t length) {
    (void)bus;
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

void d_receive_callback(eproto_bus_t* bus, uint8_t source_address, uint16_t packet_id, uint8_t* data, uint16_t length) {
    (void)bus;
    printf("Process D: Received data from device %02X, packet ID: %d: ", source_address, packet_id);
    for (uint16_t i = 0; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");

    g_last_source_address = source_address;
    g_last_packet_id = packet_id;
    g_needs_reply = 1;
}

void d_send_callback(eproto_send_status_t status, uint16_t packet_id, uint8_t* data, uint16_t length,
                     void* private_data) {
    (void)private_data;
    switch (status) {
        case EPROTO_SEND_SUCCESS:
            printf("Process D: Send success, packet ID: %d\n", packet_id);
            if (data && length > 0) {
                printf("Process D: Received response: ");
                for (uint16_t i = 0; i < length; i++) {
                    printf("%02X ", data[i]);
                }
                printf("\n");
            }
            break;
        case EPROTO_SEND_TIMEOUT:
            printf("Process D: Send timeout, packet ID: %d\n", packet_id);
            break;
        case EPROTO_SEND_ERROR:
            printf("Process D: Send error, packet ID: %d\n", packet_id);
            break;
        case EPROTO_SEND_BUSY:
            printf("Process D: Send busy, packet ID: %d\n", packet_id);
            break;
    }
}

void d_bus8_send(eproto_bus_t* bus, uint8_t* data, uint16_t length) {
    (void)bus;
    printf("Process D bus8 sending: ");
    for (uint16_t i = 0; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");

    // 总线8发送到A
    ipc_send_data(&g_ipc_tx_channel_a, data, length);
}

void d_bus9_send(eproto_bus_t* bus, uint8_t* data, uint16_t length) {
    (void)bus;
    printf("Process D bus9 sending: ");
    for (uint16_t i = 0; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");

    // 总线9发送到C
    ipc_send_data(&g_ipc_tx_channel_c, data, length);
}

void d_bus10_send(eproto_bus_t* bus, uint8_t* data, uint16_t length) {
    (void)bus;
    printf("Process D bus10 sending: ");
    for (uint16_t i = 0; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");

    // 总线10发送到E
    ipc_send_data(&g_ipc_tx_channel_e, data, length);
}

void* receive_thread_c(void* arg) {
    (void)arg;
    uint8_t rx_buffer[1024];

    while (1) {
        int received = ipc_receive_data(&g_ipc_rx_channel_c, rx_buffer, sizeof(rx_buffer));
        if (received > 0) {
            printf("Process D received from C %d bytes: ", received);
            for (int i = 0; i < received; i++) {
                printf("%02X ", rx_buffer[i]);
            }
            printf("\n");

            eproto_receive_data(&g_eproto, 0x09, rx_buffer, received);  // 总线9接收来自C的数据
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
            printf("Process D received from E %d bytes: ", received);
            for (int i = 0; i < received; i++) {
                printf("%02X ", rx_buffer[i]);
            }
            printf("\n");

            eproto_receive_data(&g_eproto, 0x0A, rx_buffer, received);  // 总线10接收来自E的数据
        }
        usleep(10000);
    }
    return NULL;
}

void* receive_thread_a(void* arg) {
    (void)arg;
    uint8_t rx_buffer[1024];

    while (1) {
        int received = ipc_receive_data(&g_ipc_rx_channel_a, rx_buffer, sizeof(rx_buffer));
        if (received > 0) {
            printf("Process D received from A %d bytes: ", received);
            for (int i = 0; i < received; i++) {
                printf("%02X ", rx_buffer[i]);
            }
            printf("\n");

            eproto_receive_data(&g_eproto, 0x08, rx_buffer, received);  // 总线8接收来自A的数据
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
        "(e.g., send 3 1 11 22 33)\n");
    printf(
        "  send_reply <data...> - Send reply to last received message (e.g., "
        "send_reply AA BB CC)\n");
    printf("  help - Show this help message\n");
    printf("  quit - Exit the program\n");
    printf("\n");
}

int main(void) {
    printf("=== Process D ===\n\n");

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

    // 总线8（连接到A2、B3）
    uint8_t d_rx_buffer8[256];
    eproto_bus_t bus8 = {
        .self_addr = 0x08,
        .send = d_bus8_send,
        .rx_buffer = d_rx_buffer8,
        .rx_buffer_size = sizeof(d_rx_buffer8),
        .name = "d_bus8",
        .user_data = NULL,
        .status_callback = mock_status_callback,
        .receive_callback = d_receive_callback,
        .forward_callback = NULL
    };
    error = eproto_add_bus(&g_eproto, &bus8);
    if (error != EPROTO_OK) {
        printf("Failed to add bus 8\n");
        return 1;
    }
    printf("Bus 8 added successfully\n");

    // 总线9（连接到C7）
    uint8_t d_rx_buffer9[256];
    eproto_bus_t bus9 = {
        .self_addr = 0x09,
        .send = d_bus9_send,
        .rx_buffer = d_rx_buffer9,
        .rx_buffer_size = sizeof(d_rx_buffer9),
        .name = "d_bus9",
        .user_data = NULL,
        .status_callback = mock_status_callback,
        .receive_callback = d_receive_callback,
        .forward_callback = NULL
    };
    error = eproto_add_bus(&g_eproto, &bus9);
    if (error != EPROTO_OK) {
        printf("Failed to add bus 9\n");
        return 1;
    }
    printf("Bus 9 added successfully\n");

    // 总线10（连接到E12）
    uint8_t d_rx_buffer10[256];
    eproto_bus_t bus10 = {
        .self_addr = 0x0A,
        .send = d_bus10_send,
        .rx_buffer = d_rx_buffer10,
        .rx_buffer_size = sizeof(d_rx_buffer10),
        .name = "d_bus10",
        .user_data = NULL,
        .status_callback = mock_status_callback,
        .receive_callback = d_receive_callback,
        .forward_callback = NULL
    };
    error = eproto_add_bus(&g_eproto, &bus10);
    if (error != EPROTO_OK) {
        printf("Failed to add bus 10\n");
        return 1;
    }
    printf("Bus 10 added successfully\n");

    // 添加目标设备
    // 总线8挂载到A2、B3
    error = eproto_add_destination_device(&g_eproto, 0x08, 0x02);  // A
    if (error != EPROTO_OK) {
        printf("Failed to add target device 0x02\n");
        return 1;
    }
    printf("Target device 0x02 added successfully\n");

    error = eproto_add_destination_device(&g_eproto, 0x08, 0x03);  // B
    if (error != EPROTO_OK) {
        printf("Failed to add target device 0x03\n");
        return 1;
    }
    printf("Target device 0x03 added successfully\n");

    // 总线9挂载到C7
    error = eproto_add_destination_device(&g_eproto, 0x09, 0x07);  // C
    if (error != EPROTO_OK) {
        printf("Failed to add target device 0x07\n");
        return 1;
    }
    printf("Target device 0x07 added successfully\n");

    // 总线10挂载到E12
    error = eproto_add_destination_device(&g_eproto, 0x0A, 0x0C);  // E
    if (error != EPROTO_OK) {
        printf("Failed to add target device 0x0C\n");
        return 1;
    }
    printf("Target device 0x0C added successfully\n");

    printf("Opening FIFO channels...\n");
    if (ipc_open_fifo(&g_ipc_tx_channel_a, FIFO_PATH_D_TO_A) < 0) {
        printf("Failed to open TX FIFO to A\n");
        return 1;
    }

    if (ipc_open_fifo(&g_ipc_rx_channel_a, FIFO_PATH_A_TO_D) < 0) {
        printf("Failed to open RX FIFO from A\n");
        return 1;
    }

    if (ipc_open_fifo(&g_ipc_tx_channel_c, FIFO_PATH_D_TO_C) < 0) {
        printf("Failed to open TX FIFO to C\n");
        return 1;
    }

    if (ipc_open_fifo(&g_ipc_rx_channel_c, FIFO_PATH_C_TO_D) < 0) {
        printf("Failed to open RX FIFO from C\n");
        return 1;
    }

    if (ipc_open_fifo(&g_ipc_tx_channel_e, FIFO_PATH_D_TO_E) < 0) {
        printf("Failed to open TX FIFO to E\n");
        return 1;
    }

    if (ipc_open_fifo(&g_ipc_rx_channel_e, FIFO_PATH_E_TO_D) < 0) {
        printf("Failed to open RX FIFO from E\n");
        return 1;
    }

    pthread_t recv_thread_a, recv_thread_c, recv_thread_e, proto_thread;
    pthread_create(&recv_thread_a, NULL, receive_thread_a, NULL);
    pthread_create(&recv_thread_c, NULL, receive_thread_c, NULL);
    pthread_create(&recv_thread_e, NULL, receive_thread_e, NULL);
    pthread_create(&proto_thread, NULL, protocol_thread, NULL);

    printf("\nProcess D ready!\n");
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

                                error = eproto_send(&g_eproto, device_addr, data, data_len, d_send_callback, NULL,
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

    printf("\nProcess D exiting\n");
    return 0;
}