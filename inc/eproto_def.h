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

#ifndef EPROTO_DEF_H
#define EPROTO_DEF_H

// eProto - 嵌入式协议（Embedded Protocol）
// 内部定义，通常不需要用户修改

#include "eproto_config.h"

// 最大并发发送包数量，默认值为 1
#ifndef EPROTO_MAX_CONCURRENT_SENDS
#define EPROTO_MAX_CONCURRENT_SENDS 1
#endif

// 握手功能配置检查
#ifndef EPROTO_ENABLE_HANDSHAKE
#define EPROTO_ENABLE_HANDSHAKE 1  // 默认启用握手功能
#endif

// 帧头长度（除数据部分外）：头(1) + 版本(1) + 长度(2) + 包类型(1) + 源地址(1) +
// 设备地址(1) + 包ID(2) + CRC(2)
#define EPROTO_FRAME_HEADER_LENGTH 11

// CRC校验类型
#define EPROTO_CRC_TYPE CRC_16_CCITT

// 协议版本号
#define EPROTO_PROTOCOL_VERSION 0x01

// 帧头
#define EPROTO_FRAME_HEADER 0xAA

// 广播地址
#define EPROTO_BROADCAST_ADDRESS 0xFF

// 日志打印接口，由用户实现
#ifndef EPROTO_DEBUG_LOG
#define EPROTO_DEBUG_LOG(fmt, ...) \
    do {                           \
    } while (0)
#endif

#ifndef EPROTO_INFO_LOG
#define EPROTO_INFO_LOG(fmt, ...) \
    do {                          \
    } while (0)
#endif

#ifndef EPROTO_WARNING_LOG
#define EPROTO_WARNING_LOG(fmt, ...) \
    do {                             \
    } while (0)
#endif

#ifndef EPROTO_ERROR_LOG
#define EPROTO_ERROR_LOG(fmt, ...) \
    do {                           \
    } while (0)
#endif

// 宏定义：获取总线名称，为空时返回"Unknown"
#define EPROTO_BUS_NAME(bus_mgr) ((bus_mgr)->bus.name ? (bus_mgr)->bus.name : "Unknown")

// 固定块内存分配器默认配置
#ifndef CONFIG_EPROTO_FIXED_BLOCK_POOLS
#define CONFIG_EPROTO_FIXED_BLOCK_POOLS X(16, 1)
#endif

#endif  // EPROTO_DEF_H
