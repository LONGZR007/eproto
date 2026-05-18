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
#include <fcntl.h>
#include <sys/stat.h>

static const char* g_test_filename = "test_file.bin";
static uint8_t g_test_file_data[512];
static uint32_t g_test_file_size = 0;
static eproto_upper_context_t g_upper_ctx;
static eproto_t g_eproto;

static void create_test_file(void) {
    for (uint32_t i = 0; i < sizeof(g_test_file_data); i++) {
        g_test_file_data[i] = (uint8_t)(i & 0xFF);
    }
    g_test_file_size = sizeof(g_test_file_data);
    printf("[Sender] Created test file '%s' with %u bytes\n", g_test_filename, g_test_file_size);
}

static void receive_callback(eproto_bus_t* bus, uint8_t source_addr, uint16_t packet_id, uint8_t* data, uint16_t length) {
    (void)bus;
    (void)source_addr;
    (void)packet_id;
    printf("[Sender] Received data length: %u\n", length);

    eproto_upper_packet_t upper_packet;
    eproto_upper_error_t err = eproto_upper_parse_packet(&g_upper_ctx, data, length, &upper_packet);
    if (err == EPROTO_UPPER_OK) {
        printf("[Sender] Parsed upper protocol: func_code=0x%02X, session_id=%u, seq_num=%u\n",
               upper_packet.func_code, upper_packet.session_id, upper_packet.seq_num);
        eproto_upper_handle_packet(&g_upper_ctx, 0x02, &upper_packet);
    } else {
        printf("[Sender] Failed to parse upper protocol\n");
    }
}

static void send_callback(eproto_send_status_t status, uint16_t packet_id, uint8_t* data, uint16_t length, void* private_data) {
    (void)data;
    (void)length;
    (void)private_data;
    (void)packet_id;
    switch (status) {
        case EPROTO_SEND_SUCCESS:
            printf("[Sender] Send success\n");
            break;
        case EPROTO_SEND_TIMEOUT:
            printf("[Sender] Send timeout\n");
            break;
        case EPROTO_SEND_ERROR:
            printf("[Sender] Send error\n");
            break;
        case EPROTO_SEND_BUSY:
            printf("[Sender] Send busy\n");
            break;
    }
}

int main(int argc, char* argv[]) {
    const char* port = SERIAL_PORT;
    if (argc > 1) {
        port = argv[1];
    }

    printf("=============================================\n");
    printf("  eProto File Transfer - Sender\n");
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
        printf("[Sender] Failed to init eProto\n");
        return 1;
    }

    eproto_upper_init(&g_upper_ctx);

    eproto_bus_t bus = {
        .self_addr = 0x01,
        .send = serial_bus_send,
        .rx_buffer = malloc(256),
        .rx_buffer_size = 256,
        .name = "sender_bus",
        .status_callback = mock_status_callback,
        .receive_callback = receive_callback,
        .forward_callback = NULL
    };

    err = eproto_add_bus(&g_eproto, &bus);
    if (err != EPROTO_OK) {
        printf("[Sender] Failed to add bus\n");
        return 1;
    }

    err = eproto_add_destination_device(&g_eproto, 0x01, 0x02);
    if (err != EPROTO_OK) {
        printf("[Sender] Failed to add destination\n");
        return 1;
    }

    create_test_file();

    printf("[Sender] Creating session...\n");
    uint16_t session_id;
    eproto_upper_error_t uerr = eproto_upper_create_session(&g_upper_ctx, &session_id);
    if (uerr != EPROTO_UPPER_OK) {
        printf("[Sender] Failed to create session\n");
        return 1;
    }
    printf("[Sender] Session created: %u\n", session_id);

    eproto_upper_session_t* session = eproto_upper_get_session(&g_upper_ctx, session_id);
    if (session) {
        session->file_size = g_test_file_size;
        session->crc_type = EPROTO_UPPER_FILE_CRC_CRC32;
        session->state = EPROTO_UPPER_SESSION_WAIT_RESPONSE;
        session->transfer_state = EPROTO_UPPER_TRANSFER_SENDING;
        memcpy(session->filename, g_test_filename, strlen(g_test_filename));
        session->filename_length = strlen(g_test_filename);
    }

    printf("[Sender] Sending file start...\n");
    eproto_upper_packet_t start_packet;
    uerr = eproto_upper_build_file_start_req(&g_upper_ctx, session_id, g_test_filename,
        g_test_file_size, EPROTO_UPPER_FILE_CRC_CRC32, 1, 0, &start_packet);
    if (uerr != EPROTO_UPPER_OK) {
        printf("[Sender] Failed to build start packet\n");
        return 1;
    }

    uint8_t tx_buf[512];
    tx_buf[0] = start_packet.flags;
    tx_buf[1] = start_packet.func_code;
    tx_buf[2] = (uint8_t)(session_id & 0xFF);
    tx_buf[3] = (uint8_t)((session_id >> 8) & 0xFF);
    tx_buf[4] = (uint8_t)(start_packet.seq_num & 0xFF);
    tx_buf[5] = (uint8_t)((start_packet.seq_num >> 8) & 0xFF);
    tx_buf[6] = (uint8_t)(start_packet.data_length & 0xFF);
    tx_buf[7] = (uint8_t)((start_packet.data_length >> 8) & 0xFF);
    memcpy(tx_buf + 8, start_packet.data, start_packet.data_length);

    eproto_send(&g_eproto, 0x02, tx_buf, 8 + start_packet.data_length, send_callback, NULL, 1);

    usleep(200000);

    printf("[Sender] Sending file data...\n");
    uint32_t crc_value = eproto_upper_calculate_crc32(g_test_file_data, g_test_file_size);
    printf("[Sender] File CRC32: 0x%08X\n", crc_value);

    eproto_upper_packet_t data_packets[16];
    uint8_t packet_count = 0;
    uerr = eproto_upper_build_file_data(&g_upper_ctx, session_id, 0, g_test_file_data,
        g_test_file_size, data_packets, &packet_count);
    if (uerr != EPROTO_UPPER_OK) {
        printf("[Sender] Failed to build data packets\n");
        return 1;
    }

    for (uint8_t i = 0; i < packet_count; i++) {
        uint16_t total_len = 8 + data_packets[i].data_length;
        tx_buf[0] = data_packets[i].flags;
        tx_buf[1] = data_packets[i].func_code;
        tx_buf[2] = (uint8_t)(session_id & 0xFF);
        tx_buf[3] = (uint8_t)((session_id >> 8) & 0xFF);
        tx_buf[4] = (uint8_t)(data_packets[i].seq_num & 0xFF);
        tx_buf[5] = (uint8_t)((data_packets[i].seq_num >> 8) & 0xFF);
        tx_buf[6] = (uint8_t)(data_packets[i].data_length & 0xFF);
        tx_buf[7] = (uint8_t)((data_packets[i].data_length >> 8) & 0xFF);
        memcpy(tx_buf + 8, data_packets[i].data, data_packets[i].data_length);

        printf("[Sender] Sending data packet %u/%u\n", i + 1, packet_count);
        eproto_send(&g_eproto, 0x02, tx_buf, total_len, send_callback, NULL, 1);
        usleep(100000);
    }

    usleep(500000);

    printf("[Sender] Sending file end...\n");
    eproto_upper_packet_t end_packet;
    uerr = eproto_upper_build_file_end(&g_upper_ctx, session_id, crc_value, EPROTO_UPPER_STATUS_SUCCESS, &end_packet);
    if (uerr != EPROTO_UPPER_OK) {
        printf("[Sender] Failed to build end packet\n");
        return 1;
    }

    uint16_t end_len = 8 + end_packet.data_length;
    tx_buf[0] = end_packet.flags;
    tx_buf[1] = end_packet.func_code;
    tx_buf[2] = (uint8_t)(session_id & 0xFF);
    tx_buf[3] = (uint8_t)((session_id >> 8) & 0xFF);
    tx_buf[4] = (uint8_t)(end_packet.seq_num & 0xFF);
    tx_buf[5] = (uint8_t)((end_packet.seq_num >> 8) & 0xFF);
    tx_buf[6] = (uint8_t)(end_packet.data_length & 0xFF);
    tx_buf[7] = (uint8_t)((end_packet.data_length >> 8) & 0xFF);
    memcpy(tx_buf + 8, end_packet.data, end_packet.data_length);

    eproto_send(&g_eproto, 0x02, tx_buf, end_len, send_callback, NULL, 1);

    for (int i = 0; i < 200; i++) {
        uint8_t rx_buf[512];
        uint16_t rx_count = serial_bus_receive(rx_buf, sizeof(rx_buf));
        if (rx_count > 0) {
            eproto_receive_data(&g_eproto, 0x01, rx_buf, rx_count);
        }
        eproto_process(&g_eproto);

        pthread_mutex_lock(&g_file_mutex);
        int complete = g_transfer_complete;
        pthread_mutex_unlock(&g_file_mutex);

        if (complete) {
            printf("[Sender] Transfer complete\n");
            break;
        }

        usleep(10000);
    }

    printf("\n=============================================\n");
    printf("  File transfer completed!\n");
    printf("=============================================\n");

    close_serial_port();
    eproto_upper_deinit(&g_upper_ctx);
    eproto_destroy(&g_eproto);

    return 0;
}
