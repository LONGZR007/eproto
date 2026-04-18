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

#include "eproto_packet_node.h"

eproto_node_t* eproto_packet_node_create(eproto_malloc_func_t malloc_func, eproto_free_func_t free_func,
                                         uint8_t src_addr, uint8_t dst_addr, uint16_t packet_id, uint8_t* data,
                                         uint16_t data_length, eproto_packet_callback_t callback, void* private_data,
                                         uint8_t need_reply, uint8_t packet_type, uint8_t max_retry_count,
                                         uint32_t timeout_ms) {
    eproto_node_t* node = (eproto_node_t*)malloc_func(sizeof(eproto_node_t));
    if (!node)
        return NULL;

    EPROTO_INIT_LIST_HEAD(&node->list);
    node->src_addr = src_addr;
    node->dst_addr = dst_addr;
    node->packet_id = packet_id;
    node->callback = callback;
    node->private_data = private_data;
    node->need_reply = need_reply;
    node->packet_type = packet_type;
    node->timestamp = 0;
    node->retry_count = 0;
    node->max_retry_count = max_retry_count;
    node->timeout_ms = timeout_ms;
    node->data_length = data_length;

    if (data_length > 0 && data) {
        node->data = (uint8_t*)malloc_func(data_length);
        if (!node->data) {
            free_func(node);
            return NULL;
        }

        for (uint16_t i = 0; i < data_length; i++) {
            node->data[i] = data[i];
        }
    } else {
        node->data = NULL;
    }

    return node;
}

void eproto_packet_node_destroy(eproto_free_func_t free_func, eproto_node_t* node) {
    if (node) {
        if (node->data) {
            free_func(node->data);
        }
        free_func(node);
    }
}

void eproto_packet_node_add(struct eproto_list_head* head, eproto_node_t* node) {
    eproto_list_add_tail(&node->list, head);
}

eproto_node_t* eproto_packet_node_remove(struct eproto_list_head* head, uint16_t packet_id) {
    struct eproto_list_head* pos;
    eproto_node_t* node;

    eproto_list_for_each(pos, head) {
        node = eproto_list_entry(pos, eproto_node_t, list);
        if (node->packet_id == packet_id) {
            eproto_list_del(pos);
            return node;
        }
    }

    return NULL;
}

eproto_node_t* eproto_packet_node_remove_first(struct eproto_list_head* head) {
    if (head == head->next)
        return NULL;

    struct eproto_list_head* first = head->next;
    eproto_node_t* node = eproto_list_entry(first, eproto_node_t, list);
    eproto_list_del(first);

    return node;
}

void eproto_packet_node_destroy_all(eproto_free_func_t free_func, struct eproto_list_head* head) {
    while (head != head->next) {
        eproto_node_t* node = eproto_packet_node_remove_first(head);
        eproto_packet_node_destroy(free_func, node);
    }
}

uint8_t eproto_packet_node_get_length(struct eproto_list_head* head) {
    uint8_t length = 0;
    struct eproto_list_head* pos;

    eproto_list_for_each(pos, head) {
        length++;
    }

    return length;
}
