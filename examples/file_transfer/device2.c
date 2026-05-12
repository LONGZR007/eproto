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

extern uint8_t* g_received_file_data;
extern uint32_t g_received_file_size;
extern pthread_mutex_t g_file_mutex;
extern int g_transfer_complete;

static uint32_t g_current_session_id = 0;
static uint32_t g_current_file_size = 0;
static uint32_t g_received_crc = 0;
static uint8_t g_current_crc_type = EPROTO_UPPER_FILE_CRC_CRC32;

static void* device2_receive_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    printf("%s receive thread started\n", data->device_name);
    fflush(stdout);

    g_current_thread_data = data;

    for (int i = 0; i < 300; i++) {
        uint8_t rx_buffer[512];
        uint16_t rx_count = device2_bus_receive(rx_buffer, sizeof(rx_buffer));
        if (rx_count > 0) {
            eproto_receive_data(&data->eproto_inst, 0x02, rx_buffer, rx_count);
        }
        usleep(5000);
    }

    printf("%s receive thread finished\n", data->device_name);
    fflush(stdout);
    pthread_exit(NULL);
}

static void upper_recv_callback(uint8_t src_addr, eproto_upper_packet_t* packet, void* user_data) {
    (void)src_addr;
    thread_data_t* data = (thread_data_t*)user_data;

    switch (packet->func_code) {
        case EPROTO_UPPER_FUNC_FILE_START_REQ: {
            if (packet->data_length < EPROTO_UPPER_FILE_START_REQ_MIN_SIZE) {
                return;
            }

            uint8_t filename_len = packet->data[0];
            char filename[128] = {0};
            if (filename_len > 127) filename_len = 127;
            memcpy(filename, &packet->data[1], filename_len);
            filename[filename_len] = '\0';

            uint32_t file_size = 0;
            memcpy(&file_size, &packet->data[1 + filename_len], 4);
            uint8_t crc_type = packet->data[1 + filename_len + 4];

            g_current_session_id = packet->session_id;
            g_current_file_size = file_size;
            g_current_crc_type = crc_type;

            printf("\n[RECV] FILE_START_REQ - Session: %u, Filename: '%s', Size: %u, CRC: %u\n",
                   packet->session_id, filename, file_size, crc_type);

            eproto_upper_packet_t rsp_packet;
            eproto_upper_build_file_start_rsp(&data->upper_ctx, packet->session_id,
                EPROTO_UPPER_STATUS_SUCCESS, EPROTO_UPPER_FILE_CHUNK_MAX_SIZE, &rsp_packet);

            uint8_t tx_data[32];
            tx_data[0] = rsp_packet.flags;
            tx_data[1] = rsp_packet.func_code;
            tx_data[2] = (uint8_t)(packet->session_id & 0xFF);
            tx_data[3] = (uint8_t)((packet->session_id >> 8) & 0xFF);
            tx_data[4] = (uint8_t)(rsp_packet.seq_num & 0xFF);
            tx_data[5] = (uint8_t)((rsp_packet.seq_num >> 8) & 0xFF);
            tx_data[6] = (uint8_t)(rsp_packet.data_length & 0xFF);
            tx_data[7] = (uint8_t)((rsp_packet.data_length >> 8) & 0xFF);
            memcpy(&tx_data[8], rsp_packet.data, rsp_packet.data_length);

            printf("[RECV] Sending FILE_START_RSP...\n");
            eproto_send(&data->eproto_inst, 0x01, tx_data, 8 + rsp_packet.data_length,
                device_send_callback, NULL, 1);
            break;
        }

        case EPROTO_UPPER_FUNC_FILE_DATA: {
            if (packet->data_length < EPROTO_UPPER_FILE_DATA_MIN_SIZE) {
                return;
            }

            uint32_t offset;
            uint16_t data_len;
            memcpy(&offset, &packet->data[0], 4);
            memcpy(&data_len, &packet->data[4], 2);

            printf("[RECV] FILE_DATA - Session: %u, Offset: %u, Length: %u, Seq: %u\n",
                   packet->session_id, offset, data_len, packet->seq_num);

            pthread_mutex_lock(&g_file_mutex);

            if (g_received_file_data == NULL) {
                g_received_file_data = (uint8_t*)malloc(g_current_file_size > 0 ? g_current_file_size : 8192);
                if (g_received_file_data) {
                    memset(g_received_file_data, 0, g_current_file_size > 0 ? g_current_file_size : 8192);
                }
            }

            if (g_received_file_data && offset + data_len <= (g_current_file_size > 0 ? g_current_file_size : 8192)) {
                memcpy(&g_received_file_data[offset], &packet->data[6], data_len);
                g_received_file_size = offset + data_len;
            }

            pthread_mutex_unlock(&g_file_mutex);

            eproto_upper_packet_t ack_packet;
            eproto_upper_build_ack(&data->upper_ctx, packet->session_id, packet->seq_num,
                EPROTO_UPPER_STATUS_SUCCESS, &ack_packet);

            uint8_t tx_data[16];
            tx_data[0] = ack_packet.flags;
            tx_data[1] = ack_packet.func_code;
            tx_data[2] = (uint8_t)(packet->session_id & 0xFF);
            tx_data[3] = (uint8_t)((packet->session_id >> 8) & 0xFF);
            tx_data[4] = (uint8_t)(packet->seq_num & 0xFF);
            tx_data[5] = (uint8_t)((packet->seq_num >> 8) & 0xFF);
            tx_data[6] = (uint8_t)(ack_packet.data_length & 0xFF);
            tx_data[7] = (uint8_t)((ack_packet.data_length >> 8) & 0xFF);
            tx_data[8] = ack_packet.data[0];

            eproto_send(&data->eproto_inst, 0x01, tx_data, 9, device_send_callback, NULL, 0);
            break;
        }

        case EPROTO_UPPER_FUNC_FILE_END: {
            if (packet->data_length < EPROTO_UPPER_FILE_END_SIZE) {
                return;
            }

            uint8_t status = packet->data[0];
            memcpy(&g_received_crc, &packet->data[1], 4);

            printf("\n[RECV] FILE_END - Session: %u, Status: %u, CRC: 0x%08X\n",
                   packet->session_id, status, g_received_crc);

            uint32_t calc_crc = eproto_upper_calculate_crc32(g_received_file_data, g_received_file_size);
            printf("[RECV] Calculated CRC: 0x%08X, Received CRC: 0x%08X\n", calc_crc, g_received_crc);

            if (calc_crc == g_received_crc) {
                printf("[RECV] CRC Check PASSED!\n");
            } else {
                printf("[RECV] CRC Check FAILED!\n");
            }

            pthread_mutex_lock(&g_file_mutex);
            g_transfer_complete = 1;
            pthread_mutex_unlock(&g_file_mutex);
            break;
        }

        default:
            break;
    }
}

static void* device2_process_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    printf("%s process thread started\n", data->device_name);
    fflush(stdout);

    g_current_thread_data = data;

    printf("\n=== Device 2: Waiting for File Transfer ===\n\n");

    for (int i = 0; i < 300; i++) {
        eproto_process(&data->eproto_inst);

        pthread_mutex_lock(&g_file_mutex);
        int complete = g_transfer_complete;
        pthread_mutex_unlock(&g_file_mutex);

        if (complete) {
            printf("[Device 2] Transfer completed, exiting...\n");
            break;
        }
        usleep(10000);
    }

    printf("\n=== Device 2: Transfer Received ===\n");
    printf("%s process thread finished\n", data->device_name);
    fflush(stdout);
    pthread_exit(NULL);
}

void* device2_thread(void* arg) {
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
        .on_packet_received = upper_recv_callback,
        .user_data = data
    };
    eproto_upper_set_callbacks(&data->upper_ctx, &callbacks);

    g_device2_eproto = &data->eproto_inst;
    g_device2_upper_ctx = &data->upper_ctx;

    eproto_bus_t bus = {
        .self_addr = 0x02,
        .send = device2_bus_send,
        .rx_buffer = data->rx_buffer,
        .rx_buffer_size = sizeof(data->rx_buffer),
        .name = "device2_bus",
        .status_callback = mock_status_callback,
        .receive_callback = device2_receive_callback,
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

    error = eproto_add_destination_device(&data->eproto_inst, 0x02, 0x01);
    if (error != EPROTO_OK) {
        printf("%s: Failed to add destination device\n", data->device_name);
        fflush(stdout);
        pthread_exit(NULL);
    }
    printf("%s: Destination device 0x01 added\n", data->device_name);
    fflush(stdout);

    pthread_t receive_thread, process_thread;

    if (pthread_create(&receive_thread, NULL, device2_receive_thread, data) != 0) {
        printf("%s: Failed to create receive thread\n", data->device_name);
        pthread_exit(NULL);
    }

    if (pthread_create(&process_thread, NULL, device2_process_thread, data) != 0) {
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
