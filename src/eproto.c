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
static void eproto_handle_retransmit(eproto_t* eproto, eproto_bus_manager_t* bus_mgr);
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
// 包类型按bit定义：
//   bit0: 1=用户回复包, 0=用户发送包
//   bit1: 1=协议确认包
//   bit6: 1=握手包
//   bit7: 1=重发包
static const char* eproto_packet_type_names[] = {
    "USER_SEND",    // bit0=0, 用户发送包
    "USER_REPLY",   // bit0=1, 用户回复包
    "PROTOCOL_ACK"  // bit1=1, 协议确认包
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


        // 初始化状态变量
        eproto->bus_managers[i].next_packet_id = 1;
        for (uint8_t j = 0; j < EPROTO_MAX_CONCURRENT_SENDS; j++) {
            eproto->bus_managers[i].last_ids[j] = 0;
        }
        eproto->bus_managers[i].last_id_index = 0;
        eproto->bus_managers[i].crc_error_count = 0;
#if EPROTO_ENABLE_HANDSHAKE
        eproto->bus_managers[i].handshake_required = 0;
#endif
        // 初始化正在发送的节点队列
        EPROTO_INIT_LIST_HEAD(&eproto->bus_managers[i].device_queues.sending_queue);
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
eproto_error_t eproto_add_bus(eproto_t* eproto, eproto_bus_t* bus) {
    if (!eproto || !bus || !bus->send || !bus->rx_buffer || bus->rx_buffer_size == 0)
        return EPROTO_ERROR_INVALID_FRAME;

    // 查找空闲的总线管理器
    uint8_t manager_index = 0;
    for (; manager_index < EPROTO_MAX_BUS_COUNT; manager_index++) {
        if (!eproto->bus_managers[manager_index].bus.send ||
            (eproto->bus_managers[manager_index].bus.self_addr == bus->self_addr)) {
            break;
        }
    }

    if (manager_index >= EPROTO_MAX_BUS_COUNT)
        return EPROTO_ERROR_ROUTE_NOT_FOUND;

    // 初始化或更新总线管理器
    eproto->bus_managers[manager_index].bus.send = bus->send;
    eproto->bus_managers[manager_index].bus.self_addr = bus->self_addr;
    eproto->bus_managers[manager_index].bus.user_data = bus->user_data;
    eproto->bus_managers[manager_index].bus.name = bus->name;
    eproto->bus_managers[manager_index].bus.status_callback = bus->status_callback;
    eproto->bus_managers[manager_index].bus.receive_callback = bus->receive_callback;
    eproto->bus_managers[manager_index].bus.forward_callback = bus->forward_callback;

    // 初始化目标设备地址数组
    eproto->bus_managers[manager_index].destination_device_count = 0;

    // 初始化接收缓冲区（必须由用户提供）
    eproto_ring_buffer_init(&eproto->bus_managers[manager_index].rx_buffer, bus->rx_buffer, bus->rx_buffer_size);

    // 初始化帧解析器
    eproto_frame_parser_config_t parser_config;
    parser_config.frame_header = EPROTO_FRAME_HEADER;
    parser_config.max_frame_length = EPROTO_FRAME_HEADER_LENGTH + EPROTO_MAX_PACKET_LENGTH;
    eproto_frame_parser_init(&eproto->bus_managers[manager_index].parser, &parser_config, eproto->user_functions.malloc,
                             eproto->user_functions.free);

    // 初始化设备队列
    EPROTO_INIT_LIST_HEAD(&eproto->bus_managers[manager_index].device_queues.send_queue);
    EPROTO_INIT_LIST_HEAD(&eproto->bus_managers[manager_index].device_queues.wait_queue);

#if EPROTO_ENABLE_HANDSHAKE
    // 初始化总线需要握手
    eproto->bus_managers[manager_index].handshake_required = 1;
    EPROTO_INFO_LOG("%s: Bus initialized with handshake required\n", bus->name);
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

#if EPROTO_ENABLE_HANDSHAKE
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
#if EPROTO_ENABLE_HANDSHAKE
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
        eproto_ring_buffer_write(&bus_mgr->rx_buffer, data, len);

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
    eproto_ring_buffer_t* rx_buffer = &bus_mgr->rx_buffer;

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
            if ((actual_packet_type & EPROTO_PACKET_TYPE_USER_REPLY) != 0) {
                eproto_process_user_reply_packet(eproto, bus_mgr, &frame);
            } else if ((actual_packet_type & EPROTO_PACKET_TYPE_PROTOCOL_ACK) != 0) {
                eproto_process_protocol_ack_packet(eproto, bus_mgr, &frame);
            } else {
                eproto_process_user_send_packet(eproto, bus_mgr, &frame, is_retransmit, is_handshake);
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

#if EPROTO_ENABLE_HANDSHAKE
    if (is_handshake) {
        // 处理握手包
        EPROTO_INFO_LOG("%s: Received handshake packet\n", EPROTO_BUS_NAME(bus_mgr));

        // 清除握手标志（收到握手包清一次）
        bus_mgr->handshake_required = 0;
        EPROTO_INFO_LOG("%s: Handshake flag cleared (received handshake packet)\n", EPROTO_BUS_NAME(bus_mgr));

        // 调用状态回调告诉用户握手成功
        if (bus_mgr->bus.status_callback) {
            bus_mgr->bus.status_callback(&bus_mgr->bus, EPROTO_STATUS_HANDSHAKE_SUCCESS, NULL, 0);
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
        bool is_duplicate = false;
        for (uint8_t i = 0; i < EPROTO_MAX_CONCURRENT_SENDS; i++) {
            if (frame->packet_id == bus_mgr->last_ids[i]) {
                is_duplicate = true;
                break;
            }
        }
        if (is_duplicate) {
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
    if (bus_mgr->bus.receive_callback) {
        EPROTO_DEBUG_LOG("%s: Calling receive callback\n", EPROTO_BUS_NAME(bus_mgr));
        bus_mgr->bus.receive_callback(&bus_mgr->bus, frame->src_addr, frame->packet_id, frame->data, frame->length);
        // 循环更新上次处理的包ID数组，实现循环覆盖
        bus_mgr->last_ids[bus_mgr->last_id_index] = frame->packet_id;
        bus_mgr->last_id_index = (bus_mgr->last_id_index + 1) % EPROTO_MAX_CONCURRENT_SENDS;
    }
}

// 处理协议层应答包
// 包类型按bit定义：
//   bit0: 1=用户回复包, 0=用户发送包
//   bit1: 1=协议确认包
//   bit6: 1=握手包
//   bit7: 1=重发包
static void eproto_process_protocol_ack_packet(eproto_t* eproto, eproto_bus_manager_t* bus_mgr, eproto_frame_t* frame) {
    EPROTO_INFO_LOG("%s: Received protocol ACK\n", EPROTO_BUS_NAME(bus_mgr));

    // 遍历正在发送的节点队列，查找匹配的包ID和类型
    struct eproto_list_head* pos, *n;
    eproto_list_for_each_safe(pos, n, &bus_mgr->device_queues.sending_queue) {
        eproto_node_t* node = eproto_list_entry(pos, eproto_node_t, list);
        
        // 判断ACK的bit0(回复包标志)是否与节点的bit0匹配
        // 协议确认包bit1=1，用户发送包bit0=0，用户回复包bit0=1
        uint8_t ack_is_for_reply = frame->packet_type & EPROTO_PACKET_TYPE_REPLY_FLAG;
        uint8_t node_is_reply = node->packet_type & EPROTO_PACKET_TYPE_REPLY_FLAG;
        
        // packet_id必须匹配，且ACK类型(bit0)必须与节点类型(bit0)一致
        if (node->packet_id == frame->packet_id && ack_is_for_reply == node_is_reply) {
            EPROTO_INFO_LOG("%s: It's an ACK for current sending packet %d (type match: %s)\n", 
                           EPROTO_BUS_NAME(bus_mgr), node->packet_id,
                           ack_is_for_reply ? "REPLY" : "SEND");

            // 检查是否是握手包的协议 ACK
#if EPROTO_ENABLE_HANDSHAKE
            if (node->packet_type & EPROTO_PACKET_TYPE_HANDSHAKE_FLAG) {
                EPROTO_INFO_LOG("%s: Handshake successful\n", EPROTO_BUS_NAME(bus_mgr));
                // 清除握手标志（收到协议层应答包清一次）
                if (bus_mgr->handshake_required) {
                    bus_mgr->handshake_required = 0;
                    EPROTO_INFO_LOG("%s: Handshake flag cleared (received protocol ACK)\n", EPROTO_BUS_NAME(bus_mgr));
                }
                // 调用状态回调告诉用户握手成功
                if (bus_mgr->bus.status_callback) {
                    bus_mgr->bus.status_callback(&bus_mgr->bus, EPROTO_STATUS_HANDSHAKE_SUCCESS, NULL, 0);
                }
                // 从链表中移除节点并销毁
                eproto_list_del(&node->list);
                eproto_packet_node_destroy(eproto->user_functions.free, node);
            } else {
#endif
                // 从链表中移除节点
                eproto_list_del(&node->list);
                
                // 判断用户是否需要回复包（仅对发送包有效，回复包不需要等待回复）
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
#if EPROTO_ENABLE_HANDSHAKE
            }
#endif

            // 发送信号以便继续检查发送队列中的下一个包
            if (eproto->user_functions.signal_send) {
                eproto->user_functions.signal_send();
            }

            break;
        }
    }
}

// 处理用户回复包
// 发送协议确认包(bit1=1) + 回复包标志(bit0=1)
static void eproto_process_user_reply_packet(eproto_t* eproto, eproto_bus_manager_t* bus_mgr, eproto_frame_t* frame) {
    EPROTO_INFO_LOG("%s: Received user reply packet, sending protocol ACK\n", EPROTO_BUS_NAME(bus_mgr));
    eproto_send_response(eproto, frame->src_addr, frame->packet_id, NULL, 0, 
                        EPROTO_PACKET_TYPE_PROTOCOL_ACK | EPROTO_PACKET_TYPE_USER_REPLY);

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
            if (bus_mgr->bus.status_callback) {
                bus_mgr->bus.status_callback(&bus_mgr->bus, EPROTO_STATUS_MULTIPLE_CRC_ERRORS, NULL, 0);
            }
            bus_mgr->crc_error_count = 0;
        } else {
            // 通知用户CRC错误
            if (bus_mgr->bus.status_callback) {
                bus_mgr->bus.status_callback(&bus_mgr->bus, EPROTO_STATUS_CRC_ERROR, NULL, 0);
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
static void eproto_handle_retransmit(eproto_t* eproto, eproto_bus_manager_t* bus_mgr) {
    uint32_t current_time = eproto->user_functions.get_timestamp();

    // 遍历正在发送的节点队列
    struct eproto_list_head* pos, *n;
    eproto_list_for_each_safe(pos, n, &bus_mgr->device_queues.sending_queue) {
        eproto_node_t* node = eproto_list_entry(pos, eproto_node_t, list);

        // 检查是否需要重发
        if (current_time - node->timestamp > node->timeout_ms) {
            if (node->retry_count < node->max_retry_count) {
                // 重发
                EPROTO_INFO_LOG("%s: Retrying packet %d, retry count: %d\n", EPROTO_BUS_NAME(bus_mgr),
                                node->packet_id, node->retry_count);
                // 重发时添加重发标志
                uint8_t retransmit_packet_type =
                    node->packet_type | EPROTO_PACKET_TYPE_RETRANSMIT_FLAG;
                eproto_send_frame(eproto, bus_mgr, node->src_addr,
                                  node->dst_addr, node->packet_id,
                                  node->data, node->data_length,
                                  retransmit_packet_type);
                node->timestamp = current_time;
                node->retry_count++;
            } else {
                // 达到最大重发次数，调用回调并移除
                EPROTO_WARNING_LOG("%s: Max retry reached for packet %d\n", EPROTO_BUS_NAME(bus_mgr),
                                   node->packet_id);

                // 保存当前节点的包类型
                uint8_t packet_type = node->packet_type;

                if (node->callback) {
                    node->callback(EPROTO_SEND_TIMEOUT, node->packet_id, NULL,
                                 0, node->private_data);
                }
                
                // 从链表中移除节点
                eproto_list_del(&node->list);
                eproto_packet_node_destroy(eproto->user_functions.free, node);

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
            }
        }
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

    // 如果重发次数和超时时间都是0，不需要添加到发送节点链表
    // 因为不需要进行重发和超时检查
    if (send_node->max_retry_count > 0 || send_node->timeout_ms > 0) {
        // 添加到正在发送的节点队列
        eproto_packet_node_add(&bus_mgr->device_queues.sending_queue, send_node);
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

        // 优先处理重发
        eproto_handle_retransmit(eproto, bus_mgr);

        // 循环发送多个包，直到发送队列为空或当前发送节点数量达到最大值
        while (1) {
            // 加锁保护发送队列检查
            int queue_is_empty = 1;
            uint8_t current_send_count = 0;
            if (eproto->user_functions.lock) {
                eproto->user_functions.lock();
            }

            // 检查发送队列
            queue_is_empty = (&bus_mgr->device_queues.send_queue == bus_mgr->device_queues.send_queue.next);
            // 检查当前发送节点数量
            current_send_count = eproto_packet_node_get_length(&bus_mgr->device_queues.sending_queue);

            if (eproto->user_functions.unlock) {
                eproto->user_functions.unlock();
            }

            if (queue_is_empty)
                break;  // 发送队列为空，结束循环

            // 检查当前发送节点数量是否达到最大值
            if (current_send_count >= EPROTO_MAX_CONCURRENT_SENDS)
                break;  // 当前发送节点数量达到最大值，结束循环

                // 检查是否需要握手
#if EPROTO_ENABLE_HANDSHAKE
            if (bus_mgr->handshake_required) {
                if (eproto_send_handshake_packet(eproto, bus_mgr)) {
                    break;  // 发送握手包后结束当前循环
                }
            }
#endif

            // 发送普通数据包
            eproto_send_normal_packet(eproto, bus_mgr);
        }
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

#if EPROTO_ENABLE_HANDSHAKE
// 发送握手包
static bool eproto_send_handshake_packet(eproto_t* eproto, eproto_bus_manager_t* bus_mgr) {
    EPROTO_INFO_LOG("%s: Handshake required, sending handshake packet first\n", EPROTO_BUS_NAME(bus_mgr));

    // 检查是否有目标设备
    if (bus_mgr->destination_device_count == 0) {
        EPROTO_ERROR_LOG("%s: No destination devices, cannot send handshake packet\n", EPROTO_BUS_NAME(bus_mgr));
        return true;  // 继续处理其他总线
    }

    // 调用用户的状态回调通知正在握手
    if (bus_mgr->bus.status_callback) {
        EPROTO_INFO_LOG("%s: Calling status callback for handshake in progress\n", EPROTO_BUS_NAME(bus_mgr));
        bus_mgr->bus.status_callback(&bus_mgr->bus, EPROTO_STATUS_HANDSHAKE_IN_PROGRESS, NULL, 0);
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

        // 添加到正在发送的节点队列
        eproto_packet_node_add(&bus_mgr->device_queues.sending_queue, handshake_node);
    } else {
        // 发送失败，销毁握手节点
        EPROTO_ERROR_LOG("%s: Failed to send handshake packet\n", EPROTO_BUS_NAME(bus_mgr));
        eproto_packet_node_destroy(eproto->user_functions.free, handshake_node);
    }

    return true;  // 继续处理其他总线
}
#endif

#if EPROTO_ENABLE_HANDSHAKE
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
            
            if (bus_mgr->bus.forward_callback) {
                eproto_error_t error = bus_mgr->bus.forward_callback(
                    &bus_mgr->bus,
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
                    &bus_mgr->bus,
                    frame->src_addr, frame->dst_addr,
                    forward_data, forward_length,
                    private_data
                );
            }
        }

        // 调用当前总线的接收回调函数，通知用户收到了广播包
    if (current_bus_mgr->bus.receive_callback) {
        EPROTO_DEBUG_LOG("%s: Calling receive callback for broadcast\n", EPROTO_BUS_NAME(current_bus_mgr));
        current_bus_mgr->bus.receive_callback(&current_bus_mgr->bus, frame->src_addr, frame->packet_id, frame->data, frame->length);
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
        
        if (destination_bus_mgr->bus.forward_callback) {
                eproto_error_t error = destination_bus_mgr->bus.forward_callback(
                    &destination_bus_mgr->bus,
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
            &destination_bus_mgr->bus,
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
    bus_mgr->bus.send(&bus_mgr->bus, send_buffer, frame_length);

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


        // 检查正在发送的节点队列
        struct eproto_list_head* pos;
        eproto_list_for_each(pos, &bus_mgr->device_queues.sending_queue) {
            eproto_node_t* node = eproto_list_entry(pos, eproto_node_t, list);
            uint32_t node_timeout = node->timestamp + node->timeout_ms;
            if (node_timeout < min_timeout_timestamp) {
                min_timeout_timestamp = node_timeout;
            }
        }

        // 检查等待队列
        struct eproto_list_head* wait_pos = NULL;
        eproto_list_for_each(wait_pos, &bus_mgr->device_queues.wait_queue) {
            eproto_node_t* wait_node = eproto_list_entry(wait_pos, eproto_node_t, list);
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
