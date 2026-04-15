#ifndef PACKET_NODE_H
#define PACKET_NODE_H

#include <stdint.h>
#include "list.h"

// 内存分配函数类型
typedef void* (*malloc_func_t)(size_t size);
typedef void (*free_func_t)(void* ptr);

// 发送状态枚举
typedef enum {
    EPROTO_SEND_SUCCESS = 0,  // 发送成功
    EPROTO_SEND_TIMEOUT,      // 发送超时
    EPROTO_SEND_ERROR,        // 发送错误
    EPROTO_SEND_BUSY          // 发送忙
} eproto_send_status_t;

// 发送回调函数类型
typedef void (*packet_callback_t)(eproto_send_status_t status, uint16_t packet_id, uint8_t* data, uint16_t length,
                                  void* private_data);

// 包节点结构体（用于发送队列和等待队列）
typedef struct eproto_node {
    struct list_head list;        // 嵌入式包节点
    uint8_t source_address;       // 源设备地址（用于转发）
    uint8_t destination_address;  // 目标设备地址
    uint16_t packet_id;           // 包ID
    uint8_t* data;                // 发送数据指针
    uint16_t data_length;         // 数据长度
    packet_callback_t callback;   // 回调函数
    void* private_data;           // 私有数据
    uint8_t no_wait;              // 是否等待回调
    uint8_t packet_type;          // 包类型
    uint32_t timestamp;           // 发送时间戳
    uint8_t retry_count;          // 重发次数
    uint8_t max_retry_count;      // 最大重发次数
    uint32_t timeout_ms;          // 超时时间（毫秒）
} eproto_node_t;

// 包节点操作函数
eproto_node_t* packet_node_create(malloc_func_t malloc_func, free_func_t free_func, uint8_t source_address,
                                  uint8_t destination_address, uint16_t packet_id, uint8_t* data, uint16_t data_length,
                                  packet_callback_t callback, void* private_data, uint8_t no_wait, uint8_t packet_type,
                                  uint8_t max_retry_count, uint32_t timeout_ms);
void packet_node_destroy(free_func_t free_func, eproto_node_t* node);
void packet_node_add(struct list_head* head, eproto_node_t* node);
eproto_node_t* packet_node_remove(struct list_head* head, uint16_t packet_id);
eproto_node_t* packet_node_remove_first(struct list_head* head);
void packet_node_destroy_all(free_func_t free_func, struct list_head* head);
uint8_t packet_node_get_length(struct list_head* head);

#endif  // PACKET_NODE_H
