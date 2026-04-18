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

#ifndef EPROTO_PACKET_NODE_H
#define EPROTO_PACKET_NODE_H

#include <stdint.h>
#include "eproto_list.h"

typedef void* (*eproto_malloc_func_t)(size_t size);
typedef void (*eproto_free_func_t)(void* ptr);

typedef enum { EPROTO_SEND_SUCCESS = 0, EPROTO_SEND_TIMEOUT, EPROTO_SEND_ERROR, EPROTO_SEND_BUSY } eproto_send_status_t;

typedef void (*eproto_packet_callback_t)(eproto_send_status_t status, uint16_t packet_id, uint8_t* data,
                                         uint16_t length, void* private_data);

typedef struct eproto_node {
    struct eproto_list_head list;
    uint8_t src_addr;
    uint8_t dst_addr;
    uint16_t packet_id;
    uint8_t* data;
    uint16_t data_length;
    eproto_packet_callback_t callback;
    void* private_data;
    uint8_t need_reply;
    uint8_t packet_type;
    uint32_t timestamp;
    uint8_t retry_count;
    uint8_t max_retry_count;
    uint32_t timeout_ms;
} eproto_node_t;

eproto_node_t* eproto_packet_node_create(eproto_malloc_func_t malloc_func, eproto_free_func_t free_func,
                                         uint8_t src_addr, uint8_t dst_addr, uint16_t packet_id, uint8_t* data,
                                         uint16_t data_length, eproto_packet_callback_t callback, void* private_data,
                                         uint8_t need_reply, uint8_t packet_type, uint8_t max_retry_count,
                                         uint32_t timeout_ms);
void eproto_packet_node_destroy(eproto_free_func_t free_func, eproto_node_t* node);
void eproto_packet_node_add(struct eproto_list_head* head, eproto_node_t* node);
eproto_node_t* eproto_packet_node_remove(struct eproto_list_head* head, uint16_t packet_id);
eproto_node_t* eproto_packet_node_remove_first(struct eproto_list_head* head);
void eproto_packet_node_destroy_all(eproto_free_func_t free_func, struct eproto_list_head* head);
uint8_t eproto_packet_node_get_length(struct eproto_list_head* head);

#endif
