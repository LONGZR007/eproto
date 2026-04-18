#include "eproto.h"
#include "serial_common.h"
#include "bus_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <semaphore.h>

static eproto_t g_eproto;
static serial_channel_t* g_serial_channels;
static uint8_t g_last_source_address = 0;
static uint16_t g_last_packet_id = 0;
static int g_needs_reply = 0;

static sem_t g_semaphore;
static int g_semaphore_initialized = 0;

// 总线发送函数数组
static void (*bus_send_functions[5])(uint8_t* data, uint16_t length);

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

void device_receive_callback(uint8_t source_address, uint16_t packet_id, uint8_t* data, uint16_t length) {
    printf("Device %c: Received data from device %02X, packet ID: %d: ", DEVICE_ID, source_address, packet_id);
    for (uint16_t i = 0; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");

    g_last_source_address = source_address;
    g_last_packet_id = packet_id;
    g_needs_reply = 1;
}

void device_send_callback(eproto_send_status_t status, uint16_t packet_id, uint8_t* data, uint16_t length, void* private_data) {
    (void)private_data;
    switch (status) {
        case EPROTO_SEND_SUCCESS:
            printf("Device %c: Send success, packet ID: %d\n", DEVICE_ID, packet_id);
            if (data && length > 0) {
                printf("Device %c: Received response: ", DEVICE_ID);
                for (uint16_t i = 0; i < length; i++) {
                    printf("%02X ", data[i]);
                }
                printf("\n");
            }
            break;
        case EPROTO_SEND_TIMEOUT:
            printf("Device %c: Send timeout, packet ID: %d\n", DEVICE_ID, packet_id);
            break;
        case EPROTO_SEND_ERROR:
            printf("Device %c: Send error, packet ID: %d\n", DEVICE_ID, packet_id);
            break;
        case EPROTO_SEND_BUSY:
            printf("Device %c: Send busy, packet ID: %d\n", DEVICE_ID, packet_id);
            break;
    }
}

// 生成总线发送函数
#define GENERATE_BUS_SEND_FUNCTION(index) \
void bus##index##_send(uint8_t* data, uint16_t length) { \
    printf("Device %c bus%d sending: ", (char)DEVICE_ID, index+1); \
    for (uint16_t i = 0; i < length; i++) { \
        printf("%02X ", data[i]); \
    } \
    printf("\n"); \
    serial_send_data(&g_serial_channels[index], data, length); \
}

// 生成总线接收线程函数
#define GENERATE_RECEIVE_THREAD(index) \
void* receive_thread##index(void* arg) { \
    (void)arg; \
    uint8_t rx_buffer[1024]; \
    uint8_t bus_address = bus_configs[index].bus_address; \
    while (1) { \
        int received = serial_receive_data(&g_serial_channels[index], rx_buffer, sizeof(rx_buffer)); \
        if (received > 0) { \
            printf("Device %c received from bus%d %d bytes: ", (char)DEVICE_ID, index+1, received); \
            for (int i = 0; i < received; i++) { \
                printf("%02X ", rx_buffer[i]); \
            } \
            printf("\n"); \
            eproto_receive_data(&g_eproto, bus_address, rx_buffer, received); \
        } \
        usleep(10000); \
    } \
    return NULL; \
}

// 生成所有总线发送函数和接收线程函数
#define GENERATE_BUS_FUNCTIONS(index) \
GENERATE_BUS_SEND_FUNCTION(index) \
GENERATE_RECEIVE_THREAD(index)

// 生成总线函数（最多支持4条总线）
GENERATE_BUS_FUNCTIONS(0)
GENERATE_BUS_FUNCTIONS(1)
GENERATE_BUS_FUNCTIONS(2)
GENERATE_BUS_FUNCTIONS(3)
GENERATE_BUS_FUNCTIONS(4)

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
        "(e.g., send 2 1 11 22 33)\n"
    );
    printf(
        "  send_reply <data...> - Send reply to last received message (e.g., "
        "send_reply AA BB CC)\n"
    );
    printf("  help - Show this help message\n");
    printf("  quit - Exit the program\n");
    printf("\n");
}

int main(int argc, char *argv[]) {
    printf("=== Device %c ===\n\n", DEVICE_ID);

    // 检查命令行参数
    if (argc < 2 * bus_count + 1) {
        fprintf(stderr, "Usage: %s ", argv[0]);
        for (int i = 0; i < bus_count; i++) {
            fprintf(stderr, "<serial_port_%d> <baud_rate_%d> ", i+1, i+1);
        }
        fprintf(stderr, "\n");
        return 1;
    }

    // 初始化串口通道
    g_serial_channels = (serial_channel_t*)malloc(sizeof(serial_channel_t) * bus_count);
    if (!g_serial_channels) {
        printf("Failed to allocate memory for serial channels\n");
        return 1;
    }

    // 打开串口
    for (int i = 0; i < bus_count; i++) {
        serial_init_channel(&g_serial_channels[i]);
        if (serial_open(&g_serial_channels[i], argv[2*i + 1], atoi(argv[2*i + 2]))) {
            printf("Failed to open serial port for bus %d\n", i+1);
            free(g_serial_channels);
            return 1;
        }
        printf("Serial port %s opened at %s baud for bus %d\n", 
               argv[2*i + 1], argv[2*i + 2], i+1);
    }

    // 初始化总线发送函数数组
    if (bus_count > 0) bus_send_functions[0] = bus0_send;
    if (bus_count > 1) bus_send_functions[1] = bus1_send;
    if (bus_count > 2) bus_send_functions[2] = bus2_send;
    if (bus_count > 3) bus_send_functions[3] = bus3_send;
    if (bus_count > 4) bus_send_functions[4] = bus4_send;

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
        for (int i = 0; i < bus_count; i++) {
            serial_close(&g_serial_channels[i]);
        }
        free(g_serial_channels);
        return 1;
    }
    printf("eProto initialized successfully\n");

    // 初始化总线
    uint8_t rx_buffers[5][256];
    for (int i = 0; i < bus_count; i++) {
        eproto_bus_t bus = {.send = bus_send_functions[i], .receive = NULL};
        uint8_t bus_address = bus_configs[i].bus_address;
        int target_count = bus_configs[i].target_count;
        uint8_t* target_addresses = bus_configs[i].target_addresses;
        char bus_name[16];
        sprintf(bus_name, "bus%d", i+1);
        
        error = eproto_add_bus(&g_eproto, bus_address, &bus, 
                              rx_buffers[i], sizeof(rx_buffers[i]), 
                              bus_name, mock_wakeup,
                              mock_status_callback, device_receive_callback);
        if (error != EPROTO_OK) {
            printf("Failed to add bus %d\n", i+1);
            for (int j = 0; j < bus_count; j++) {
                serial_close(&g_serial_channels[j]);
            }
            free(g_serial_channels);
            eproto_destroy(&g_eproto);
            return 1;
        }
        printf("Bus %d added successfully\n", i+1);

        // 添加目标设备
        for (int j = 0; j < target_count; j++) {
            error = eproto_add_destination_device(&g_eproto, 
                                                bus_address, 
                                                target_addresses[j]);
            if (error != EPROTO_OK) {
                printf("Failed to add target device 0x%02X to bus %d\n", 
                       target_addresses[j], 
                       i+1);
                for (int k = 0; k < bus_count; k++) {
                    serial_close(&g_serial_channels[k]);
                }
                free(g_serial_channels);
                eproto_destroy(&g_eproto);
                return 1;
            }
            printf("Target device 0x%02X added to bus %d successfully\n", 
                   target_addresses[j], 
                   i+1);
        }
    }

    // 创建接收线程
    pthread_t recv_threads[5];
    if (bus_count > 0) pthread_create(&recv_threads[0], NULL, receive_thread0, NULL);
    if (bus_count > 1) pthread_create(&recv_threads[1], NULL, receive_thread1, NULL);
    if (bus_count > 2) pthread_create(&recv_threads[2], NULL, receive_thread2, NULL);
    if (bus_count > 3) pthread_create(&recv_threads[3], NULL, receive_thread3, NULL);
    if (bus_count > 4) pthread_create(&recv_threads[4], NULL, receive_thread4, NULL);

    // 创建协议线程
    pthread_t proto_thread;
    pthread_create(&proto_thread, NULL, protocol_thread, NULL);

    printf("\nDevice %c ready!\n", DEVICE_ID);
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

                                error = eproto_send(&g_eproto, device_addr, data, data_len, device_send_callback, NULL,
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

                        error = eproto_send_user_reply(&g_eproto, g_last_source_address, g_last_packet_id, data, data_len);
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

    // 清理资源
    for (int i = 0; i < bus_count; i++) {
        serial_close(&g_serial_channels[i]);
    }
    free(g_serial_channels);
    eproto_destroy(&g_eproto);

    printf("\nDevice %c exiting\n", DEVICE_ID);
    return 0;
}