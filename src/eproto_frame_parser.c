#include "eproto_frame_parser.h"
#include "eproto_crc16.h"
#include "eproto.h"
#include <stddef.h>
#include <string.h>

void eproto_frame_parser_init(eproto_frame_parser_t* parser, eproto_frame_parser_config_t* config, eproto_malloc_func_t malloc_func,
                               eproto_free_func_t free_func) {
    if (!parser || !config || !malloc_func || !free_func)
        return;

    parser->config = *config;
    parser->malloc_func = malloc_func;
    parser->free_func = free_func;
}

static uint8_t is_eproto_frame_header(eproto_ring_buffer_t* rb, uint8_t header) {
    if (!rb || !rb->buffer)
        return 0;

    uint16_t available = eproto_ring_buffer_available(rb);
    if (available == 0)
        return 0;

    uint16_t tail = rb->tail;

    if (rb->buffer[tail] == header) {
        return 1;
    }

    return 0;
}

static uint16_t peek_from_eproto_ring_buffer(eproto_ring_buffer_t* rb, uint8_t* buffer, uint16_t length, uint16_t offset) {
    if (!rb || !rb->buffer || !buffer)
        return 0;

    uint16_t available = eproto_ring_buffer_available(rb);
    if (offset >= available)
        return 0;
    if (length > available - offset)
        length = available - offset;

    uint16_t tail = (rb->tail + offset) % rb->size;
    uint16_t size = rb->size;

    for (uint16_t i = 0; i < length; i++) {
        buffer[i] = rb->buffer[tail];
        tail = (tail + 1) % size;
    }

    return length;
}

eproto_frame_parser_error_t eproto_frame_parser_parse(eproto_ring_buffer_t* rb, eproto_frame_parser_t* parser, eproto_frame_t* result) {
    if (!rb || !parser || !result)
        return EPROTO_FRAME_PARSER_ERROR_INSUFFICIENT_DATA;

    result->header = 0;
    result->version = 0;
    result->length = 0;
    result->packet_type = 0;
    result->source_address = 0;
    result->destination_address = 0;
    result->packet_id = 0;
    result->data = NULL;

    if (eproto_ring_buffer_available(rb) == 0) {
        return EPROTO_FRAME_PARSER_ERROR_NO_HEADER;
    }

    uint8_t found_header = 0;

    while (eproto_ring_buffer_available(rb) >= 10) {
        if (is_eproto_frame_header(rb, parser->config.frame_header)) {
            found_header = 1;
            uint8_t version, length_high, length_low, packet_type, source_address, destination_address, packet_id_high,
                packet_id_low;
            if (peek_from_eproto_ring_buffer(rb, &version, 1, 1) != 1) {
                eproto_ring_buffer_discard(rb, 1);
                continue;
            }
            if (peek_from_eproto_ring_buffer(rb, &length_high, 1, 2) != 1) {
                eproto_ring_buffer_discard(rb, 1);
                continue;
            }
            if (peek_from_eproto_ring_buffer(rb, &length_low, 1, 3) != 1) {
                eproto_ring_buffer_discard(rb, 1);
                continue;
            }
            if (peek_from_eproto_ring_buffer(rb, &packet_type, 1, 4) != 1) {
                eproto_ring_buffer_discard(rb, 1);
                continue;
            }
            if (peek_from_eproto_ring_buffer(rb, &source_address, 1, 5) != 1) {
                eproto_ring_buffer_discard(rb, 1);
                continue;
            }
            if (peek_from_eproto_ring_buffer(rb, &destination_address, 1, 6) != 1) {
                eproto_ring_buffer_discard(rb, 1);
                continue;
            }
            if (peek_from_eproto_ring_buffer(rb, &packet_id_high, 1, 7) != 1) {
                eproto_ring_buffer_discard(rb, 1);
                continue;
            }
            if (peek_from_eproto_ring_buffer(rb, &packet_id_low, 1, 8) != 1) {
                eproto_ring_buffer_discard(rb, 1);
                continue;
            }

            uint16_t data_length = (length_high << 8) | length_low;
            uint16_t total_frame_length = EPROTO_FRAME_HEADER_LENGTH + data_length;

            if (total_frame_length > parser->config.max_frame_length) {
                return EPROTO_FRAME_PARSER_ERROR_INVALID_LENGTH;
            }

            if (eproto_ring_buffer_available(rb) < total_frame_length) {
                return EPROTO_FRAME_PARSER_ERROR_INSUFFICIENT_DATA;
            }

            uint16_t crc_calculated = EPROTO_CRC16_CCITT_INIT;

            uint16_t crc_length = total_frame_length - 2;
            uint16_t tail = rb->tail;
            uint16_t size = rb->size;

            if (tail + crc_length <= size) {
                crc_calculated = eproto_crc16_ccitt_ex(&rb->buffer[tail], crc_length, crc_calculated);
            } else {
                uint16_t first_part = size - tail;
                uint16_t second_part = crc_length - first_part;

                crc_calculated = eproto_crc16_ccitt_ex(&rb->buffer[tail], first_part, crc_calculated);
                crc_calculated = eproto_crc16_ccitt_ex(&rb->buffer[0], second_part, crc_calculated);
            }

            uint8_t crc_high, crc_low;
            if (peek_from_eproto_ring_buffer(rb, &crc_high, 1, total_frame_length - 2) != 1) {
                eproto_ring_buffer_discard(rb, 1);
                continue;
            }
            if (peek_from_eproto_ring_buffer(rb, &crc_low, 1, total_frame_length - 1) != 1) {
                eproto_ring_buffer_discard(rb, 1);
                continue;
            }
            uint16_t crc_received = (crc_high << 8) | crc_low;

            if (crc_calculated != crc_received) {
                eproto_ring_buffer_discard(rb, 1);
                continue;
            }

            eproto_ring_buffer_read(rb, &result->header, 1);
            eproto_ring_buffer_read(rb, &result->version, 1);

            uint8_t length_bytes[2];
            eproto_ring_buffer_read(rb, length_bytes, 2);
            result->length = (length_bytes[0] << 8) | length_bytes[1];

            uint8_t pkt_type;
            eproto_ring_buffer_read(rb, &pkt_type, 1);
            result->packet_type = (eproto_packet_type_t)pkt_type;
            eproto_ring_buffer_read(rb, &result->source_address, 1);
            eproto_ring_buffer_read(rb, &result->destination_address, 1);

            uint8_t packet_id_bytes[2];
            eproto_ring_buffer_read(rb, packet_id_bytes, 2);
            result->packet_id = (packet_id_bytes[0] << 8) | packet_id_bytes[1];

            if (data_length > 0) {
                result->data = (uint8_t*)parser->malloc_func(data_length);
                if (!result->data) {
                    return EPROTO_FRAME_PARSER_ERROR_MEMORY_ALLOC;
                }
                eproto_ring_buffer_read(rb, result->data, data_length);
            } else {
                result->data = NULL;
            }

            eproto_ring_buffer_discard(rb, 2);

            return EPROTO_FRAME_PARSER_OK;
        }

        eproto_ring_buffer_discard(rb, 1);
    }

    if (!found_header) {
        return EPROTO_FRAME_PARSER_ERROR_NO_HEADER;
    } else if (eproto_ring_buffer_available(rb) > 0) {
        return EPROTO_FRAME_PARSER_ERROR_INSUFFICIENT_DATA;
    } else {
        return EPROTO_FRAME_PARSER_ERROR_NO_HEADER;
    }
}

void eproto_frame_parser_free_result(eproto_frame_parser_t* parser, eproto_frame_t* result) {
    if (!parser || !result)
        return;

    if (result->data) {
        parser->free_func(result->data);
        result->data = NULL;
    }

    result->header = 0;
    result->version = 0;
    result->length = 0;
    result->packet_type = 0;
    result->source_address = 0;
    result->destination_address = 0;
    result->packet_id = 0;
}

uint16_t eproto_frame_parser_pack_frame(uint8_t* buffer, uint16_t buffer_size, uint8_t frame_header, uint8_t source_address,
                                         uint8_t destination_address, uint16_t packet_id, uint8_t packet_type, uint8_t* data,
                                         uint16_t data_length) {
    if (!buffer)
        return 0;

    uint16_t total_length = EPROTO_FRAME_HEADER_LENGTH + data_length;

    if (buffer_size < total_length) {
        return 0;
    }

    uint16_t pos = 0;

    buffer[pos++] = frame_header;
    buffer[pos++] = EPROTO_PROTOCOL_VERSION;
    buffer[pos++] = (data_length >> 8) & 0xFF;
    buffer[pos++] = data_length & 0xFF;
    buffer[pos++] = packet_type;
    buffer[pos++] = source_address;
    buffer[pos++] = destination_address;
    buffer[pos++] = (packet_id >> 8) & 0xFF;
    buffer[pos++] = packet_id & 0xFF;
    if (data && data_length > 0) {
        for (uint16_t i = 0; i < data_length; i++) {
            buffer[pos++] = data[i];
        }
    }

    uint16_t crc = eproto_crc16_ccitt(buffer, pos);
    buffer[pos++] = (crc >> 8) & 0xFF;
    buffer[pos++] = crc & 0xFF;

    return pos;
}
