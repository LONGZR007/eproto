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

#include "eproto.h"
#include "eproto_crc16.h"
#include "eproto_packet_node.h"
#include "eproto_ring_buffer.h"
#include "eproto_frame_parser.h"
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

// 静态函数声明
static eproto_bus_manager_t* eproto_find_bus_by_addr(eproto_t* eproto, uint8_t bus_addr);
static eproto_bus_manager_t* eproto_find_bus_by_destination(eproto_t* eproto, uint8_t dst_addr);
static eproto_error_t eproto_handle_broadcast(eproto_t* eproto, uint8_t* data, uint16_t length,
                                              eproto_packet_callback_t callback, void* private_data, uint8_t need_reply);
static void eproto_process_received_data(eproto_t* eproto);
static void eproto_process_bus_received_data(eproto_t* eproto, eproto_bus_manager_t* bus_mgr, uint8_t bus_index);
static void eproto_process_user_send_packet(eproto_t* eproto, eproto_bus_manager_t* bus_mgr, eproto_frame_t* frame,
                                            uint8_t is_retransmit, uint8_t is_handshake);
static void eproto_process_protocol_ack_packet(eproto_t* eproto, eproto_bus_manager_t* bus_mgr, eproto_frame_t* frame);
static void eproto_process_user_reply_packet(eproto_t* eproto, eproto_bus_manager_t* bus_mgr, eproto_frame_t* frame);
static void eproto_process_parse_error(eproto_bus_manager_t* bus_mgr, eproto_frame_parser_error_t error);
static bool eproto_handle_retransmit(eproto_t* eproto, eproto_bus_manager_t* bus_mgr);
static void eproto_send_normal_packet(eproto_t* eproto, eproto_bus_manager_t* bus_mgr);
static void eproto_process_send_queue(eproto_t* eproto);
static void eproto_process_wait_queue(eproto_t* eproto);
static bool eproto_send_handshake_packet(eproto_t* eproto, eproto_bus_manager_t* bus_mgr);
static void eproto_forward_frame(eproto_t* eproto, eproto_bus_manager_t* current_bus_mgr, eproto_frame_t* frame);
static void eproto_forward_protocol_ack(eproto_t* eproto, eproto_bus_manager_t* current_bus_mgr, eproto_frame_t* frame);
static eproto_error_t eproto_send_frame(eproto_t* eproto, eproto_bus_manager_t* bus_mgr, uint8_t src_addr,
                                        uint8_t dst_addr, uint16_t packet_id, uint8_t* data, uint16_t length,
                                        uint8_t packet_type);
static eproto_error_t eproto_send_response(eproto_t* eproto, uint8_t bus_addr, uint16_t packet_id, uint8_t* data,
                                           uint16_t length, uint8_t packet_type);
static uint32_t eproto_find_min_timeout_timestamp(eproto_t* eproto);

// 包类型名称数组
static const char* eproto_packet_type_names[] = {
    "USER_SEND",    // EPROTO_PACKET_TYPE_USER_SEND
    "USER_REPLY",   // EPROTO_PACKET_TYPE_USER_REPLY
    "PROTOCOL_ACK"  // EPROTO_PACKET_TYPE_PROTOCOL_ACK
};

// ====================================
// 初始化与销毁
// ====================================

// 初始化函数
eproto_error_t eproto_init(eproto_t* eproto, eproto_user_functions_t* user_functions) {
    if (!eproto || !user_functions)
        return EPROTO_ERROR_INVALID_FRAME;

    // 保存用户函数
    memcpy(&eproto->user_functions, user_functions, sizeof(eproto_user_functions_t));

    // 确保 signal_send 函数指针被正确初始化
    if (!eproto->user_functions.signal_send) {
        eproto->user_functions.signal_send = NULL;
    }

    // 检查内存分配接口，用户必须提供
    if (!eproto->user_functions.malloc || !eproto->user_functions.free) {
        EPROTO_ERROR_LOG("Error: Memory allocation functions must be provided\n");
        return EPROTO_ERROR_INVALID_FRAME;
    }

    // 初始化总线管理器
    for (uint8_t i = 0; i < EPROTO_MAX_BUS_COUNT; i++) {
        eproto->bus_managers[i].bus.send = NULL;
        eproto->bus_managers[i].name = NULL;

        // 初始化接口函数
        eproto->bus_managers[i].status_callback = NULL;
        eproto->bus_managers[i].receive_callback = NULL;
        eproto->bus_managers[i].forward_callback = NULL;  // 新增：初始化转发回调
        // 初始化状态变量
        eproto->bus_managers[i].next_packet_id = 1;
        eproto->bus_managers[i].last_id = 0;
        eproto->bus_managers[i].crc_error_count = 0;
#ifdef EPROTO_ENABLE_HANDSHAKE
        eproto->bus_managers[i].handshake_required = 0;
#endif
        eproto->bus_managers[i].current_send_node = NULL;
        // 初始化目标设备地址数组
        eproto->bus_managers[i].destination_device_count = 0;

        // 初始化设备队列
        EPROTO_INIT_LIST_HEAD(&eproto->bus_managers[i].device_queues.send_queue);
        EPROTO_INIT_LIST_HEAD(&eproto->bus_managers[i].device_queues.wait_queue);
    }

    return EPROTO_OK;
}

// 销毁函数
void eproto_destroy(eproto_t* eproto) {
    if (!eproto)
        return;

    // 销毁设备队列（不释放用户提供的缓冲区，由用户自己负责）
    for (uint8_t i = 0; i < EPROTO_MAX_BUS_COUNT; i++) {
        // 加锁保护发送队列操作
        if (eproto->user_functions.lock) {
            eproto->user_functions.lock();
        }

        // 销毁设备队列
        eproto_packet_node_destroy_all(eproto->user_functions.free, &eproto->bus_managers[i].device_queues.send_queue);
        eproto_packet_node_destroy_all(eproto->user_functions.free, &eproto->bus_managers[i].device_queues.wait_queue);

        // 解锁
        if (eproto->user_functions.unlock) {
            eproto->user_functions.unlock();
        }
    }
}

// ====================================
// 总线管理核心
// ====================================

// 根据总线地址查找总线管理器（查找自己所在的总线）
static eproto_bus_manager_t* eproto_find_bus_by_addr(eproto_t* eproto, uint8_t bus_addr) {
    // 根据 bus_addr 直接匹配总线管理器的 bus.self_addr
    for (uint8_t i = 0; i < EPROTO_MAX_BUS_COUNT; i++) {
        if (eproto->bus_managers[i].bus.send && eproto->bus_managers[i].bus.self_addr == bus_addr) {
            return &eproto->bus_managers[i];
        }
    }

    return NULL;
}

// 根据目标设备地址查找挂载的总线管理器
static eproto_bus_manager_t* eproto_find_bus_by_destination(eproto_t* eproto, uint8_t dst_addr) {
    // 根据目标设备地址查找挂载的总线
    for (uint8_t i = 0; i < EPROTO_MAX_BUS_COUNT; i++) {
        if (eproto->bus_managers[i].bus.send) {
            for (uint8_t j = 0; j < eproto->bus_managers[i].destination_device_count; j++) {
                if (eproto->bus_managers[i].destination_devices[j] == dst_addr) {
                    return &eproto->bus_managers[i];
                }
            }
        }
    }

    return NULL;
}

// 添加总线
eproto_error_t eproto_add_bus(eproto_t* eproto, uint8_t self_addr, eproto_bus_send_func_t send_func, uint8_t* rx_buffer,
                              uint16_t rx_buffer_size, const char* name,
                              eproto_status_callback_t status_callback, receive_callback_t receive_callback,
                              eproto_forward_callback_t forward_callback) {
    if (!eproto || !send_func || !rx_buffer || rx_buffer_size == 0)
        return EPROTO_ERROR_INVALID_FRAME;

    // 查找空闲的总线管理器
    uint8_t manager_index = 0;
    for (; manager_index < EPROTO_MAX_BUS_COUNT; manager_index++) {
        if (!eproto->bus_managers[manager_index].bus.send ||
            (eproto->bus_managers[manager_index].bus.self_addr == self_addr)) {
            break;
        }
    }

    if (manager_index >= EPROTO_MAX_BUS_COUNT)
        return EPROTO_ERROR_ROUTE_NOT_FOUND;

    // 初始化或更新总线管理器
    eproto->bus_managers[manager_index].bus.send = send_func;
    eproto->bus_managers[manager_index].bus.self_addr = self_addr;
    eproto->bus_managers[manager_index].name = name;

    // 设置接口函数
    eproto->bus_managers[manager_index].status_callback = status_callback;
    eproto->bus_managers[manager_index].receive_callback = receive_callback;
    eproto->bus_managers[manager_index].forward_callback = forward_callback;  // 新增：设置转发回调
    // 初始化目标设备地址数组
    eproto->bus_managers[manager_index].destination_device_count = 0;

    // 初始化接收缓冲区（必须由用户提供）
    eproto_ring_buffer_init(&eproto->bus_managers[manager_index].bus.rx_buffer, rx_buffer, rx_buffer_size);

    // 初始化帧解析器
    eproto_frame_parser_config_t parser_config;
    parser_config.frame_header = EPROTO_FRAME_HEADER;
    parser_config.max_frame_length = EPROTO_FRAME_HEADER_LENGTH + EPROTO_MAX_PACKET_LENGTH;
    eproto_frame_parser_init(&eproto->bus_managers[manager_index].parser, &parser_config, eproto->user_functions.malloc,
                             eproto->user_functions.free);

    // 初始化设备队列
    EPROTO_INIT_LIST_HEAD(&eproto->bus_managers[manager_index].device_queues.send_queue);
    EPROTO_INIT_LIST_HEAD(&eproto->bus_managers[manager_index].device_queues.wait_queue);

#ifdef EPROTO_ENABLE_HANDSHAKE
    // 初始化总线需要握手
    eproto->bus_managers[manager_index].handshake_required = 1;
    EPROTO_INFO_LOG("%s: Bus initialized with handshake required\n", name);
#endif

    return EPROTO_OK;
}

// 添加目标设备地址
eproto_error_t eproto_add_destination_device(eproto_t* eproto, uint8_t self_addr, uint8_t dst_addr) {
    if (!eproto)
        return EPROTO_ERROR_INVALID_FRAME;

    // 查找对应的总线管理器
    eproto_bus_manager_t* bus_mgr = NULL;
    for (uint8_t i = 0; i < EPROTO_MAX_BUS_COUNT; i++) {
        if (eproto->bus_managers[i].bus.send && eproto->bus_managers[i].bus.self_addr == self_addr) {
            bus_mgr = &eproto->bus_managers[i];
            break;
        }
    }

    if (!bus_mgr)
        return EPROTO_ERROR_ROUTE_NOT_FOUND;

    // 检查目标设备地址是否已经存在
    for (uint8_t i = 0; i < bus_mgr->destination_device_count; i++) {
        if (bus_mgr->destination_devices[i] == dst_addr) {
            return EPROTO_OK;  // 已经存在，直接返回成功
        }
    }

    // 检查目标设备地址数组是否已满
    if (bus_mgr->destination_device_count >= EPROTO_MAX_DESTINATION_DEVICES) {
        return EPROTO_ERROR_BUFFER_FULL;
    }

    // 添加目标设备地址
    bus_mgr->destination_devices[bus_mgr->destination_device_count] = dst_addr;
    bus_mgr->destination_device_count++;

    return EPROTO_OK;
}

#ifdef EPROTO_ENABLE_HANDSHAKE
// 设置总线握手标志
eproto_error_t eproto_set_handshake(eproto_t* eproto, uint8_t bus_addr, uint8_t required) {
    if (!eproto)
        return EPROTO_ERROR_INVALID_FRAME;

    // 查找对应的总线管理器
    eproto_bus_manager_t* bus_mgr = eproto_find_bus_by_addr(eproto, bus_addr);
    if (!bus_mgr)
        return EPROTO_ERROR_ROUTE_NOT_FOUND;

    // 设置握手标志
    bus_mgr->handshake_required = required;
    EPROTO_INFO_LOG("%s: Set handshake required: %d\n", EPROTO_BUS_NAME(bus_mgr), required);

    return EPROTO_OK;
}
#endif

// 获取状态
uint8_t eproto_get_status(eproto_t* eproto, uint8_t bus_addr) {
    if (!eproto)
        return 0;

    // 查找对应的总线管理器
    eproto_bus_manager_t* bus_mgr = eproto_find_bus_by_addr(eproto, bus_addr);
    if (!bus_mgr)
        return 0;

        // 返回是否需要握手
#ifdef EPROTO_ENABLE_HANDSHAKE
    return bus_mgr->handshake_required;
#else
    return 0;
#endif
}

// ====================================
// 总线操作 - 数据发送
// ====================================

// 处理广播发送
static eproto_error_t eproto_handle_broadcast(eproto_t* eproto, uint8_t* data, uint16_t length,
                                              eproto_packet_callback_t callback, void* private_data, uint8_t need_reply) {
    if (!eproto)
        return EPROTO_ERROR_INVALID_FRAME;

    bool has_active_bus = false;

    // 遍历所有总线管理器
    for (uint8_t i = 0; i < EPROTO_MAX_BUS_COUNT; i++) {
        eproto_bus_manager_t* bus_mgr = &eproto->bus_managers[i];
        if (!bus_mgr->bus.send)
            continue;

        has_active_bus = true;

        // 生成用户包的包ID
        uint16_t packet_id = bus_mgr->next_packet_id++;
        if (bus_mgr->next_packet_id == 0)
            bus_mgr->next_packet_id = 1;  // 避免0值

        // 创建广播包节点，重发次数和超时时间强制为0
        eproto_node_t* node = eproto_packet_node_create(
            eproto->user_functions.malloc, eproto->user_functions.free, bus_mgr->bus.self_addr, EPROTO_BROADCAST_ADDRESS,
            packet_id, data, length, callback, private_data, need_reply, EPROTO_PACKET_TYPE_USER_SEND, 0, 0);
        if (!node)
            continue;  // 创建失败，继续处理其他总线

        // 加锁保护发送队列操作
        if (eproto->user_functions.lock) {
            eproto->user_functions.lock();
        }

        // 添加用户包到发送队列
        eproto_packet_node_add(&bus_mgr->device_queues.send_queue, node);

        // 解锁
        if (eproto->user_functions.unlock) {
            eproto->user_functions.unlock();
        }
    }

    if (!has_active_bus) {
        return EPROTO_ERROR_ROUTE_NOT_FOUND;
    }

    // 调用用户提供的发送信号接口（如果有）
    if (eproto->user_functions.signal_send) {
        eproto->user_functions.signal_send();
    }

    return EPROTO_OK;
}

// 主动发送数据接口（扩展）
eproto_error_t eproto_send_ex(eproto_t* eproto, uint8_t dst_addr, uint8_t* data, uint16_t length,
                              eproto_packet_callback_t callback, void* private_data, uint8_t need_reply,
                              uint8_t max_retry_count, uint32_t timeout_ms) {
    if (!eproto)
        return EPROTO_ERROR_INVALID_FRAME;
    if (length > EPROTO_MAX_PACKET_LENGTH)
        return EPROTO_ERROR_INVALID_FRAME;

    // 检查是否是广播地址
    if (dst_addr == EPROTO_BROADCAST_ADDRESS) {
        // 处理广播发送
        return eproto_handle_broadcast(eproto, data, length, callback, private_data, need_reply);
    }

    // 找到对应的总线管理器
    eproto_bus_manager_t* bus_mgr = eproto_find_bus_by_destination(eproto, dst_addr);
    if (!bus_mgr)
        return EPROTO_ERROR_ROUTE_NOT_FOUND;

    // 生成用户包的包ID
    uint16_t packet_id = bus_mgr->next_packet_id++;
    if (bus_mgr->next_packet_id == 0)
        bus_mgr->next_packet_id = 1;  // 避免0值

    // 创建用户包节点
    eproto_node_t* node = eproto_packet_node_create(
        eproto->user_functions.malloc, eproto->user_functions.free, bus_mgr->bus.self_addr, dst_addr, packet_id, data,
        length, callback, private_data, need_reply, EPROTO_PACKET_TYPE_USER_SEND, max_retry_count, timeout_ms);
    if (!node)
        return EPROTO_ERROR_BUFFER_FULL;

    // 加锁保护发送队列操作
    if (eproto->user_functions.lock) {
        eproto->user_functions.lock();
    }

    // 添加用户包到发送队列
    eproto_packet_node_add(&bus_mgr->device_queues.send_queue, node);

    // 解锁
    if (eproto->user_functions.unlock) {
        eproto->user_functions.unlock();
    }

    // 调用用户提供的发送信号接口（如果有）
    if (eproto->user_functions.signal_send) {
        eproto->user_functions.signal_send();
    }

    return EPROTO_OK;
}

// 主动发送数据接口
eproto_error_t eproto_send(eproto_t* eproto, uint8_t dst_addr, uint8_t* data, uint16_t length,
                           eproto_packet_callback_t callback, void* private_data, uint8_t need_reply) {
    return eproto_send_ex(eproto, dst_addr, data, length, callback, private_data, need_reply,
                          EPROTO_DEFAULT_MAX_RETRY_COUNT, EPROTO_DEFAULT_RETRY_TIMEOUT_MS);
}

// 用户回复包发送接口（扩展）
eproto_error_t eproto_send_user_reply_ex(eproto_t* eproto, uint8_t dst_addr, uint16_t packet_id, uint8_t* data,
                                         uint16_t length, uint8_t max_retry_count, uint32_t timeout_ms) {
    if (!eproto)
        return EPROTO_ERROR_INVALID_FRAME;
    if (length > EPROTO_MAX_PACKET_LENGTH)
        return EPROTO_ERROR_INVALID_FRAME;

    // 找到对应的总线管理器
    eproto_bus_manager_t* bus_mgr = eproto_find_bus_by_destination(eproto, dst_addr);
    if (!bus_mgr)
        return EPROTO_ERROR_ROUTE_NOT_FOUND;

    // 创建链表节点（没有回调，不需要等待）
    eproto_node_t* node = eproto_packet_node_create(eproto->user_functions.malloc, eproto->user_functions.free,
                                                    bus_mgr->bus.self_addr, dst_addr, packet_id, data, length, NULL, NULL,
                                                    0, EPROTO_PACKET_TYPE_USER_REPLY, max_retry_count, timeout_ms);
    if (!node)
        return EPROTO_ERROR_BUFFER_FULL;

    // 加锁保护发送队列操作
    if (eproto->user_functions.lock) {
        eproto->user_functions.lock();
    }

    // 添加到对应设备的发送队列
    eproto_packet_node_add(&bus_mgr->device_queues.send_queue, node);

    // 解锁
    if (eproto->user_functions.unlock) {
        eproto->user_functions.unlock();
    }

    // 调用用户提供的发送信号接口（如果有）
    if (eproto->user_functions.signal_send) {
        eproto->user_functions.signal_send();
    }

    return EPROTO_OK;
}

// 用户回复包发送接口
eproto_error_t eproto_send_user_reply(eproto_t* eproto, uint8_t dst_addr, uint16_t packet_id, uint8_t* data,
                                      uint16_t length) {
    return eproto_send_user_reply_ex(eproto, dst_addr, packet_id, data, length, EPROTO_DEFAULT_MAX_RETRY_COUNT,
                                     EPROTO_DEFAULT_RETRY_TIMEOUT_MS);
}

// ====================================
// 总线操作 - 数据接收
// ====================================

// 接收数据处理（由中断或轮询调用）
// bus_addr: 总线地址
void eproto_receive_data(eproto_t* eproto, uint8_t bus_addr, const uint8_t* data, size_t len) {
    if (!eproto || !data || len == 0)
        return;
    eproto_bus_manager_t* bus_mgr = eproto_find_bus_by_addr(eproto, bus_addr);
    if (bus_mgr) {
        eproto_ring_buffer_write(&bus_mgr->bus.rx_buffer, data, len);

        // 调用用户提供的发送信号接口（如果有）
        if (eproto->user_functions.signal_send) {
            eproto->user_functions.signal_send();
        }
    }
}

// 处理接收到的数据
static void eproto_process_received_data(eproto_t* eproto) {
    if (!eproto)
        return;

    // 遍历所有总线管理器
    for (uint8_t bus_index = 0; bus_index < EPROTO_MAX_BUS_COUNT; bus_index++) {
        eproto_bus_manager_t* bus_mgr = &eproto->bus_managers[bus_index];
        if (!bus_mgr->bus.send)
            continue;

        eproto_process_bus_received_data(eproto, bus_mgr, bus_index);
    }
}

// 处理单个总线的接收到的数据
static void eproto_process_bus_received_data(eproto_t* eproto, eproto_bus_manager_t* bus_mgr, uint8_t bus_index) {
    eproto_ring_buffer_t* rx_buffer = &bus_mgr->bus.rx_buffer;

    if (eproto_ring_buffer_available(rx_buffer) > 0) {
        EPROTO_DEBUG_LOG("%s: Processing %d bytes in bus %d\n", EPROTO_BUS_NAME(bus_mgr),
                         eproto_ring_buffer_available(rx_buffer), bus_index);
    }

    // 尝试解析帧
    while (eproto_ring_buffer_available(rx_buffer) > 0) {
        eproto_frame_t frame;
        eproto_frame_parser_error_t error = eproto_frame_parser_parse(rx_buffer, &bus_mgr->parser, &frame);

        if (error == EPROTO_FRAME_PARSER_OK) {
            // 检查设备地址是否匹配
            if (frame.dst_addr != bus_mgr->bus.self_addr) {
                // 根据包类型处理转发
                if (frame.packet_type == EPROTO_PACKET_TYPE_PROTOCOL_ACK) {
                    // 转发协议应答包
                    eproto_forward_protocol_ack(eproto, bus_mgr, &frame);
                } else {
                    // 转发普通数据帧
                    eproto_forward_frame(eproto, bus_mgr, &frame);
                }

                // 释放解析结果
                eproto_frame_parser_free_result(&bus_mgr->parser, &frame);
                continue;
            }

            // 去除重发标志和握手标志，获取实际包类型
            uint8_t actual_packet_type =
                frame.packet_type & ~(EPROTO_PACKET_TYPE_RETRANSMIT_FLAG | EPROTO_PACKET_TYPE_HANDSHAKE_FLAG);

            EPROTO_INFO_LOG(
                "%s: Received valid frame from %02X, packet ID: %d, "
                "type: %s\n",
                EPROTO_BUS_NAME(bus_mgr), frame.src_addr, frame.packet_id,
                eproto_packet_type_names[actual_packet_type]);

            // 检查是否是重发包
            uint8_t is_retransmit = frame.packet_type & EPROTO_PACKET_TYPE_RETRANSMIT_FLAG;
            // 检查是否是握手包
            uint8_t is_handshake = frame.packet_type & EPROTO_PACKET_TYPE_HANDSHAKE_FLAG;

            // 根据实际包类型处理
            switch (actual_packet_type) {
                case EPROTO_PACKET_TYPE_USER_SEND:
                    eproto_process_user_send_packet(eproto, bus_mgr, &frame, is_retransmit, is_handshake);
                    break;

                case EPROTO_PACKET_TYPE_PROTOCOL_ACK:
                    eproto_process_protocol_ack_packet(eproto, bus_mgr, &frame);
                    break;

                case EPROTO_PACKET_TYPE_USER_REPLY:
                    eproto_process_user_reply_packet(eproto, bus_mgr, &frame);
                    break;
            }

            // 释放解析结果
            eproto_frame_parser_free_result(&bus_mgr->parser, &frame);
        } else if (error == EPROTO_FRAME_PARSER_ERROR_NO_HEADER ||
                   error == EPROTO_FRAME_PARSER_ERROR_INSUFFICIENT_DATA) {
            // 没有找到帧头或数据不足，退出循环
            break;
        } else {
            // 处理其他错误
            eproto_process_parse_error(bus_mgr, error);
        }
    }
}

// ====================================
// 总线操作 - 包处理
// ====================================

// 处理用户发送包
static void eproto_process_user_send_packet(eproto_t* eproto, eproto_bus_manager_t* bus_mgr, eproto_frame_t* frame,
                                            uint8_t is_retransmit, uint8_t is_handshake) {
    // 发送协议层应答包
    EPROTO_INFO_LOG("%s: Received user send packet, sending protocol ACK\n", EPROTO_BUS_NAME(bus_mgr));
    eproto_send_response(eproto, frame->src_addr, frame->packet_id, NULL, 0, EPROTO_PACKET_TYPE_PROTOCOL_ACK);

#ifdef EPROTO_ENABLE_HANDSHAKE
    if (is_handshake) {
        // 处理握手包
        EPROTO_INFO_LOG("%s: Received handshake packet\n", EPROTO_BUS_NAME(bus_mgr));

        // 清除握手标志（收到握手包清一次）
        bus_mgr->handshake_required = 0;
        EPROTO_INFO_LOG("%s: Handshake flag cleared (received handshake packet)\n", EPROTO_BUS_NAME(bus_mgr));

        // 调用状态回调告诉用户握手成功
        if (bus_mgr->status_callback) {
            bus_mgr->status_callback(EPROTO_STATUS_HANDSHAKE_SUCCESS, NULL, 0);
        }

        // 握手改为不需要回复包，直接返回
        return;
    }
#endif

    // 检查是否是重发包
    if (is_retransmit) {
        EPROTO_INFO_LOG("%s: Received retransmit packet, checking ID: %d\n", EPROTO_BUS_NAME(bus_mgr),
                        frame->packet_id);
        // 如果是重发包，检查包ID是否与上次处理的ID相同
        if (frame->packet_id == bus_mgr->last_id) {
            EPROTO_INFO_LOG(
                "%s: Retransmit packet ID %d matches last ID, "
                "skipping callback\n",
                EPROTO_BUS_NAME(bus_mgr), frame->packet_id);
            // ID相同，不调用回调函数
            return;
        } else {
            EPROTO_INFO_LOG(
                "%s: Retransmit packet ID %d is new, calling "
                "callback\n",
                EPROTO_BUS_NAME(bus_mgr), frame->packet_id);
        }
    }

    // 调用接收回调函数
    if (bus_mgr->receive_callback) {
        EPROTO_DEBUG_LOG("%s: Calling receive callback\n", EPROTO_BUS_NAME(bus_mgr));
        bus_mgr->receive_callback(frame->src_addr, frame->packet_id, frame->data, frame->length);
        // 更新上次处理的包ID
        bus_mgr->last_id = frame->packet_id;
    }
}

// 处理协议层应答包
static void eproto_process_protocol_ack_packet(eproto_t* eproto, eproto_bus_manager_t* bus_mgr, eproto_frame_t* frame) {
    EPROTO_INFO_LOG("%s: Received protocol ACK\n", EPROTO_BUS_NAME(bus_mgr));

    // 检查是否需要处理协议应答包：ID匹配当前发送节点
    if (bus_mgr->current_send_node && bus_mgr->current_send_node->packet_id == frame->packet_id) {
        EPROTO_INFO_LOG("%s: It's an ACK for current sending packet %d\n", EPROTO_BUS_NAME(bus_mgr),
                        bus_mgr->current_send_node->packet_id);
        eproto_node_t* node = bus_mgr->current_send_node;

        // 检查是否是握手包的协议 ACK
#ifdef EPROTO_ENABLE_HANDSHAKE
        if (node->packet_type & EPROTO_PACKET_TYPE_HANDSHAKE_FLAG) {
            EPROTO_INFO_LOG("%s: Handshake successful\n", EPROTO_BUS_NAME(bus_mgr));
            // 清除握手标志（收到协议层应答包清一次）
            if (bus_mgr->handshake_required) {
                bus_mgr->handshake_required = 0;
                EPROTO_INFO_LOG("%s: Handshake flag cleared (received protocol ACK)\n", EPROTO_BUS_NAME(bus_mgr));
            }
            // 调用状态回调告诉用户握手成功
            if (bus_mgr->status_callback) {
                bus_mgr->status_callback(EPROTO_STATUS_HANDSHAKE_SUCCESS, NULL, 0);
            }
            // 销毁握手节点
            eproto_packet_node_destroy(eproto->user_functions.free, node);
        } else {
#endif
            // 判断用户是否需要回复包
            if (node->need_reply) {
                // 用户需要回复，放入等待队列
                EPROTO_INFO_LOG("%s: User needs reply, adding to wait queue\n", EPROTO_BUS_NAME(bus_mgr));
                eproto_packet_node_add(&bus_mgr->device_queues.wait_queue, node);
            } else {
                // 用户不需要回复，直接调用回调
                EPROTO_INFO_LOG(
                    "%s: User doesn't need reply, calling callback "
                    "directly\n",
                    EPROTO_BUS_NAME(bus_mgr));
                if (node->callback) {
                    node->callback(EPROTO_SEND_SUCCESS, node->packet_id, frame->data, frame->length,
                                   node->private_data);
                }
                eproto_packet_node_destroy(eproto->user_functions.free, node);
            }
#ifdef EPROTO_ENABLE_HANDSHAKE
        }
#endif

        // 清空当前发送节点
        bus_mgr->current_send_node = NULL;
    }
}

// 处理用户回复包
static void eproto_process_user_reply_packet(eproto_t* eproto, eproto_bus_manager_t* bus_mgr, eproto_frame_t* frame) {
    // 发送协议层应答包
    EPROTO_INFO_LOG("%s: Received user reply packet, sending protocol ACK\n", EPROTO_BUS_NAME(bus_mgr));
    eproto_send_response(eproto, frame->src_addr, frame->packet_id, NULL, 0, EPROTO_PACKET_TYPE_PROTOCOL_ACK);

    // 从等待队列中移除
    eproto_node_t* reply_node = eproto_packet_node_remove(&bus_mgr->device_queues.wait_queue, frame->packet_id);
    if (reply_node) {
        // 收到了回复包，调用回调
        EPROTO_INFO_LOG("%s: Received reply packet, calling callback\n", EPROTO_BUS_NAME(bus_mgr));
        if (reply_node->callback) {
            reply_node->callback(EPROTO_SEND_SUCCESS, reply_node->packet_id, frame->data, frame->length,
                                 reply_node->private_data);
        }
        eproto_packet_node_destroy(eproto->user_functions.free, reply_node);
    } else {
        // 用户不需要回复，直接丢弃
        EPROTO_INFO_LOG("%s: User doesn't need reply, discarding\n", EPROTO_BUS_NAME(bus_mgr));
    }
}

// 处理解析错误
static void eproto_process_parse_error(eproto_bus_manager_t* bus_mgr, eproto_frame_parser_error_t error) {
    if (error == EPROTO_FRAME_PARSER_ERROR_CRC_CHECK) {
        // CRC校验失败
        EPROTO_WARNING_LOG("%s: CRC error\n", EPROTO_BUS_NAME(bus_mgr));
        bus_mgr->crc_error_count++;
        if (bus_mgr->crc_error_count >= 3) {
            // 多次连续CRC错误，通知用户
            if (bus_mgr->status_callback) {
                bus_mgr->status_callback(EPROTO_STATUS_MULTIPLE_CRC_ERRORS, NULL, 0);
            }
            bus_mgr->crc_error_count = 0;
        } else {
            // 通知用户CRC错误
            if (bus_mgr->status_callback) {
                bus_mgr->status_callback(EPROTO_STATUS_CRC_ERROR, NULL, 0);
            }
        }
    } else if (error == EPROTO_FRAME_PARSER_ERROR_INVALID_LENGTH) {
        // 无效帧长度
        EPROTO_WARNING_LOG("%s: Invalid frame length\n", EPROTO_BUS_NAME(bus_mgr));
    } else if (error == EPROTO_FRAME_PARSER_ERROR_MEMORY_ALLOC) {
        // 内存分配失败
        EPROTO_ERROR_LOG("%s: Memory allocation error\n", EPROTO_BUS_NAME(bus_mgr));
    }
}

// ====================================
// 总线操作 - 队列管理
// ====================================

// 处理重发逻辑
static bool eproto_handle_retransmit(eproto_t* eproto, eproto_bus_manager_t* bus_mgr) {
    uint32_t current_time = eproto->user_functions.get_timestamp();

    // 检查是否需要重发
    if (current_time - bus_mgr->current_send_node->timestamp > bus_mgr->current_send_node->timeout_ms) {
        if (bus_mgr->current_send_node->retry_count < bus_mgr->current_send_node->max_retry_count) {
            // 重发
            EPROTO_INFO_LOG("%s: Retrying packet %d, retry count: %d\n", EPROTO_BUS_NAME(bus_mgr),
                            bus_mgr->current_send_node->packet_id, bus_mgr->current_send_node->retry_count);
            // 重发时添加重发标志
            uint8_t retransmit_packet_type =
                bus_mgr->current_send_node->packet_type | EPROTO_PACKET_TYPE_RETRANSMIT_FLAG;
            eproto_send_frame(eproto, bus_mgr, bus_mgr->current_send_node->src_addr,
                              bus_mgr->current_send_node->dst_addr, bus_mgr->current_send_node->packet_id,
                              bus_mgr->current_send_node->data, bus_mgr->current_send_node->data_length,
                              retransmit_packet_type);
            bus_mgr->current_send_node->timestamp = current_time;
            bus_mgr->current_send_node->retry_count++;
            return true;  // 需要继续处理其他总线
        } else {
            // 达到最大重发次数，调用回调并移除
            EPROTO_WARNING_LOG("%s: Max retry reached for packet %d\n", EPROTO_BUS_NAME(bus_mgr),
                               bus_mgr->current_send_node->packet_id);

            // 保存当前节点的包类型
            uint8_t packet_type = bus_mgr->current_send_node->packet_type;

            if (bus_mgr->current_send_node->callback) {
                bus_mgr->current_send_node->callback(EPROTO_SEND_TIMEOUT, bus_mgr->current_send_node->packet_id, NULL,
                                                     0, bus_mgr->current_send_node->private_data);
            }
            eproto_packet_node_destroy(eproto->user_functions.free, bus_mgr->current_send_node);
            bus_mgr->current_send_node = NULL;

            // 只有当当前节点是握手包时，才处理发送队列中的节点
            if (packet_type & EPROTO_PACKET_TYPE_HANDSHAKE_FLAG) {
                // 检查发送队列是否不为空
                if (!eproto_list_empty(&bus_mgr->device_queues.send_queue)) {
                    // 取出第一个节点
                    eproto_node_t* send_node = eproto_packet_node_remove_first(&bus_mgr->device_queues.send_queue);
                    if (send_node) {
                        // 调用回调告诉用户发送超时
                        if (send_node->callback) {
                            send_node->callback(EPROTO_SEND_TIMEOUT, send_node->packet_id, NULL, 0,
                                                send_node->private_data);
                        }
                        // 销毁取出来的节点
                        eproto_packet_node_destroy(eproto->user_functions.free, send_node);
                    }
                }
            }

            return false;  // 继续处理该总线
        }
    } else {
        // 当前有发送节点且未超时，继续处理其他总线管理器
        return true;
    }
}

// 发送普通数据包
static void eproto_send_normal_packet(eproto_t* eproto, eproto_bus_manager_t* bus_mgr) {
    // 加锁保护发送队列操作
    if (eproto->user_functions.lock) {
        eproto->user_functions.lock();
    }

    // 取出第一个节点发送
    eproto_node_t* send_node = eproto_packet_node_remove_first(&bus_mgr->device_queues.send_queue);

    // 解锁
    if (eproto->user_functions.unlock) {
        eproto->user_functions.unlock();
    }

    if (!send_node)
        return;

    // 发送数据
    eproto_error_t error =
        eproto_send_frame(eproto, bus_mgr, send_node->src_addr, send_node->dst_addr, send_node->packet_id,
                          send_node->data, send_node->data_length, send_node->packet_type);
    if (error != EPROTO_OK) {
        // 发送失败，调用回调
        if (send_node->callback) {
            send_node->callback(EPROTO_SEND_ERROR, send_node->packet_id, NULL, 0, send_node->private_data);
        }
        eproto_packet_node_destroy(eproto->user_functions.free, send_node);
        return;
    }

    // 记录发送时间
    send_node->timestamp = eproto->user_functions.get_timestamp();
    send_node->retry_count = 0;

    // 如果重发次数和超时时间都是0，不需要设置为当前发送节点
    // 因为不需要进行重发和超时检查
    if (send_node->max_retry_count > 0 || send_node->timeout_ms > 0) {
        // 设置为当前发送节点
        bus_mgr->current_send_node = send_node;
    } else {
        // 不需要重发和超时检查，直接释放节点
        eproto_packet_node_destroy(eproto->user_functions.free, send_node);

        // 发送信号以便继续检查发送队列中的下一个包
        if (eproto->user_functions.signal_send) {
            eproto->user_functions.signal_send();
        }
    }
}

// 处理发送队列
static void eproto_process_send_queue(eproto_t* eproto) {
    if (!eproto)
        return;

    // 遍历所有总线管理器
    for (uint8_t manager_index = 0; manager_index < EPROTO_MAX_BUS_COUNT; manager_index++) {
        eproto_bus_manager_t* bus_mgr = &eproto->bus_managers[manager_index];
        if (!bus_mgr->bus.send)
            continue;

        // 检查当前是否有正在发送的节点，优先处理重发
        if (bus_mgr->current_send_node) {
            if (eproto_handle_retransmit(eproto, bus_mgr)) {
                continue;  // 重发后继续处理其他总线管理器
            }
        }

        // 加锁保护发送队列检查
        int queue_is_empty = 1;
        if (eproto->user_functions.lock) {
            eproto->user_functions.lock();
        }

        // 检查发送队列
        queue_is_empty = (&bus_mgr->device_queues.send_queue == bus_mgr->device_queues.send_queue.next);

        if (eproto->user_functions.unlock) {
            eproto->user_functions.unlock();
        }

        if (queue_is_empty)
            continue;

            // 检查是否需要握手
#ifdef EPROTO_ENABLE_HANDSHAKE
        if (bus_mgr->handshake_required) {
            if (eproto_send_handshake_packet(eproto, bus_mgr)) {
                continue;  // 发送握手包后继续处理其他总线管理器
            }
        }
#endif

        // 发送普通数据包
        eproto_send_normal_packet(eproto, bus_mgr);
    }
}

// 处理等待队列（检查超时）
static void eproto_process_wait_queue(eproto_t* eproto) {
    if (!eproto)
        return;

    // 遍历所有总线管理器
    for (uint8_t manager_index = 0; manager_index < EPROTO_MAX_BUS_COUNT; manager_index++) {
        eproto_bus_manager_t* bus_mgr = &eproto->bus_managers[manager_index];
        if (!bus_mgr->bus.send)
            continue;

        // 检查等待队列
        if (&bus_mgr->device_queues.wait_queue == bus_mgr->device_queues.wait_queue.next)
            continue;

        uint32_t current_time = eproto->user_functions.get_timestamp();
        struct eproto_list_head *pos, *n;
        eproto_node_t* node;

        eproto_list_for_each_safe(pos, n, &bus_mgr->device_queues.wait_queue) {
            node = eproto_list_entry(pos, eproto_node_t, list);

            // 检查是否超时
            if (current_time - node->timestamp > node->timeout_ms) {
                // 超时，调用回调并移除
                if (node->callback) {
                    node->callback(EPROTO_SEND_TIMEOUT, node->packet_id, NULL, 0, node->private_data);
                }

                // 从链表中删除节点
                eproto_list_del(pos);
                // 销毁节点
                eproto_packet_node_destroy(eproto->user_functions.free, node);
            }
        }
    }
}

// ====================================
// 总线操作 - 握手处理
// ====================================

#ifdef EPROTO_ENABLE_HANDSHAKE
// 发送握手包
static bool eproto_send_handshake_packet(eproto_t* eproto, eproto_bus_manager_t* bus_mgr) {
    EPROTO_INFO_LOG("%s: Handshake required, sending handshake packet first\n", EPROTO_BUS_NAME(bus_mgr));

    // 检查是否有目标设备
    if (bus_mgr->destination_device_count == 0) {
        EPROTO_ERROR_LOG("%s: No destination devices, cannot send handshake packet\n", EPROTO_BUS_NAME(bus_mgr));
        return true;  // 继续处理其他总线
    }

    // 调用用户的状态回调通知正在握手
    if (bus_mgr->status_callback) {
        EPROTO_INFO_LOG("%s: Calling status callback for handshake in progress\n", EPROTO_BUS_NAME(bus_mgr));
        bus_mgr->status_callback(EPROTO_STATUS_HANDSHAKE_IN_PROGRESS, NULL, 0);
    }

    // 生成握手包的包ID
    uint16_t handshake_packet_id = bus_mgr->next_packet_id++;
    if (bus_mgr->next_packet_id == 0)
        bus_mgr->next_packet_id = 1;  // 避免0值

    // 创建握手包节点（使用握手标志）
    uint8_t handshake_packet_type = EPROTO_PACKET_TYPE_USER_SEND | EPROTO_PACKET_TYPE_HANDSHAKE_FLAG;
    eproto_node_t* handshake_node =
        eproto_packet_node_create(eproto->user_functions.malloc, eproto->user_functions.free, bus_mgr->bus.self_addr,
                                  bus_mgr->destination_devices[0], handshake_packet_id, NULL, 0, NULL, NULL, 0,
                                  handshake_packet_type, EPROTO_HANDSHAKE_MAX_RETRY_COUNT, EPROTO_HANDSHAKE_TIMEOUT_MS);
    if (!handshake_node)
        return true;  // 继续处理其他总线

    // 直接发送握手包
    eproto_error_t error = eproto_send_frame(eproto, bus_mgr, handshake_node->src_addr, handshake_node->dst_addr,
                                             handshake_node->packet_id, handshake_node->data,
                                             handshake_node->data_length, handshake_node->packet_type);

    if (error == EPROTO_OK) {
        // 记录发送时间
        handshake_node->timestamp = eproto->user_functions.get_timestamp();
        handshake_node->retry_count = 0;

        // 设置为当前发送节点
        bus_mgr->current_send_node = handshake_node;
    } else {
        // 发送失败，销毁握手节点
        EPROTO_ERROR_LOG("%s: Failed to send handshake packet\n", EPROTO_BUS_NAME(bus_mgr));
        eproto_packet_node_destroy(eproto->user_functions.free, handshake_node);
    }

    return true;  // 继续处理其他总线
}
#endif

#ifdef EPROTO_ENABLE_HANDSHAKE
// 执行总线握手
eproto_error_t eproto_handshake(eproto_t* eproto, uint8_t bus_addr) {
    if (!eproto)
        return EPROTO_ERROR_INVALID_FRAME;

    // 查找对应的总线管理器
    eproto_bus_manager_t* bus_mgr = eproto_find_bus_by_addr(eproto, bus_addr);
    if (!bus_mgr)
        return EPROTO_ERROR_ROUTE_NOT_FOUND;

    // 检查是否需要握手
    if (!bus_mgr->handshake_required) {
        EPROTO_INFO_LOG("%s: Handshake not required\n", EPROTO_BUS_NAME(bus_mgr));
        return EPROTO_OK;
    }

    // 生成握手包的包ID
    uint16_t handshake_packet_id = bus_mgr->next_packet_id++;
    if (bus_mgr->next_packet_id == 0)
        bus_mgr->next_packet_id = 1;  // 避免0值

    // 创建握手包节点（使用握手标志）
    uint8_t handshake_packet_type = EPROTO_PACKET_TYPE_USER_SEND | EPROTO_PACKET_TYPE_HANDSHAKE_FLAG;
    eproto_node_t* handshake_node =
        eproto_packet_node_create(eproto->user_functions.malloc, eproto->user_functions.free, bus_mgr->bus.self_addr,
                                  bus_mgr->destination_devices[0], handshake_packet_id, NULL, 0, NULL, NULL, 0,
                                  handshake_packet_type, EPROTO_HANDSHAKE_MAX_RETRY_COUNT, EPROTO_HANDSHAKE_TIMEOUT_MS);
    if (!handshake_node) {
        EPROTO_ERROR_LOG("%s: Failed to create handshake node\n", EPROTO_BUS_NAME(bus_mgr));
        return EPROTO_ERROR_BUFFER_FULL;
    }

    // 加锁保护发送队列操作
    if (eproto->user_functions.lock) {
        eproto->user_functions.lock();
    }

    // 添加到发送队列
    eproto_packet_node_add(&bus_mgr->device_queues.send_queue, handshake_node);

    // 解锁
    if (eproto->user_functions.unlock) {
        eproto->user_functions.unlock();
    }

    EPROTO_INFO_LOG("%s: Handshake node added to send queue\n", EPROTO_BUS_NAME(bus_mgr));

    return EPROTO_OK;
}
#endif

// ====================================
// 总线操作 - 转发处理
// ====================================

// 转发数据帧
static void eproto_forward_frame(eproto_t* eproto, eproto_bus_manager_t* current_bus_mgr, eproto_frame_t* frame) {
    EPROTO_INFO_LOG("%s: Frame addred to %02X, checking for forwarding...\n", EPROTO_BUS_NAME(current_bus_mgr),
                    frame->dst_addr);

    // 检查是否是广播包
    if (frame->dst_addr == EPROTO_BROADCAST_ADDRESS) {
        EPROTO_INFO_LOG("%s: Broadcasting to all buses...\n", EPROTO_BUS_NAME(current_bus_mgr));

        // 遍历所有总线管理器
        for (uint8_t i = 0; i < EPROTO_MAX_BUS_COUNT; i++) {
            eproto_bus_manager_t* bus_mgr = &eproto->bus_managers[i];
            if (!bus_mgr->bus.send || bus_mgr == current_bus_mgr)
                continue;  // 跳过当前总线

            // 检查是否设置了转发回调
            uint8_t* forward_data = frame->data;
            uint16_t forward_length = frame->length;
            uint8_t* temp_data = NULL;
            void* private_data = NULL;  // 新增：局部变量存储私有数据
            eproto_forward_post_func_t post_func = NULL;
            
            if (bus_mgr->forward_callback) {
                eproto_error_t error = bus_mgr->forward_callback(
                    current_bus_mgr->bus.self_addr, bus_mgr->bus.self_addr,
                    frame->data, frame->length,
                    &temp_data, &forward_length,
                    &post_func,
                    &private_data
                );
                
                if (error == EPROTO_OK && temp_data) {
                    forward_data = temp_data;
                }
            }

            // 创建新的广播包节点，保持原始信息不变
            eproto_node_t* forward_node = eproto_packet_node_create(
                eproto->user_functions.malloc, eproto->user_functions.free, frame->src_addr, EPROTO_BROADCAST_ADDRESS,
                frame->packet_id, forward_data, forward_length, NULL, NULL,
                0,                          // need_reply - 转发包不需要等待回调
                frame->packet_type, 0, 0);  // 转发包不重发，无超时

            if (forward_node) {
                // 加锁保护发送队列操作
                if (eproto->user_functions.lock) {
                    eproto->user_functions.lock();
                }

                // 添加到目标总线的发送队列
                eproto_packet_node_add(&bus_mgr->device_queues.send_queue, forward_node);

                // 解锁
                if (eproto->user_functions.unlock) {
                    eproto->user_functions.unlock();
                }

                EPROTO_INFO_LOG("%s: Forwarded broadcast to bus %02X\n", EPROTO_BUS_NAME(current_bus_mgr),
                                bus_mgr->bus.self_addr);
            } else {
                EPROTO_ERROR_LOG("%s: Failed to create forward node for bus %02X\n", EPROTO_BUS_NAME(current_bus_mgr),
                                 bus_mgr->bus.self_addr);
            }
            
            // 调用后处理回调
            if (post_func) {
                post_func(
                    frame->src_addr, frame->dst_addr,
                    forward_data, forward_length,
                    private_data
                );
            }
        }

        // 调用当前总线的接收回调函数，通知用户收到了广播包
        if (current_bus_mgr->receive_callback) {
            EPROTO_DEBUG_LOG("%s: Calling receive callback for broadcast\n", EPROTO_BUS_NAME(current_bus_mgr));
            current_bus_mgr->receive_callback(frame->src_addr, frame->packet_id, frame->data, frame->length);
            // 广播包不更新last_id，避免影响重发检测
        }

        return;
    }

    // 查找目标设备所在的总线
    eproto_bus_manager_t* destination_bus_mgr = eproto_find_bus_by_destination(eproto, frame->dst_addr);
    if (destination_bus_mgr) {
        EPROTO_INFO_LOG("%s: Found destination bus for %02X, forwarding...\n", EPROTO_BUS_NAME(current_bus_mgr),
                        frame->dst_addr);

        // 检查是否设置了转发回调
        uint8_t* forward_data = frame->data;
        uint16_t forward_length = frame->length;
        uint8_t* temp_data = NULL;
        void* private_data = NULL;  // 新增：局部变量存储私有数据
        eproto_forward_post_func_t post_func = NULL;
        
        if (destination_bus_mgr->forward_callback) {
                eproto_error_t error = destination_bus_mgr->forward_callback(
                    current_bus_mgr->bus.self_addr, destination_bus_mgr->bus.self_addr,
                    frame->data, frame->length,
                    &temp_data, &forward_length,
                    &post_func,
                    &private_data
                );
                
                if (error == EPROTO_OK && temp_data) {
                    forward_data = temp_data;
                }
            }

        // 创建新的数据包节点，保持原始信息不变
        eproto_node_t* forward_node =
            eproto_packet_node_create(eproto->user_functions.malloc, eproto->user_functions.free, frame->src_addr,
                                      frame->dst_addr, frame->packet_id, forward_data, forward_length, NULL, NULL,
                                      0,                          // need_reply - 转发包不需要等待回调
                                      frame->packet_type, 0, 0);  // 转发包不重发，无超时

        if (forward_node) {
            // 加锁保护发送队列操作
            if (eproto->user_functions.lock) {
                eproto->user_functions.lock();
            }

            // 添加到目标总线的发送队列
            eproto_packet_node_add(&destination_bus_mgr->device_queues.send_queue, forward_node);

            // 解锁
            if (eproto->user_functions.unlock) {
                eproto->user_functions.unlock();
            }

            EPROTO_INFO_LOG("%s: Forwarded packet successfully\n", EPROTO_BUS_NAME(current_bus_mgr));
        } else {
            EPROTO_ERROR_LOG("%s: Failed to create forward node\n", EPROTO_BUS_NAME(current_bus_mgr));
        }
        
        // 调用后处理回调
        if (post_func) {
            post_func(
                frame->src_addr, frame->dst_addr,
                forward_data, forward_length,
                private_data
            );
        }
    } else {
        EPROTO_WARNING_LOG("%s: No route found for %02X, dropping packet\n", EPROTO_BUS_NAME(current_bus_mgr),
                           frame->dst_addr);
    }
}

// 直接发送协议应答包（用于转发）
static void eproto_forward_protocol_ack(eproto_t* eproto, eproto_bus_manager_t* current_bus_mgr,
                                        eproto_frame_t* frame) {
    EPROTO_INFO_LOG(
        "%s: Protocol ACK for packet %d, destination %02X is not me, "
        "forwarding...\n",
        EPROTO_BUS_NAME(current_bus_mgr), frame->packet_id, frame->dst_addr);

    // 查找目标设备所在的总线
    eproto_bus_manager_t* destination_bus_mgr = eproto_find_bus_by_destination(eproto, frame->dst_addr);
    if (destination_bus_mgr) {
        EPROTO_INFO_LOG("%s: Found destination bus for %02X, forwarding protocol ACK\n",
                        EPROTO_BUS_NAME(current_bus_mgr), frame->dst_addr);

        // 直接发送协议应答包，不需要放入队列
        eproto_send_frame(eproto, destination_bus_mgr, frame->src_addr, frame->dst_addr, frame->packet_id, frame->data,
                          frame->length, frame->packet_type);
        EPROTO_INFO_LOG("%s: Forwarded protocol ACK successfully\n", EPROTO_BUS_NAME(current_bus_mgr));
    } else {
        EPROTO_WARNING_LOG("%s: No route found for %02X, dropping protocol ACK\n", EPROTO_BUS_NAME(current_bus_mgr),
                           frame->dst_addr);
    }
}

// ====================================
// 总线辅助 - 工具函数
// ====================================

// 发送数据帧
static eproto_error_t eproto_send_frame(eproto_t* eproto, eproto_bus_manager_t* bus_mgr, uint8_t src_addr,
                                        uint8_t dst_addr, uint16_t packet_id, uint8_t* data, uint16_t length,
                                        uint8_t packet_type) {
    if (!bus_mgr || !eproto)
        return EPROTO_ERROR_ROUTE_NOT_FOUND;

    uint16_t buffer_size = EPROTO_FRAME_HEADER_LENGTH + length;
    uint8_t* send_buffer = (uint8_t*)eproto->user_functions.malloc(buffer_size);
    if (!send_buffer) {
        return EPROTO_ERROR_BUFFER_FULL;
    }

    uint16_t frame_length = eproto_frame_parser_pack_frame(send_buffer, buffer_size, EPROTO_FRAME_HEADER, src_addr,
                                                           dst_addr, packet_id, packet_type, data, length);

    if (frame_length == 0) {
        eproto->user_functions.free(send_buffer);
        return EPROTO_ERROR_INVALID_FRAME;
    }

    // 发送帧
    bus_mgr->bus.send(send_buffer, frame_length);

    // 释放缓冲区
    eproto->user_functions.free(send_buffer);

    return EPROTO_OK;
}

// 应答发送数据接口（内部使用）
// 注意：data 会被内部复制到分配的内存中，用户可以在调用后释放原始数据
static eproto_error_t eproto_send_response(eproto_t* eproto, uint8_t bus_addr, uint16_t packet_id, uint8_t* data,
                                           uint16_t length, uint8_t packet_type) {
    if (!eproto)
        return EPROTO_ERROR_INVALID_FRAME;
    if (length > EPROTO_MAX_PACKET_LENGTH)
        return EPROTO_ERROR_INVALID_FRAME;

    // 找到对应的总线管理器
    eproto_bus_manager_t* bus_mgr = eproto_find_bus_by_destination(eproto, bus_addr);
    if (!bus_mgr)
        return EPROTO_ERROR_ROUTE_NOT_FOUND;

    // 直接发送应答帧
    return eproto_send_frame(eproto, bus_mgr, bus_mgr->bus.self_addr, bus_addr, packet_id, data, length, packet_type);
}

// 查找所有总线下的最小超时时间戳
static uint32_t eproto_find_min_timeout_timestamp(eproto_t* eproto) {
    uint32_t min_timeout_timestamp = UINT32_MAX;

    for (uint8_t i = 0; i < EPROTO_MAX_BUS_COUNT; i++) {
        eproto_bus_manager_t* bus_mgr = &eproto->bus_managers[i];
        if (!bus_mgr->bus.send)
            continue;

        // 检查当前发送节点
        if (bus_mgr->current_send_node) {
            uint32_t node_timeout = bus_mgr->current_send_node->timestamp + bus_mgr->current_send_node->timeout_ms;
            if (node_timeout < min_timeout_timestamp) {
                min_timeout_timestamp = node_timeout;
            }
        }

        // 检查等待队列
        struct eproto_list_head* pos = NULL;
        eproto_list_for_each(pos, &bus_mgr->device_queues.wait_queue) {
            eproto_node_t* wait_node = eproto_list_entry(pos, eproto_node_t, list);
            uint32_t node_timeout = wait_node->timestamp + wait_node->timeout_ms;
            if (node_timeout < min_timeout_timestamp) {
                min_timeout_timestamp = node_timeout;
            }
        }
    }

    return min_timeout_timestamp;
}

// ====================================
// 主循环
// ====================================

// 等待信号
uint8_t eproto_wait_for_signal(eproto_t* eproto) {
    if (!eproto)
        return 0;
    if (eproto->user_functions.signal_wait) {
        return eproto->user_functions.signal_wait(eproto->user_functions.timeout_timestamp);
    } else {
        return EPROTO_SIGNAL_TIMEOUT;  // 没有信号回调接口，默认超时，走轮询模式
    }
}

// 处理函数（用于超时和重发）
uint32_t eproto_process(eproto_t* eproto) {
    if (!eproto)
        return 0;

    uint8_t signal_result = eproto_wait_for_signal(eproto);
    if (signal_result == EPROTO_SIGNAL_NO_PROGRESS) {
        return 0;
    }
    // 有信号或超时继续执行后续处理

    // 处理接收到的数据
    eproto_process_received_data(eproto);

    // 处理发送队列
    eproto_process_send_queue(eproto);

    // 处理等待队列（检查超时）
    eproto_process_wait_queue(eproto);

    // 查找所有总线下的最小超时时间戳
    uint32_t min_timeout_timestamp = eproto_find_min_timeout_timestamp(eproto);

    // 更新用户函数中的超时时间戳
    eproto->user_functions.timeout_timestamp = min_timeout_timestamp;

    return min_timeout_timestamp;
}
