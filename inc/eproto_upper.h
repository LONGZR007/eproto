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

#ifndef EPROTO_UPPER_H
#define EPROTO_UPPER_H

#include <stdint.h>
#include <stddef.h>

#define EPROTO_UPPER_PROTOCOL_VERSION 0x01

#define EPROTO_UPPER_FLAG_ENCRYPT      0x01
#define EPROTO_UPPER_FLAG_NEED_REPLY    0x02
#define EPROTO_UPPER_FLAG_REQUEST       0x00
#define EPROTO_UPPER_FLAG_RESPONSE      0x04
#define EPROTO_UPPER_FLAG_END           0x08
#define EPROTO_UPPER_FLAG_ACK           0x10

#define EPROTO_UPPER_FUNC_FILE_START_REQ    0x01
#define EPROTO_UPPER_FUNC_FILE_START_RSP    0x02
#define EPROTO_UPPER_FUNC_FILE_DATA         0x03
#define EPROTO_UPPER_FUNC_FILE_END          0x04
#define EPROTO_UPPER_FUNC_FILE_CANCEL       0x05
#define EPROTO_UPPER_FUNC_HEARTBEAT         0x10
#define EPROTO_UPPER_FUNC_ACK                0x80

#define EPROTO_UPPER_FILE_CRC_NONE          0x00
#define EPROTO_UPPER_FILE_CRC_CRC16         0x01
#define EPROTO_UPPER_FILE_CRC_CRC32         0x02
#define EPROTO_UPPER_FILE_CRC_MD5           0x03
#define EPROTO_UPPER_FILE_CRC_SHA256        0x04

#define EPROTO_UPPER_STATUS_SUCCESS         0x00
#define EPROTO_UPPER_STATUS_ERROR           0x01
#define EPROTO_UPPER_STATUS_BUSY            0x02
#define EPROTO_UPPER_STATUS_SESSION_EXIST   0x03
#define EPROTO_UPPER_STATUS_SESSION_NOT_EXIST 0x04
#define EPROTO_UPPER_STATUS_INVALID_PARAM   0x05
#define EPROTO_UPPER_STATUS_FILE_TOO_LARGE  0x06
#define EPROTO_UPPER_STATUS_CRC_ERROR       0x07
#define EPROTO_UPPER_STATUS_TRANSFER_ABORT  0x08

#define EPROTO_UPPER_MAX_PAYLOAD_SIZE       230
#define EPROTO_UPPER_MAX_FILENAME_LEN       64
#define EPROTO_UPPER_MAX_SESSIONS           4
#define EPROTO_UPPER_MAX_FILENAME_LEN       64
#define EPROTO_UPPER_FILE_CHUNK_MAX_SIZE    (EPROTO_UPPER_MAX_PAYLOAD_SIZE - 8)

#define EPROTO_UPPER_HEADER_SIZE            8
#define EPROTO_UPPER_FILE_START_REQ_MIN_SIZE 6
#define EPROTO_UPPER_FILE_START_RSP_SIZE     5
#define EPROTO_UPPER_FILE_DATA_MIN_SIZE     7
#define EPROTO_UPPER_FILE_END_SIZE          7

typedef enum {
    EPROTO_UPPER_OK = 0,
    EPROTO_UPPER_ERROR_INVALID_PARAM,
    EPROTO_UPPER_ERROR_BUFFER_FULL,
    EPROTO_UPPER_ERROR_SESSION_NOT_FOUND,
    EPROTO_UPPER_ERROR_SESSION_EXISTS,
    EPROTO_UPPER_ERROR_DATA_TOO_LARGE,
    EPROTO_UPPER_ERROR_INVALID_STATE,
    EPROTO_UPPER_ERROR_CRC_MISMATCH,
    EPROTO_UPPER_ERROR_TIMEOUT,
    EPROTO_UPPER_ERROR_ALLOC_FAILED
} eproto_upper_error_t;

typedef enum {
    EPROTO_UPPER_SESSION_FREE = 0,
    EPROTO_UPPER_SESSION_WAIT_RESPONSE,
    EPROTO_UPPER_SESSION_TRANSFERRING,
    EPROTO_UPPER_SESSION_COMPLETED,
    EPROTO_UPPER_SESSION_CANCELLED
} eproto_upper_session_state_t;

typedef enum {
    EPROTO_UPPER_TRANSFER_IDLE = 0,
    EPROTO_UPPER_TRANSFER_SENDING,
    EPROTO_UPPER_TRANSFER_RECEIVING,
    EPROTO_UPPER_TRANSFER_COMPLETED,
    EPROTO_UPPER_TRANSFER_FAILED
} eproto_upper_transfer_state_t;

typedef struct {
    uint8_t flags;
    uint8_t func_code;
    uint16_t session_id;
    uint16_t seq_num;
    uint16_t data_length;
} eproto_upper_header_t;

typedef struct {
    uint8_t filename_length;
    uint8_t filename[EPROTO_UPPER_MAX_FILENAME_LEN];
    uint32_t file_size;
    uint8_t crc_type;
} eproto_upper_file_start_req_t;

typedef struct {
    uint8_t status;
    uint16_t session_id;
    uint16_t max_chunk_size;
} eproto_upper_file_start_rsp_t;

typedef struct {
    uint16_t session_id;
    uint32_t offset;
    uint16_t data_length;
    uint8_t data[EPROTO_UPPER_FILE_CHUNK_MAX_SIZE];
} eproto_upper_file_data_t;

typedef struct {
    uint16_t session_id;
    uint32_t crc_value;
    uint8_t status;
} eproto_upper_file_end_t;

typedef struct {
    uint16_t session_id;
    uint16_t seq_num;
    uint8_t status;
} eproto_upper_ack_t;

typedef struct {
    uint8_t flags;
    uint8_t func_code;
    uint16_t session_id;
    uint16_t seq_num;
    uint16_t data_length;
    uint8_t data[EPROTO_UPPER_MAX_PAYLOAD_SIZE];
} eproto_upper_packet_t;

typedef struct {
    uint16_t session_id;
    uint16_t max_chunk_size;
    uint32_t file_size;
    uint32_t transferred_bytes;
    uint32_t crc_value;
    uint8_t crc_type;
    uint8_t filename[EPROTO_UPPER_MAX_FILENAME_LEN];
    uint8_t filename_length;
    eproto_upper_session_state_t state;
    eproto_upper_transfer_state_t transfer_state;
    uint32_t last_activity_time;
    uint16_t expected_seq;
    void* user_context;
} eproto_upper_session_t;

typedef struct {
    eproto_upper_session_t sessions[EPROTO_UPPER_MAX_SESSIONS];
    uint16_t next_session_id;
    uint8_t (*malloc)(size_t size, void** out_ptr);
    uint8_t (*free)(void* ptr);
    void (*lock)(void);
    void (*unlock)(void);
    uint32_t (*get_timestamp)(void);
    uint32_t session_timeout_ms;
} eproto_upper_context_t;

typedef void (*eproto_upper_file_callback_t)(uint16_t session_id, uint8_t status, void* user_data);
typedef void (*eproto_upper_data_callback_t)(uint16_t session_id, uint32_t offset, uint8_t* data, uint16_t length, void* user_data);
typedef void (*eproto_upper_progress_callback_t)(uint16_t session_id, uint32_t transferred, uint32_t total, void* user_data);
typedef void (*eproto_upper_recv_callback_t)(uint8_t src_addr, eproto_upper_packet_t* packet, void* user_data);
typedef uint32_t (*eproto_upper_crc_func_t)(uint8_t* data, size_t length);

typedef struct {
    eproto_upper_file_callback_t on_file_start_req;
    eproto_upper_file_callback_t on_file_start_rsp;
    eproto_upper_file_callback_t on_file_end;
    eproto_upper_data_callback_t on_file_data;
    eproto_upper_progress_callback_t on_progress;
    eproto_upper_recv_callback_t on_packet_received;
    void* user_data;
} eproto_upper_callbacks_t;

void eproto_upper_init(eproto_upper_context_t* ctx);
void eproto_upper_deinit(eproto_upper_context_t* ctx);

eproto_upper_error_t eproto_upper_set_functions(eproto_upper_context_t* ctx,
    uint8_t (*malloc_func)(size_t size, void** out_ptr),
    uint8_t (*free_func)(void* ptr),
    void (*lock_func)(void),
    void (*unlock_func)(void),
    uint32_t (*get_timestamp_func)(void));

void eproto_upper_set_callbacks(eproto_upper_context_t* ctx, eproto_upper_callbacks_t* callbacks);

eproto_upper_error_t eproto_upper_create_session(eproto_upper_context_t* ctx, uint16_t* session_id);
eproto_upper_error_t eproto_upper_close_session(eproto_upper_context_t* ctx, uint16_t session_id);
eproto_upper_session_t* eproto_upper_get_session(eproto_upper_context_t* ctx, uint16_t session_id);

eproto_upper_error_t eproto_upper_build_file_start_req(eproto_upper_context_t* ctx,
    uint16_t session_id, const char* filename, uint32_t file_size, uint8_t crc_type,
    uint8_t need_reply, uint8_t encrypted, eproto_upper_packet_t* out_packet);

eproto_upper_error_t eproto_upper_build_file_start_rsp(eproto_upper_context_t* ctx,
    uint16_t session_id, uint8_t status, uint16_t max_chunk_size,
    eproto_upper_packet_t* out_packet);

eproto_upper_error_t eproto_upper_build_file_data(eproto_upper_context_t* ctx,
    uint16_t session_id, uint32_t offset, uint8_t* data, uint16_t length,
    eproto_upper_packet_t* out_packets, uint8_t* out_packet_count);

eproto_upper_error_t eproto_upper_build_file_end(eproto_upper_context_t* ctx,
    uint16_t session_id, uint32_t crc_value, uint8_t status,
    eproto_upper_packet_t* out_packet);

eproto_upper_error_t eproto_upper_build_file_cancel(eproto_upper_context_t* ctx,
    uint16_t session_id, eproto_upper_packet_t* out_packet);

eproto_upper_error_t eproto_upper_build_ack(eproto_upper_context_t* ctx,
    uint16_t session_id, uint16_t seq_num, uint8_t status,
    eproto_upper_packet_t* out_packet);

eproto_upper_error_t eproto_upper_build_heartbeat(eproto_upper_context_t* ctx,
    uint16_t session_id, uint8_t need_reply, eproto_upper_packet_t* out_packet);

eproto_upper_error_t eproto_upper_parse_packet(eproto_upper_context_t* ctx,
    uint8_t* data, uint16_t length, eproto_upper_packet_t* out_packet);

eproto_upper_error_t eproto_upper_handle_packet(eproto_upper_context_t* ctx,
    uint8_t src_addr, eproto_upper_packet_t* packet);

eproto_upper_error_t eproto_upper_send_file_start(eproto_upper_context_t* ctx,
    uint16_t session_id, const char* filename, uint32_t file_size, uint8_t crc_type);

eproto_upper_error_t eproto_upper_send_file_data(eproto_upper_context_t* ctx,
    uint16_t session_id, uint8_t* file_data, uint32_t file_size);

eproto_upper_error_t eproto_upper_send_file_end(eproto_upper_context_t* ctx,
    uint16_t session_id);

uint16_t eproto_upper_calculate_crc16(uint8_t* data, size_t length);
uint32_t eproto_upper_calculate_crc32(uint8_t* data, size_t length);

eproto_upper_error_t eproto_upper_get_payload_from_packet(eproto_upper_packet_t* packet,
    uint8_t** out_payload, uint16_t* out_length);

#endif
