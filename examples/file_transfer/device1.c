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

extern int g_transfer_complete;
extern pthread_mutex_t g_file_mutex;

static const char* g_test_filename = "test_file.bin";
uint8_t g_test_file_data[512];
uint32_t g_test_file_size = 0;

static void create_test_file(void) {
    for (uint32_t i = 0; i < sizeof(g_test_file_data); i++) {
        g_test_file_data[i] = (uint8_t)(i & 0xFF);
    }
    g_test_file_size = sizeof(g_test_file_data);
    printf("Device 1: Created test file '%s' with %u bytes\n", g_test_filename, g_test_file_size);
}

static void* device1_receive_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    printf("%s receive thread started\n", data->device_name);
    fflush(stdout);

    g_current_thread_data = data;

    for (int i = 0; i < 200; i++) {
        uint8_t rx_buffer[512];
        uint16_t rx_count = device1_bus_receive(rx_buffer, sizeof(rx_buffer));
        if (rx_count > 0) {
            eproto_receive_data(&data->eproto_inst, 0x01, rx_buffer, rx_count);
        }
        usleep(5000);
    }

    printf("%s receive thread finished\n", data->device_name);
    fflush(stdout);
    pthread_exit(NULL);
}

static void* device1_process_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    printf("%s process thread started\n", data->device_name);
    fflush(stdout);

    g_current_thread_data = data;

    usleep(10000);

    create_test_file();

    printf("\n=== Device 1: Starting File Transfer Demo ===\n");
    printf("Filename: %s, Size: %u bytes\n\n", g_test_filename, g_test_file_size);

    uint16_t session_id;
    eproto_upper_error_t err = eproto_upper_create_session(&data->upper_ctx, &session_id);
    if (err != EPROTO_UPPER_OK) {
        printf("Device 1: Failed to create session: %d\n", err);
        pthread_exit(NULL);
    }
    printf("Device 1: Created session %u\n", session_id);

    eproto_upper_session_t* session = eproto_upper_get_session(&data->upper_ctx, session_id);
    if (session) {
        session->file_size = g_test_file_size;
        session->crc_type = EPROTO_UPPER_FILE_CRC_CRC32;
        session->state = EPROTO_UPPER_SESSION_WAIT_RESPONSE;
        session->transfer_state = EPROTO_UPPER_TRANSFER_SENDING;
        memcpy(session->filename, g_test_filename, strlen(g_test_filename));
        session->filename_length = strlen(g_test_filename);
    }

    eproto_upper_packet_t start_packet;
    err = eproto_upper_build_file_start_req(&data->upper_ctx, session_id, g_test_filename,
        g_test_file_size, EPROTO_UPPER_FILE_CRC_CRC32, 1, 0, &start_packet);
    if (err != EPROTO_UPPER_OK) {
        printf("Device 1: Failed to build start request: %d\n", err);
        pthread_exit(NULL);
    }

    uint8_t total_len = EPROTO_UPPER_HEADER_SIZE + start_packet.data_length;
    uint8_t* tx_data = (uint8_t*)malloc(total_len);
    if (!tx_data) {
        printf("Device 1: Memory allocation failed\n");
        pthread_exit(NULL);
    }

    tx_data[0] = start_packet.flags;
    tx_data[1] = start_packet.func_code;
    tx_data[2] = (uint8_t)(session_id & 0xFF);
    tx_data[3] = (uint8_t)((session_id >> 8) & 0xFF);
    tx_data[4] = (uint8_t)(start_packet.seq_num & 0xFF);
    tx_data[5] = (uint8_t)((start_packet.seq_num >> 8) & 0xFF);
    tx_data[6] = (uint8_t)(start_packet.data_length & 0xFF);
    tx_data[7] = (uint8_t)((start_packet.data_length >> 8) & 0xFF);
    memcpy(&tx_data[8], start_packet.data, start_packet.data_length);

    printf("Device 1: Sending FILE_START_REQ...\n");
    eproto_error_t send_err = eproto_send(&data->eproto_inst, 0x02, tx_data, total_len,
        device_send_callback, NULL, 1);
    if (send_err != EPROTO_OK) {
        printf("Device 1: Failed to send start request\n");
        free(tx_data);
        pthread_exit(NULL);
    }
    free(tx_data);

    usleep(50000);

    printf("\nDevice 1: Sending file data...\n");
    uint32_t crc_value = eproto_upper_calculate_crc32(g_test_file_data, g_test_file_size);
    printf("Device 1: File CRC32 = 0x%08X\n", crc_value);

    eproto_upper_packet_t data_packets[16];
    uint8_t packet_count = 0;
    err = eproto_upper_build_file_data(&data->upper_ctx, session_id, 0, g_test_file_data,
        g_test_file_size, data_packets, &packet_count);
    if (err != EPROTO_UPPER_OK) {
        printf("Device 1: Failed to build data packets: %d\n", err);
        pthread_exit(NULL);
    }

    for (uint8_t i = 0; i < packet_count; i++) {
        uint16_t total_len = EPROTO_UPPER_HEADER_SIZE + data_packets[i].data_length;
        uint8_t* tx_data = (uint8_t*)malloc(total_len);
        if (!tx_data) continue;

        tx_data[0] = data_packets[i].flags;
        tx_data[1] = data_packets[i].func_code;
        tx_data[2] = (uint8_t)(session_id & 0xFF);
        tx_data[3] = (uint8_t)((session_id >> 8) & 0xFF);
        tx_data[4] = (uint8_t)(data_packets[i].seq_num & 0xFF);
        tx_data[5] = (uint8_t)((data_packets[i].seq_num >> 8) & 0xFF);
        tx_data[6] = (uint8_t)(data_packets[i].data_length & 0xFF);
        tx_data[7] = (uint8_t)((data_packets[i].data_length >> 8) & 0xFF);
        memcpy(&tx_data[8], data_packets[i].data, data_packets[i].data_length);

        printf("Device 1: Sending FILE_DATA packet %u/%u (%u bytes)...\n",
            i + 1, packet_count, data_packets[i].data_length - 6);

        eproto_send(&data->eproto_inst, 0x02, tx_data, total_len, device_send_callback, NULL, 1);
        free(tx_data);
        usleep(10000);
    }

    usleep(30000);

    printf("\nDevice 1: Sending FILE_END...\n");
    eproto_upper_packet_t end_packet;
    err = eproto_upper_build_file_end(&data->upper_ctx, session_id, crc_value,
        EPROTO_UPPER_STATUS_SUCCESS, &end_packet);
    if (err != EPROTO_UPPER_OK) {
        printf("Device 1: Failed to build end packet: %d\n", err);
        pthread_exit(NULL);
    }

    total_len = EPROTO_UPPER_HEADER_SIZE + end_packet.data_length;
    tx_data = (uint8_t*)malloc(total_len);
    if (!tx_data) {
        pthread_exit(NULL);
    }

    tx_data[0] = end_packet.flags;
    tx_data[1] = end_packet.func_code;
    tx_data[2] = (uint8_t)(session_id & 0xFF);
    tx_data[3] = (uint8_t)((session_id >> 8) & 0xFF);
    tx_data[4] = (uint8_t)(end_packet.seq_num & 0xFF);
    tx_data[5] = (uint8_t)((end_packet.seq_num >> 8) & 0xFF);
    tx_data[6] = (uint8_t)(end_packet.data_length & 0xFF);
    tx_data[7] = (uint8_t)((end_packet.data_length >> 8) & 0xFF);
    memcpy(&tx_data[8], end_packet.data, end_packet.data_length);

    eproto_send(&data->eproto_inst, 0x02, tx_data, total_len, device_send_callback, NULL, 1);
    free(tx_data);

    for (int i = 0; i < 200; i++) {
        eproto_process(&data->eproto_inst);

        pthread_mutex_lock(&g_file_mutex);
        int complete = g_transfer_complete;
        pthread_mutex_unlock(&g_file_mutex);

        if (complete) {
            printf("[Device 1] Transfer completed, exiting...\n");
            break;
        }
        usleep(10000);
    }

    printf("\n=== Device 1: File Transfer Complete ===\n");
    printf("%s process thread finished\n", data->device_name);
    fflush(stdout);
    pthread_exit(NULL);
}

void* device1_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    printf("%s thread started\n", data->device_name);
    fflush(stdout);

    eproto_user_functions_t user_functions = {
        .malloc = mock_malloc,
        .free = mock_free,
        .signal_wait = device_signal_wait,
        .signal_send = device_signal_send,
        .lock = mock_lock,
        .unlock = mock_unlock,
        .get_timestamp = mock_get_timestamp,
        .timeout_timestamp = 0
    };

    eproto_error_t error = eproto_init(&data->eproto_inst, &user_functions);
    if (error != EPROTO_OK) {
        printf("%s: Failed to initialize eProto\n", data->device_name);
        fflush(stdout);
        pthread_exit(NULL);
    }
    printf("%s: eProto initialized\n", data->device_name);
    fflush(stdout);

    eproto_upper_init(&data->upper_ctx);
    eproto_upper_set_functions(&data->upper_ctx, NULL, NULL, mock_lock, mock_unlock, mock_get_timestamp);

    eproto_upper_callbacks_t callbacks = {
        .on_file_start_req = upper_file_start_req_callback,
        .on_file_start_rsp = upper_file_start_rsp_callback,
        .on_file_end = upper_file_end_callback,
        .on_file_data = upper_file_data_callback,
        .on_progress = upper_progress_callback,
        .on_packet_received = NULL,
        .user_data = data
    };
    eproto_upper_set_callbacks(&data->upper_ctx, &callbacks);

    g_device1_eproto = &data->eproto_inst;
    g_device1_upper_ctx = &data->upper_ctx;

    eproto_bus_t bus = {
        .self_addr = 0x01,
        .send = device1_bus_send,
        .rx_buffer = data->rx_buffer,
        .rx_buffer_size = sizeof(data->rx_buffer),
        .name = "device1_bus",
        .status_callback = mock_status_callback,
        .receive_callback = device1_receive_callback,
        .forward_callback = NULL
    };

    error = eproto_add_bus(&data->eproto_inst, &bus);
    if (error != EPROTO_OK) {
        printf("%s: Failed to add bus\n", data->device_name);
        fflush(stdout);
        pthread_exit(NULL);
    }
    printf("%s: Bus added\n", data->device_name);
    fflush(stdout);

    error = eproto_add_destination_device(&data->eproto_inst, 0x01, 0x02);
    if (error != EPROTO_OK) {
        printf("%s: Failed to add destination device\n", data->device_name);
        fflush(stdout);
        pthread_exit(NULL);
    }
    printf("%s: Destination device 0x02 added\n", data->device_name);
    fflush(stdout);

    pthread_t receive_thread, process_thread;

    if (pthread_create(&receive_thread, NULL, device1_receive_thread, data) != 0) {
        printf("%s: Failed to create receive thread\n", data->device_name);
        pthread_exit(NULL);
    }

    if (pthread_create(&process_thread, NULL, device1_process_thread, data) != 0) {
        printf("%s: Failed to create process thread\n", data->device_name);
        pthread_exit(NULL);
    }

    pthread_join(receive_thread, NULL);
    pthread_join(process_thread, NULL);

    eproto_upper_deinit(&data->upper_ctx);
    eproto_destroy(&data->eproto_inst);

    printf("%s thread finished\n", data->device_name);
    pthread_exit(NULL);
}
