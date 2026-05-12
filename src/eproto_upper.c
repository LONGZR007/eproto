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

#include "eproto_upper.h"
#include <string.h>
#include <stdio.h>

#define EPROTO_UPPER_DEBUG_LOG(fmt, ...) \
    do { } while(0)

static eproto_upper_callbacks_t g_callbacks = {0};

static void eproto_upper_default_lock(void) {}
static void eproto_upper_default_unlock(void) {}
static uint32_t eproto_upper_default_get_timestamp(void) {
    return 0;
}
static uint8_t eproto_upper_default_malloc(size_t size, void** out_ptr) {
    (void)size;
    *out_ptr = NULL;
    return 0;
}
static uint8_t eproto_upper_default_free(void* ptr) {
    (void)ptr;
    return 1;
}

static uint32_t calculate_crc32_table(uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    static uint32_t crc_table[256];
    static int table_init = 0;

    if (!table_init) {
        for (int i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int j = 0; j < 8; j++) {
                if (c & 1) {
                    c = 0xEDB88320 ^ (c >> 1);
                } else {
                    c = c >> 1;
                }
            }
            crc_table[i] = c;
        }
        table_init = 1;
    }

    for (size_t i = 0; i < length; i++) {
        crc = crc_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }

    return crc ^ 0xFFFFFFFF;
}

void eproto_upper_init(eproto_upper_context_t* ctx) {
    if (!ctx) {
        return;
    }
    memset(ctx, 0, sizeof(eproto_upper_context_t));
    ctx->next_session_id = 1;
    ctx->session_timeout_ms = 30000;
    ctx->malloc = eproto_upper_default_malloc;
    ctx->free = eproto_upper_default_free;
    ctx->lock = eproto_upper_default_lock;
    ctx->unlock = eproto_upper_default_unlock;
    ctx->get_timestamp = eproto_upper_default_get_timestamp;

    for (int i = 0; i < EPROTO_UPPER_MAX_SESSIONS; i++) {
        ctx->sessions[i].state = EPROTO_UPPER_SESSION_FREE;
        ctx->sessions[i].transfer_state = EPROTO_UPPER_TRANSFER_IDLE;
    }
}

void eproto_upper_deinit(eproto_upper_context_t* ctx) {
    if (!ctx) {
        return;
    }
    for (int i = 0; i < EPROTO_UPPER_MAX_SESSIONS; i++) {
        if (ctx->sessions[i].state != EPROTO_UPPER_SESSION_FREE) {
            ctx->sessions[i].state = EPROTO_UPPER_SESSION_FREE;
            ctx->sessions[i].transfer_state = EPROTO_UPPER_TRANSFER_IDLE;
        }
    }
}

eproto_upper_error_t eproto_upper_set_functions(eproto_upper_context_t* ctx,
    uint8_t (*malloc_func)(size_t size, void** out_ptr),
    uint8_t (*free_func)(void* ptr),
    void (*lock_func)(void),
    void (*unlock_func)(void),
    uint32_t (*get_timestamp_func)(void)) {

    if (!ctx) {
        return EPROTO_UPPER_ERROR_INVALID_PARAM;
    }

    if (malloc_func) ctx->malloc = malloc_func;
    if (free_func) ctx->free = free_func;
    if (lock_func) ctx->lock = lock_func;
    if (unlock_func) ctx->unlock = unlock_func;
    if (get_timestamp_func) ctx->get_timestamp = get_timestamp_func;

    return EPROTO_UPPER_OK;
}

void eproto_upper_set_callbacks(eproto_upper_context_t* ctx, eproto_upper_callbacks_t* callbacks) {
    if (callbacks) {
        memcpy(&g_callbacks, callbacks, sizeof(eproto_upper_callbacks_t));
    }
}

eproto_upper_error_t eproto_upper_create_session(eproto_upper_context_t* ctx, uint16_t* session_id) {
    if (!ctx || !session_id) {
        return EPROTO_UPPER_ERROR_INVALID_PARAM;
    }

    ctx->lock();

    int free_index = -1;
    for (int i = 0; i < EPROTO_UPPER_MAX_SESSIONS; i++) {
        if (ctx->sessions[i].state == EPROTO_UPPER_SESSION_FREE) {
            free_index = i;
            break;
        }
    }

    if (free_index < 0) {
        ctx->unlock();
        return EPROTO_UPPER_ERROR_BUFFER_FULL;
    }

    eproto_upper_session_t* session = &ctx->sessions[free_index];
    session->session_id = ctx->next_session_id++;
    if (ctx->next_session_id == 0) {
        ctx->next_session_id = 1;
    }
    session->state = EPROTO_UPPER_SESSION_WAIT_RESPONSE;
    session->transfer_state = EPROTO_UPPER_TRANSFER_IDLE;
    session->max_chunk_size = EPROTO_UPPER_FILE_CHUNK_MAX_SIZE;
    session->last_activity_time = ctx->get_timestamp();

    *session_id = session->session_id;

    ctx->unlock();

    EPROTO_UPPER_DEBUG_LOG("Created session %u at index %d\n", session->session_id, free_index);

    return EPROTO_UPPER_OK;
}

eproto_upper_error_t eproto_upper_close_session(eproto_upper_context_t* ctx, uint16_t session_id) {
    if (!ctx) {
        return EPROTO_UPPER_ERROR_INVALID_PARAM;
    }

    ctx->lock();

    for (int i = 0; i < EPROTO_UPPER_MAX_SESSIONS; i++) {
        if (ctx->sessions[i].session_id == session_id) {
            ctx->sessions[i].state = EPROTO_UPPER_SESSION_FREE;
            ctx->sessions[i].transfer_state = EPROTO_UPPER_TRANSFER_IDLE;
            ctx->unlock();
            EPROTO_UPPER_DEBUG_LOG("Closed session %u\n", session_id);
            return EPROTO_UPPER_OK;
        }
    }

    ctx->unlock();
    return EPROTO_UPPER_ERROR_SESSION_NOT_FOUND;
}

eproto_upper_session_t* eproto_upper_get_session(eproto_upper_context_t* ctx, uint16_t session_id) {
    if (!ctx) {
        return NULL;
    }

    for (int i = 0; i < EPROTO_UPPER_MAX_SESSIONS; i++) {
        if (ctx->sessions[i].session_id == session_id) {
            return &ctx->sessions[i];
        }
    }
    return NULL;
}

eproto_upper_error_t eproto_upper_build_file_start_req(eproto_upper_context_t* ctx,
    uint16_t session_id, const char* filename, uint32_t file_size, uint8_t crc_type,
    uint8_t need_reply, uint8_t encrypted, eproto_upper_packet_t* out_packet) {

    if (!ctx || !filename || !out_packet) {
        return EPROTO_UPPER_ERROR_INVALID_PARAM;
    }

    size_t filename_len = strlen(filename);
    if (filename_len == 0 || filename_len > EPROTO_UPPER_MAX_FILENAME_LEN) {
        return EPROTO_UPPER_ERROR_INVALID_PARAM;
    }

    uint16_t total_length = EPROTO_UPPER_HEADER_SIZE + 1 + filename_len + 4 + 1;
    if (total_length > EPROTO_UPPER_MAX_PAYLOAD_SIZE + EPROTO_UPPER_HEADER_SIZE) {
        return EPROTO_UPPER_ERROR_DATA_TOO_LARGE;
    }

    out_packet->flags = encrypted ? EPROTO_UPPER_FLAG_ENCRYPT : 0;
    if (need_reply) {
        out_packet->flags |= EPROTO_UPPER_FLAG_NEED_REPLY;
    }
    out_packet->func_code = EPROTO_UPPER_FUNC_FILE_START_REQ;
    out_packet->session_id = session_id;
    out_packet->seq_num = 0;

    uint8_t* payload = out_packet->data;
    payload[0] = (uint8_t)filename_len;
    memcpy(&payload[1], filename, filename_len);
    memcpy(&payload[1 + filename_len], &file_size, 4);
    payload[1 + filename_len + 4] = crc_type;

    out_packet->data_length = 1 + filename_len + 4 + 1;

    return EPROTO_UPPER_OK;
}

eproto_upper_error_t eproto_upper_build_file_start_rsp(eproto_upper_context_t* ctx,
    uint16_t session_id, uint8_t status, uint16_t max_chunk_size,
    eproto_upper_packet_t* out_packet) {

    (void)ctx;

    if (!out_packet) {
        return EPROTO_UPPER_ERROR_INVALID_PARAM;
    }

    out_packet->flags = EPROTO_UPPER_FLAG_RESPONSE;
    out_packet->func_code = EPROTO_UPPER_FUNC_FILE_START_RSP;
    out_packet->session_id = session_id;
    out_packet->seq_num = 0;

    uint8_t* payload = out_packet->data;
    payload[0] = status;
    memcpy(&payload[1], &max_chunk_size, 2);

    out_packet->data_length = EPROTO_UPPER_FILE_START_RSP_SIZE;

    return EPROTO_UPPER_OK;
}

eproto_upper_error_t eproto_upper_build_file_data(eproto_upper_context_t* ctx,
    uint16_t session_id, uint32_t offset, uint8_t* data, uint16_t length,
    eproto_upper_packet_t* out_packets, uint8_t* out_packet_count) {

    if (!ctx || !data || !out_packets || !out_packet_count) {
        return EPROTO_UPPER_ERROR_INVALID_PARAM;
    }

    if (length == 0) {
        *out_packet_count = 0;
        return EPROTO_UPPER_OK;
    }

    uint16_t max_chunk = EPROTO_UPPER_FILE_CHUNK_MAX_SIZE;
    eproto_upper_session_t* session = eproto_upper_get_session(ctx, session_id);
    if (session) {
        max_chunk = session->max_chunk_size;
        if (max_chunk == 0) {
            max_chunk = EPROTO_UPPER_FILE_CHUNK_MAX_SIZE;
        }
    }

    uint8_t packet_count = (length + max_chunk - 1) / max_chunk;
    if (packet_count > 16) {
        packet_count = 16;
    }

    *out_packet_count = packet_count;
    uint16_t remaining = length;
    uint32_t current_offset = offset;
    uint16_t current_pos = 0;

    for (uint8_t i = 0; i < packet_count && remaining > 0; i++) {
        uint16_t chunk_size = (remaining > max_chunk) ? max_chunk : remaining;

        out_packets[i].flags = 0;
        out_packets[i].func_code = EPROTO_UPPER_FUNC_FILE_DATA;
        out_packets[i].session_id = session_id;
        out_packets[i].seq_num = i + 1;

        uint8_t* payload = out_packets[i].data;
        memcpy(&payload[0], &current_offset, 4);
        memcpy(&payload[4], &chunk_size, 2);
        memcpy(&payload[6], &data[current_pos], chunk_size);

        out_packets[i].data_length = 6 + chunk_size;

        current_pos += chunk_size;
        current_offset += chunk_size;
        remaining -= chunk_size;
    }

    return EPROTO_UPPER_OK;
}

eproto_upper_error_t eproto_upper_build_file_end(eproto_upper_context_t* ctx,
    uint16_t session_id, uint32_t crc_value, uint8_t status,
    eproto_upper_packet_t* out_packet) {

    (void)ctx;

    if (!out_packet) {
        return EPROTO_UPPER_ERROR_INVALID_PARAM;
    }

    out_packet->flags = EPROTO_UPPER_FLAG_END;
    out_packet->func_code = EPROTO_UPPER_FUNC_FILE_END;
    out_packet->session_id = session_id;
    out_packet->seq_num = 0;

    uint8_t* payload = out_packet->data;
    payload[0] = status;
    memcpy(&payload[1], &crc_value, 4);

    out_packet->data_length = EPROTO_UPPER_FILE_END_SIZE;

    return EPROTO_UPPER_OK;
}

eproto_upper_error_t eproto_upper_build_file_cancel(eproto_upper_context_t* ctx,
    uint16_t session_id, eproto_upper_packet_t* out_packet) {

    (void)ctx;

    if (!out_packet) {
        return EPROTO_UPPER_ERROR_INVALID_PARAM;
    }

    out_packet->flags = EPROTO_UPPER_FLAG_END;
    out_packet->func_code = EPROTO_UPPER_FUNC_FILE_CANCEL;
    out_packet->session_id = session_id;
    out_packet->seq_num = 0;
    out_packet->data_length = 0;

    return EPROTO_UPPER_OK;
}

eproto_upper_error_t eproto_upper_build_ack(eproto_upper_context_t* ctx,
    uint16_t session_id, uint16_t seq_num, uint8_t status,
    eproto_upper_packet_t* out_packet) {

    (void)ctx;

    if (!out_packet) {
        return EPROTO_UPPER_ERROR_INVALID_PARAM;
    }

    out_packet->flags = EPROTO_UPPER_FLAG_RESPONSE | EPROTO_UPPER_FLAG_ACK;
    out_packet->func_code = EPROTO_UPPER_FUNC_ACK;
    out_packet->session_id = session_id;
    out_packet->seq_num = seq_num;

    uint8_t* payload = out_packet->data;
    payload[0] = status;

    out_packet->data_length = 1;

    return EPROTO_UPPER_OK;
}

eproto_upper_error_t eproto_upper_build_heartbeat(eproto_upper_context_t* ctx,
    uint16_t session_id, uint8_t need_reply, eproto_upper_packet_t* out_packet) {

    (void)ctx;

    if (!out_packet) {
        return EPROTO_UPPER_ERROR_INVALID_PARAM;
    }

    out_packet->flags = need_reply ? EPROTO_UPPER_FLAG_NEED_REPLY : 0;
    out_packet->func_code = EPROTO_UPPER_FUNC_HEARTBEAT;
    out_packet->session_id = session_id;
    out_packet->seq_num = 0;
    out_packet->data_length = 0;

    return EPROTO_UPPER_OK;
}

eproto_upper_error_t eproto_upper_parse_packet(eproto_upper_context_t* ctx,
    uint8_t* data, uint16_t length, eproto_upper_packet_t* out_packet) {

    (void)ctx;

    if (!data || !out_packet) {
        return EPROTO_UPPER_ERROR_INVALID_PARAM;
    }

    if (length < EPROTO_UPPER_HEADER_SIZE) {
        return EPROTO_UPPER_ERROR_INVALID_PARAM;
    }

    uint16_t offset = 0;
    out_packet->flags = data[offset++];
    out_packet->func_code = data[offset++];
    out_packet->session_id = (uint16_t)data[offset] | ((uint16_t)data[offset + 1] << 8);
    offset += 2;
    out_packet->seq_num = (uint16_t)data[offset] | ((uint16_t)data[offset + 1] << 8);
    offset += 2;
    out_packet->data_length = (uint16_t)data[offset] | ((uint16_t)data[offset + 1] << 8);
    offset += 2;

    if (offset + out_packet->data_length > length) {
        return EPROTO_UPPER_ERROR_INVALID_PARAM;
    }

    if (out_packet->data_length > EPROTO_UPPER_MAX_PAYLOAD_SIZE) {
        return EPROTO_UPPER_ERROR_INVALID_PARAM;
    }

    if (out_packet->data_length > 0) {
        memcpy(out_packet->data, &data[offset], out_packet->data_length);
    }

    return EPROTO_UPPER_OK;
}

eproto_upper_error_t eproto_upper_handle_packet(eproto_upper_context_t* ctx,
    uint8_t src_addr, eproto_upper_packet_t* packet) {

    if (!ctx || !packet) {
        return EPROTO_UPPER_ERROR_INVALID_PARAM;
    }

    eproto_upper_session_t* session = eproto_upper_get_session(ctx, packet->session_id);

    switch (packet->func_code) {
        case EPROTO_UPPER_FUNC_FILE_START_REQ: {
            if (packet->data_length < EPROTO_UPPER_FILE_START_REQ_MIN_SIZE) {
                return EPROTO_UPPER_ERROR_INVALID_PARAM;
            }

            uint8_t filename_len = packet->data[0];
            if (filename_len > EPROTO_UPPER_MAX_FILENAME_LEN ||
                packet->data_length < (1 + filename_len + 4 + 1)) {
                return EPROTO_UPPER_ERROR_INVALID_PARAM;
            }

            eproto_upper_file_start_req_t req;
            req.filename_length = filename_len;
            memcpy(req.filename, &packet->data[1], filename_len);
            req.filename[filename_len] = '\0';
            memcpy(&req.file_size, &packet->data[1 + filename_len], 4);
            req.crc_type = packet->data[1 + filename_len + 4];

            if (g_callbacks.on_file_start_req) {
                g_callbacks.on_file_start_req(packet->session_id, EPROTO_UPPER_STATUS_SUCCESS, g_callbacks.user_data);
            }

            if (g_callbacks.on_packet_received) {
                g_callbacks.on_packet_received(src_addr, packet, g_callbacks.user_data);
            }
            break;
        }

        case EPROTO_UPPER_FUNC_FILE_START_RSP: {
            if (packet->data_length < EPROTO_UPPER_FILE_START_RSP_SIZE) {
                return EPROTO_UPPER_ERROR_INVALID_PARAM;
            }

            uint8_t status = packet->data[0];
            uint16_t max_chunk;
            memcpy(&max_chunk, &packet->data[1], 2);

            if (session) {
                session->state = EPROTO_UPPER_SESSION_TRANSFERRING;
                session->max_chunk_size = max_chunk;
                session->transfer_state = EPROTO_UPPER_TRANSFER_SENDING;
                session->last_activity_time = ctx->get_timestamp();
            }

            if (g_callbacks.on_file_start_rsp) {
                g_callbacks.on_file_start_rsp(packet->session_id, status, g_callbacks.user_data);
            }

            if (g_callbacks.on_packet_received) {
                g_callbacks.on_packet_received(src_addr, packet, g_callbacks.user_data);
            }
            break;
        }

        case EPROTO_UPPER_FUNC_FILE_DATA: {
            if (packet->data_length < EPROTO_UPPER_FILE_DATA_MIN_SIZE) {
                return EPROTO_UPPER_ERROR_INVALID_PARAM;
            }

            uint32_t offset;
            uint16_t data_length;
            memcpy(&offset, &packet->data[0], 4);
            memcpy(&data_length, &packet->data[4], 2);

            if (packet->data_length < (6 + data_length)) {
                return EPROTO_UPPER_ERROR_INVALID_PARAM;
            }

            if (session) {
                session->transfer_state = EPROTO_UPPER_TRANSFER_RECEIVING;
                session->last_activity_time = ctx->get_timestamp();
                session->transferred_bytes = offset + data_length;
            }

            if (g_callbacks.on_file_data) {
                g_callbacks.on_file_data(packet->session_id, offset, &packet->data[6], data_length, g_callbacks.user_data);
            }

            if (g_callbacks.on_progress) {
                if (session) {
                    g_callbacks.on_progress(packet->session_id, session->transferred_bytes, session->file_size, g_callbacks.user_data);
                }
            }

            if (g_callbacks.on_packet_received) {
                g_callbacks.on_packet_received(src_addr, packet, g_callbacks.user_data);
            }
            break;
        }

        case EPROTO_UPPER_FUNC_FILE_END: {
            if (packet->data_length < EPROTO_UPPER_FILE_END_SIZE) {
                return EPROTO_UPPER_ERROR_INVALID_PARAM;
            }

            uint8_t status = packet->data[0];
            uint32_t crc_value;
            memcpy(&crc_value, &packet->data[1], 4);

            if (session) {
                session->state = EPROTO_UPPER_SESSION_COMPLETED;
                session->transfer_state = EPROTO_UPPER_TRANSFER_COMPLETED;
            }

            if (g_callbacks.on_file_end) {
                g_callbacks.on_file_end(packet->session_id, status, g_callbacks.user_data);
            }

            if (g_callbacks.on_packet_received) {
                g_callbacks.on_packet_received(src_addr, packet, g_callbacks.user_data);
            }
            break;
        }

        case EPROTO_UPPER_FUNC_FILE_CANCEL: {
            if (session) {
                session->state = EPROTO_UPPER_SESSION_CANCELLED;
                session->transfer_state = EPROTO_UPPER_TRANSFER_FAILED;
            }

            if (g_callbacks.on_packet_received) {
                g_callbacks.on_packet_received(src_addr, packet, g_callbacks.user_data);
            }
            break;
        }

        case EPROTO_UPPER_FUNC_ACK: {
            if (packet->data_length < 1) {
                return EPROTO_UPPER_ERROR_INVALID_PARAM;
            }

            if (session) {
                session->last_activity_time = ctx->get_timestamp();
            }

            if (g_callbacks.on_packet_received) {
                g_callbacks.on_packet_received(src_addr, packet, g_callbacks.user_data);
            }
            break;
        }

        case EPROTO_UPPER_FUNC_HEARTBEAT: {
            if (session) {
                session->last_activity_time = ctx->get_timestamp();
            }

            if (g_callbacks.on_packet_received) {
                g_callbacks.on_packet_received(src_addr, packet, g_callbacks.user_data);
            }
            break;
        }

        default:
            return EPROTO_UPPER_ERROR_INVALID_PARAM;
    }

    return EPROTO_UPPER_OK;
}

eproto_upper_error_t eproto_upper_send_file_start(eproto_upper_context_t* ctx,
    uint16_t session_id, const char* filename, uint32_t file_size, uint8_t crc_type) {

    if (!ctx || !filename) {
        return EPROTO_UPPER_ERROR_INVALID_PARAM;
    }

    eproto_upper_session_t* session = eproto_upper_get_session(ctx, session_id);
    if (!session) {
        eproto_upper_error_t err = eproto_upper_create_session(ctx, &session_id);
        if (err != EPROTO_UPPER_OK) {
            return err;
        }
        session = eproto_upper_get_session(ctx, session_id);
    }

    if (session) {
        session->file_size = file_size;
        session->crc_type = crc_type;
        session->state = EPROTO_UPPER_SESSION_WAIT_RESPONSE;
        session->transfer_state = EPROTO_UPPER_TRANSFER_SENDING;

        size_t filename_len = strlen(filename);
        if (filename_len > EPROTO_UPPER_MAX_FILENAME_LEN) {
            filename_len = EPROTO_UPPER_MAX_FILENAME_LEN;
        }
        memcpy(session->filename, filename, filename_len);
        session->filename[filename_len] = '\0';
        session->filename_length = (uint8_t)filename_len;
    }

    return EPROTO_UPPER_OK;
}

eproto_upper_error_t eproto_upper_send_file_data(eproto_upper_context_t* ctx,
    uint16_t session_id, uint8_t* file_data, uint32_t file_size) {

    if (!ctx || !file_data) {
        return EPROTO_UPPER_ERROR_INVALID_PARAM;
    }

    eproto_upper_session_t* session = eproto_upper_get_session(ctx, session_id);
    if (!session) {
        return EPROTO_UPPER_ERROR_SESSION_NOT_FOUND;
    }

    if (session->crc_type == EPROTO_UPPER_FILE_CRC_CRC32) {
        session->crc_value = calculate_crc32_table(file_data, file_size);
    } else if (session->crc_type == EPROTO_UPPER_FILE_CRC_CRC16) {
        session->crc_value = eproto_upper_calculate_crc16(file_data, file_size);
    }

    return EPROTO_UPPER_OK;
}

eproto_upper_error_t eproto_upper_send_file_end(eproto_upper_context_t* ctx,
    uint16_t session_id) {

    if (!ctx) {
        return EPROTO_UPPER_ERROR_INVALID_PARAM;
    }

    eproto_upper_session_t* session = eproto_upper_get_session(ctx, session_id);
    if (!session) {
        return EPROTO_UPPER_ERROR_SESSION_NOT_FOUND;
    }

    session->state = EPROTO_UPPER_SESSION_COMPLETED;
    session->transfer_state = EPROTO_UPPER_TRANSFER_COMPLETED;

    return EPROTO_UPPER_OK;
}

uint16_t eproto_upper_calculate_crc16(uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFF;
    static uint16_t crc_table[256];
    static int table_init = 0;

    if (!table_init) {
        const uint16_t polynomial = 0x8005;
        for (int i = 0; i < 256; i++) {
            uint16_t c = i << 8;
            for (int j = 0; j < 8; j++) {
                if ((c ^ polynomial) & 0x8000) {
                    c = (c << 1) ^ polynomial;
                } else {
                    c = c << 1;
                }
            }
            crc_table[i] = c;
        }
        table_init = 1;
    }

    for (size_t i = 0; i < length; i++) {
        crc = (crc << 8) ^ crc_table[((crc >> 8) ^ data[i]) & 0xFF];
    }

    return crc;
}

uint32_t eproto_upper_calculate_crc32(uint8_t* data, size_t length) {
    return calculate_crc32_table(data, length);
}

eproto_upper_error_t eproto_upper_get_payload_from_packet(eproto_upper_packet_t* packet,
    uint8_t** out_payload, uint16_t* out_length) {

    if (!packet || !out_payload || !out_length) {
        return EPROTO_UPPER_ERROR_INVALID_PARAM;
    }

    *out_payload = packet->data;
    *out_length = packet->data_length;

    return EPROTO_UPPER_OK;
}
