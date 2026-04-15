#ifndef EPROTO_FRAME_PARSER_H
#define EPROTO_FRAME_PARSER_H

#include <stdint.h>
#include <stddef.h>
#include "eproto_ring_buffer.h"

#define EPROTO_PACKET_TYPE_RETRANSMIT_FLAG 0x80
#define EPROTO_PACKET_TYPE_HANDSHAKE_FLAG 0x40

typedef enum {
    EPROTO_PACKET_TYPE_USER_SEND = 0,
    EPROTO_PACKET_TYPE_USER_REPLY,
    EPROTO_PACKET_TYPE_PROTOCOL_ACK
} eproto_packet_type_t;

typedef struct eproto_frame {
    uint8_t header;
    uint8_t version;
    uint16_t length;
    eproto_packet_type_t packet_type;
    uint8_t source_address;
    uint8_t destination_address;
    uint16_t packet_id;
    uint8_t* data;
} eproto_frame_t;

typedef enum {
    EPROTO_FRAME_PARSER_OK = 0,
    EPROTO_FRAME_PARSER_ERROR_NO_HEADER,
    EPROTO_FRAME_PARSER_ERROR_INVALID_LENGTH,
    EPROTO_FRAME_PARSER_ERROR_INSUFFICIENT_DATA,
    EPROTO_FRAME_PARSER_ERROR_MEMORY_ALLOC,
    EPROTO_FRAME_PARSER_ERROR_CRC_CHECK
} eproto_frame_parser_error_t;

typedef struct {
    uint8_t frame_header;
    uint16_t max_frame_length;
} eproto_frame_parser_config_t;

typedef struct {
    uint8_t* frame_data;
    uint16_t frame_length;
} eproto_frame_parser_result_t;

typedef void* (*eproto_malloc_func_t)(size_t size);
typedef void (*eproto_free_func_t)(void* ptr);

typedef struct {
    eproto_frame_parser_config_t config;
    eproto_malloc_func_t malloc_func;
    eproto_free_func_t free_func;
} eproto_frame_parser_t;

void eproto_frame_parser_init(eproto_frame_parser_t* parser, eproto_frame_parser_config_t* config,
                              eproto_malloc_func_t malloc_func, eproto_free_func_t free_func);

eproto_frame_parser_error_t eproto_frame_parser_parse(eproto_ring_buffer_t* rb, eproto_frame_parser_t* parser,
                                                      eproto_frame_t* result);

void eproto_frame_parser_free_result(eproto_frame_parser_t* parser, eproto_frame_t* result);

uint16_t eproto_frame_parser_pack_frame(uint8_t* buffer, uint16_t buffer_size, uint8_t frame_header,
                                        uint8_t source_address, uint8_t destination_address, uint16_t packet_id,
                                        uint8_t packet_type, uint8_t* data, uint16_t data_length);

#endif
