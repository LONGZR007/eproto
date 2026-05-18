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

#include "common_serial.h"

static uint32_t g_current_session_id = 0;
static uint32_t g_current_file_size = 0;
static uint32_t g_received_crc = 0;
static eproto_upper_context_t g_upper_ctx;
static eproto_t g_eproto;

static void receive_callback(eproto_bus_t* bus, uint8_t source_addr, uint16_t packet_id, uint8_t* data, uint16_t length) {
    (void)bus;
    (void)packet_id;
    (void)source_addr;
    printf("[Receiver] Received data length: %u\n", length);

    eproto_upper_packet_t upper_packet;
    eproto_upper_error_t err = eproto_upper_parse_packet(&g_upper_ctx, data, length, &upper_packet);
    if (err == EPROTO_UPPER_OK) {
        printf("[Receiver] Parsed upper protocol: func_code=0x%02X, session_id=%u, seq_num=%u\n",
               upper_packet.func_code, upper_packet.session_id, upper_packet.seq_num);

        switch (upper_packet.func_code) {
            case EPROTO_UPPER_FUNC_FILE_START_REQ: {
                if (upper_packet.data_length < EPROTO_UPPER_FILE_START_REQ_MIN_SIZE) {
                    break;
                }

                uint8_t filename_len = upper_packet.data[0];
                char filename[128] = {0};
                if (filename_len > 127) filename_len = 127;
                memcpy(filename, upper_packet.data + 1, filename_len);
                filename[filename_len] = '\0';

                uint32_t file_size = 0;
                memcpy(&file_size, upper_packet.data + 1 + filename_len, 4);
                uint8_t crc_type = upper_packet.data[1 + filename_len + 4];

                g_current_session_id = upper_packet.session_id;
                g_current_file_size = file_size;

                printf("\n[Receiver] FILE_START_REQ\n");
                printf("  Session: %u\n", upper_packet.session_id);
                printf("  Filename: %s\n", filename);
                printf("  Filesize: %u bytes\n", file_size);
                printf("  CRC Type: %u\n", crc_type);

                pthread_mutex_lock(&g_file_mutex);
                if (g_received_file_data) {
                    free(g_received_file_data);
                }
                g_received_file_data = (uint8_t*)malloc(file_size);
                if (g_received_file_data) {
                    memset(g_received_file_data, 0, file_size);
                }
                g_received_file_size = 0;
                g_transfer_complete = 0;
                pthread_mutex_unlock(&g_file_mutex);

                eproto_upper_packet_t rsp_packet;
                eproto_upper_build_file_start_rsp(&g_upper_ctx, upper_packet.session_id,
                    EPROTO_UPPER_STATUS_SUCCESS, EPROTO_UPPER_FILE_CHUNK_MAX_SIZE, &rsp_packet);

                uint8_t tx_buf[64];
                tx_buf[0] = rsp_packet.flags;
                tx_buf[1] = rsp_packet.func_code;
                tx_buf[2] = (uint8_t)(upper_packet.session_id & 0xFF);
                tx_buf[3] = (uint8_t)((upper_packet.session_id >> 8) & 0xFF);
                tx_buf[4] = (uint8_t)(rsp_packet.seq_num & 0xFF);
                tx_buf[5] = (uint8_t)((rsp_packet.seq_num >> 8) & 0xFF);
                tx_buf[6] = (uint8_t)(rsp_packet.data_length & 0xFF);
                tx_buf[7] = (uint8_t)((rsp_packet.data_length >> 8) & 0xFF);
                memcpy(tx_buf + 8, rsp_packet.data, rsp_packet.data_length);

                printf("[Receiver] Sending FILE_START_RSP\n");
                eproto_send(&g_eproto, 0x01, tx_buf, 8 + rsp_packet.data_length, NULL, NULL, 0);
                break;
            }

            case EPROTO_UPPER_FUNC_FILE_DATA: {
                if (upper_packet.data_length < EPROTO_UPPER_FILE_DATA_MIN_SIZE) {
                    break;
                }

                uint32_t offset;
                uint16_t data_len;
                memcpy(&offset, upper_packet.data, 4);
                memcpy(&data_len, upper_packet.data + 4, 2);

                printf("[Receiver] FILE_DATA: offset=%u, length=%u, seq=%u\n",
                       offset, data_len, upper_packet.seq_num);

                pthread_mutex_lock(&g_file_mutex);
                if (g_received_file_data && offset + data_len <= g_current_file_size) {
                    memcpy(g_received_file_data + offset, upper_packet.data + 6, data_len);
                    g_received_file_size = offset + data_len;
                }
                pthread_mutex_unlock(&g_file_mutex);

                eproto_upper_packet_t ack_packet;
                eproto_upper_build_ack(&g_upper_ctx, upper_packet.session_id, upper_packet.seq_num,
                    EPROTO_UPPER_STATUS_SUCCESS, &ack_packet);

                uint8_t ack_buf[32];
                ack_buf[0] = ack_packet.flags;
                ack_buf[1] = ack_packet.func_code;
                ack_buf[2] = (uint8_t)(upper_packet.session_id & 0xFF);
                ack_buf[3] = (uint8_t)((upper_packet.session_id >> 8) & 0xFF);
                ack_buf[4] = (uint8_t)(upper_packet.seq_num & 0xFF);
                ack_buf[5] = (uint8_t)((upper_packet.seq_num >> 8) & 0xFF);
                ack_buf[6] = (uint8_t)(ack_packet.data_length & 0xFF);
                ack_buf[7] = (uint8_t)((ack_packet.data_length >> 8) & 0xFF);
                memcpy(ack_buf + 8, ack_packet.data, ack_packet.data_length);

                eproto_send(&g_eproto, 0x01, ack_buf, 8 + ack_packet.data_length, NULL, NULL, 0);
                break;
            }

            case EPROTO_UPPER_FUNC_FILE_END: {
                if (upper_packet.data_length < EPROTO_UPPER_FILE_END_SIZE) {
                    break;
                }

                uint8_t status = upper_packet.data[0];
                memcpy(&g_received_crc, upper_packet.data + 1, 4);

                printf("\n[Receiver] FILE_END\n");
                printf("  Status: %u\n", status);
                printf("  CRC: 0x%08X\n", g_received_crc);

                pthread_mutex_lock(&g_file_mutex);
                uint32_t calc_crc = eproto_upper_calculate_crc32(g_received_file_data, g_received_file_size);
                pthread_mutex_unlock(&g_file_mutex);

                printf("[Receiver] Calculated CRC: 0x%08X\n", calc_crc);
                if (calc_crc == g_received_crc) {
                    printf("[Receiver] CRC Check PASSED!\n");
                } else {
                    printf("[Receiver] CRC Check FAILED!\n");
                }

                pthread_mutex_lock(&g_file_mutex);
                g_transfer_complete = 1;
                pthread_mutex_unlock(&g_file_mutex);
                break;
            }
        }
    } else {
        printf("[Receiver] Failed to parse upper protocol: %d\n", err);
    }
}

int main(int argc, char* argv[]) {
    const char* port = SERIAL_PORT;
    if (argc > 1) {
        port = argv[1];
    }

    printf("=============================================\n");
    printf("  eProto File Transfer - Receiver\n");
    printf("  Using Serial Port\n");
    printf("=============================================\n\n");

    printf("Opening serial port: %s\n", port);
    fflush(stdout);

    if (setup_serial_port(port, SERIAL_BAUD) != 0) {
        return 1;
    }

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

    eproto_error_t err = eproto_init(&g_eproto, &user_functions);
    if (err != EPROTO_OK) {
        printf("[Receiver] Failed to init eProto\n");
        return 1;
    }

    eproto_upper_init(&g_upper_ctx);

    eproto_bus_t bus = {
        .self_addr = 0x02,
        .send = serial_bus_send,
        .rx_buffer = malloc(256),
        .rx_buffer_size = 256,
        .name = "receiver_bus",
        .status_callback = mock_status_callback,
        .receive_callback = receive_callback,
        .forward_callback = NULL
    };

    err = eproto_add_bus(&g_eproto, &bus);
    if (err != EPROTO_OK) {
        printf("[Receiver] Failed to add bus\n");
        return 1;
    }

    err = eproto_add_destination_device(&g_eproto, 0x02, 0x01);
    if (err != EPROTO_OK) {
        printf("[Receiver] Failed to add destination\n");
        return 1;
    }

    printf("\n[Receiver] Waiting for file transfer...\n");

    for (int i = 0; i < 10000; i++) {
        uint8_t rx_buf[512];
        uint16_t rx_count = serial_bus_receive(rx_buf, sizeof(rx_buf));
        if (rx_count > 0) {
            eproto_receive_data(&g_eproto, 0x02, rx_buf, rx_count);
        }
        eproto_process(&g_eproto);

        pthread_mutex_lock(&g_file_mutex);
        int complete = g_transfer_complete;
        pthread_mutex_unlock(&g_file_mutex);

        if (complete) {
            printf("\n[Receiver] File transfer complete!\n");
            break;
        }

        usleep(10000);
    }

    printf("\n=============================================\n");
    printf("  File received!\n");
    printf("  Total bytes: %u\n", g_received_file_size);
    printf("=============================================\n");

    if (g_received_file_data) {
        printf("\n[Receiver] Saving to received_file.bin...\n");
        FILE* f = fopen("received_file.bin", "wb");
        if (f) {
            fwrite(g_received_file_data, 1, g_received_file_size, f);
            fclose(f);
            printf("[Receiver] File saved!\n");
        }
    }

    close_serial_port();
    eproto_upper_deinit(&g_upper_ctx);
    eproto_destroy(&g_eproto);

    return 0;
}
