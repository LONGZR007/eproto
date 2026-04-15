#include "frame_parser.h"
#include "crc16.h"
#include "eproto.h"
#include <stddef.h>
#include <string.h>

// 初始化帧解析器
void frame_parser_init(frame_parser_t* parser, frame_parser_config_t* config, malloc_func_t malloc_func,
                       free_func_t free_func) {
    if (!parser || !config || !malloc_func || !free_func)
        return;

    parser->config = *config;
    parser->malloc_func = malloc_func;
    parser->free_func = free_func;
}

// 在环形缓冲区中查找帧头
static uint8_t is_frame_header(ring_buffer_t* rb, uint8_t header) {
    if (!rb || !rb->buffer)
        return 0;

    uint16_t available = ring_buffer_available(rb);
    if (available == 0)
        return 0;

    uint16_t tail = rb->tail;

    if (rb->buffer[tail] == header) {
        return 1;  // 返回找到帧头
    }

    return 0;  // 返回未找到帧头
}

// 从环形缓冲区读取指定长度的数据（不移动读指针）
static uint16_t peek_from_ring_buffer(ring_buffer_t* rb, uint8_t* buffer, uint16_t length, uint16_t offset) {
    if (!rb || !rb->buffer || !buffer)
        return 0;

    uint16_t available = ring_buffer_available(rb);
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

// 解析帧
frame_parser_error_t frame_parser_parse(ring_buffer_t* rb, frame_parser_t* parser, eproto_frame_t* result) {
    if (!rb || !parser || !result)
        return FRAME_PARSER_ERROR_INSUFFICIENT_DATA;

    // 初始化结果
    result->header = 0;
    result->version = 0;
    result->length = 0;
    result->packet_type = 0;
    result->source_address = 0;
    result->destination_address = 0;
    result->packet_id = 0;
    result->data = NULL;

    // 检查是否有数据
    if (ring_buffer_available(rb) == 0) {
        return FRAME_PARSER_ERROR_NO_HEADER;
    }

    uint8_t found_header = 0;

    // 查找帧头
    while (ring_buffer_available(rb) >= 10) {  // 帧头(1) + 版本(1) + 长度(2) + 包类型(1) + 原地址(1) +
                                               // 设备地址(1) + 包ID(2)
        if (is_frame_header(rb, parser->config.frame_header)) {
            found_header = 1;
            uint8_t version, length_high, length_low, packet_type, source_address, destination_address, packet_id_high,
                packet_id_low;
            if (peek_from_ring_buffer(rb, &version, 1, 1) != 1) {
                ring_buffer_discard(rb, 1);
                continue;
            }
            if (peek_from_ring_buffer(rb, &length_high, 1, 2) != 1) {
                ring_buffer_discard(rb, 1);
                continue;
            }
            if (peek_from_ring_buffer(rb, &length_low, 1, 3) != 1) {
                ring_buffer_discard(rb, 1);
                continue;
            }
            if (peek_from_ring_buffer(rb, &packet_type, 1, 4) != 1) {
                ring_buffer_discard(rb, 1);
                continue;
            }
            if (peek_from_ring_buffer(rb, &source_address, 1, 5) != 1) {
                ring_buffer_discard(rb, 1);
                continue;
            }
            if (peek_from_ring_buffer(rb, &destination_address, 1, 6) != 1) {
                ring_buffer_discard(rb, 1);
                continue;
            }
            if (peek_from_ring_buffer(rb, &packet_id_high, 1, 7) != 1) {
                ring_buffer_discard(rb, 1);
                continue;
            }
            if (peek_from_ring_buffer(rb, &packet_id_low, 1, 8) != 1) {
                ring_buffer_discard(rb, 1);
                continue;
            }

            uint16_t data_length = (length_high << 8) | length_low;
            uint16_t total_frame_length = EPROTO_FRAME_HEADER_LENGTH + data_length;

            // 检查帧长度是否有效
            if (total_frame_length > parser->config.max_frame_length) {
                return FRAME_PARSER_ERROR_INVALID_LENGTH;
            }

            // 检查是否有足够的数据
            if (ring_buffer_available(rb) < total_frame_length) {
                return FRAME_PARSER_ERROR_INSUFFICIENT_DATA;
            }

            // 计算CRC
            uint16_t crc_calculated = CRC16_CCITT_INIT;

            // 从环形缓冲区读取数据计算CRC（不包括CRC字段本身）
            uint16_t crc_length = total_frame_length - 2;
            uint16_t tail = rb->tail;
            uint16_t size = rb->size;

            // 分段计算CRC
            if (tail + crc_length <= size) {
                // 数据在缓冲区中是连续的
                crc_calculated = crc16_ccitt_ex(&rb->buffer[tail], crc_length, crc_calculated);
            } else {
                // 数据在缓冲区中是分段的
                uint16_t first_part = size - tail;
                uint16_t second_part = crc_length - first_part;

                // 计算第一部分
                crc_calculated = crc16_ccitt_ex(&rb->buffer[tail], first_part, crc_calculated);
                // 计算第二部分
                crc_calculated = crc16_ccitt_ex(&rb->buffer[0], second_part, crc_calculated);
            }

            // 读取接收到的CRC
            uint8_t crc_high, crc_low;
            if (peek_from_ring_buffer(rb, &crc_high, 1, total_frame_length - 2) != 1) {
                ring_buffer_discard(rb, 1);
                continue;
            }
            if (peek_from_ring_buffer(rb, &crc_low, 1, total_frame_length - 1) != 1) {
                ring_buffer_discard(rb, 1);
                continue;
            }
            uint16_t crc_received = (crc_high << 8) | crc_low;

            if (crc_calculated != crc_received) {
                ring_buffer_discard(rb, 1);
                continue;
            }

            // 读取帧头信息
            ring_buffer_read(rb, &result->header, 1);
            ring_buffer_read(rb, &result->version, 1);

            // 读取长度（处理大小端）
            uint8_t length_bytes[2];
            ring_buffer_read(rb, length_bytes, 2);
            result->length = (length_bytes[0] << 8) | length_bytes[1];

            uint8_t pkt_type;
            ring_buffer_read(rb, &pkt_type, 1);
            result->packet_type = (eproto_packet_type_t)pkt_type;
            ring_buffer_read(rb, &result->source_address, 1);
            ring_buffer_read(rb, &result->destination_address, 1);

            // 读取包ID（处理大小端）
            uint8_t packet_id_bytes[2];
            ring_buffer_read(rb, packet_id_bytes, 2);
            result->packet_id = (packet_id_bytes[0] << 8) | packet_id_bytes[1];

            // 读取数据
            if (data_length > 0) {
                result->data = (uint8_t*)parser->malloc_func(data_length);
                if (!result->data) {
                    return FRAME_PARSER_ERROR_MEMORY_ALLOC;
                }
                ring_buffer_read(rb, result->data, data_length);
            } else {
                result->data = NULL;
            }

            // 跳过CRC字段
            ring_buffer_discard(rb, 2);

            return FRAME_PARSER_OK;
        }

        // 未找到帧头，移动读位置跳过第一个字节
        ring_buffer_discard(rb, 1);
    }

    // 没有找到完整的帧
    if (!found_header) {
        return FRAME_PARSER_ERROR_NO_HEADER;
    } else if (ring_buffer_available(rb) > 0) {
        return FRAME_PARSER_ERROR_INSUFFICIENT_DATA;
    } else {
        return FRAME_PARSER_ERROR_NO_HEADER;
    }
}

// 释放解析结果
void frame_parser_free_result(frame_parser_t* parser, eproto_frame_t* result) {
    if (!parser || !result)
        return;

    if (result->data) {
        parser->free_func(result->data);
        result->data = NULL;
    }

    // 重置其他字段
    result->header = 0;
    result->version = 0;
    result->length = 0;
    result->packet_type = 0;
    result->source_address = 0;
    result->destination_address = 0;
    result->packet_id = 0;
}

// 打包帧
uint16_t frame_parser_pack_frame(uint8_t* buffer, uint16_t buffer_size, uint8_t frame_header, uint8_t source_address,
                                 uint8_t destination_address, uint16_t packet_id, uint8_t packet_type, uint8_t* data,
                                 uint16_t data_length) {
    if (!buffer)
        return 0;

    uint16_t total_length = EPROTO_FRAME_HEADER_LENGTH + data_length;

    if (buffer_size < total_length) {
        return 0;
    }

    uint16_t pos = 0;

    // 帧头
    buffer[pos++] = frame_header;
    // 版本
    buffer[pos++] = EPROTO_PROTOCOL_VERSION;
    // 长度
    buffer[pos++] = (data_length >> 8) & 0xFF;
    buffer[pos++] = data_length & 0xFF;
    // 包类型
    buffer[pos++] = packet_type;
    // 源地址
    buffer[pos++] = source_address;
    // 目标地址
    buffer[pos++] = destination_address;
    // 包ID
    buffer[pos++] = (packet_id >> 8) & 0xFF;
    buffer[pos++] = packet_id & 0xFF;
    // 数据
    if (data && data_length > 0) {
        for (uint16_t i = 0; i < data_length; i++) {
            buffer[pos++] = data[i];
        }
    }

    // 计算CRC（不包括CRC字段本身）
    uint16_t crc = crc16_ccitt(buffer, pos);
    buffer[pos++] = (crc >> 8) & 0xFF;
    buffer[pos++] = crc & 0xFF;

    return pos;
}
