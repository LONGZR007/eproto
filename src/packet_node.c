#include "packet_node.h"

// 创建包节点
eproto_node_t* packet_node_create(malloc_func_t malloc_func, free_func_t free_func, uint8_t source_address,
                                  uint8_t destination_address, uint16_t packet_id, uint8_t* data, uint16_t data_length,
                                  packet_callback_t callback, void* private_data, uint8_t no_wait, uint8_t packet_type,
                                  uint8_t max_retry_count, uint32_t timeout_ms) {
    eproto_node_t* node = (eproto_node_t*)malloc_func(sizeof(eproto_node_t));
    if (!node)
        return NULL;

    // 初始化链表节点
    INIT_LIST_HEAD(&node->list);
    node->source_address = source_address;
    node->destination_address = destination_address;
    node->packet_id = packet_id;
    node->callback = callback;
    node->private_data = private_data;
    node->no_wait = no_wait;
    node->packet_type = packet_type;
    node->timestamp = 0;
    node->retry_count = 0;
    node->max_retry_count = max_retry_count;
    node->timeout_ms = timeout_ms;
    node->data_length = data_length;

    // 复制数据到分配的内存
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

// 销毁包节点
void packet_node_destroy(free_func_t free_func, eproto_node_t* node) {
    if (node) {
        if (node->data) {
            free_func(node->data);
        }
        free_func(node);
    }
}

// 添加节点到链表尾部
void packet_node_add(struct list_head* head, eproto_node_t* node) {
    list_add_tail(&node->list, head);
}

// 根据packet_id移除节点
eproto_node_t* packet_node_remove(struct list_head* head, uint16_t packet_id) {
    struct list_head* pos;
    eproto_node_t* node;

    list_for_each(pos, head) {
        node = list_entry(pos, eproto_node_t, list);
        if (node->packet_id == packet_id) {
            list_del(pos);
            return node;
        }
    }

    return NULL;
}

// 移除链表第一个节点
eproto_node_t* packet_node_remove_first(struct list_head* head) {
    if (head == head->next)
        return NULL;

    struct list_head* first = head->next;
    eproto_node_t* node = list_entry(first, eproto_node_t, list);
    list_del(first);

    return node;
}

// 销毁所有节点
void packet_node_destroy_all(free_func_t free_func, struct list_head* head) {
    while (head != head->next) {
        eproto_node_t* node = packet_node_remove_first(head);
        packet_node_destroy(free_func, node);
    }
}

// 获取链表长度
uint8_t packet_node_get_length(struct list_head* head) {
    uint8_t length = 0;
    struct list_head* pos;

    list_for_each(pos, head) {
        length++;
    }

    return length;
}
