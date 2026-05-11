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

#ifndef EPROTO_CONFIG_H
#define EPROTO_CONFIG_H

// eProto - 嵌入式协议（Embedded Protocol）
// "e"代表嵌入式（Embedded），"Proto"代表协议（Protocol）

// 协议配置参数

// 最大数据包长度
#define EPROTO_MAX_PACKET_LENGTH 256

// 重发配置
#define EPROTO_DEFAULT_MAX_RETRY_COUNT 3     // 默认最大重发次数
#define EPROTO_DEFAULT_RETRY_TIMEOUT_MS 100  // 默认超时时间

// 握手配置
#define EPROTO_HANDSHAKE_MAX_RETRY_COUNT 3  // 握手最大重发次数
#define EPROTO_HANDSHAKE_TIMEOUT_MS 1000      // 握手超时时间（毫秒）

// 环形缓冲区大小 - 由用户在初始化时提供

// 发送队列最大长度
#define EPROTO_MAX_SEND_QUEUE_SIZE 16

// 等待应答队列最大长度
#define EPROTO_MAX_WAIT_QUEUE_SIZE 16

// 总线数量
#define EPROTO_MAX_BUS_COUNT 16

// 单个总线下最多支持的目标设备数量
#define EPROTO_MAX_DESTINATION_DEVICES 16

// 禁用握手功能
#define EPROTO_ENABLE_HANDSHAKE 0

// 超时时间计算系数
#define EPROTO_TIMEOUT_FACTOR 1.5

// 日志等级定义
#define EPROTO_LOG_LEVEL_DEBUG 0    // 调试级别，高频率打印，打印收发的二进制数据
#define EPROTO_LOG_LEVEL_INFO 1      // 信息级别，重要信息打印
#define EPROTO_LOG_LEVEL_WARNING 2   // 警告级别，打印警告信息
#define EPROTO_LOG_LEVEL_ERROR 3     // 错误级别，打印错误信息

// 日志打印接口，由用户实现
#ifndef EPROTO_DEBUG_LOG
#define EPROTO_DEBUG_LOG(fmt, ...) printf(fmt, ##__VA_ARGS__)
#endif

#ifndef EPROTO_INFO_LOG
#define EPROTO_INFO_LOG(fmt, ...) printf(fmt, ##__VA_ARGS__)
#endif

#ifndef EPROTO_WARNING_LOG
#define EPROTO_WARNING_LOG(fmt, ...) printf(fmt, ##__VA_ARGS__)
#endif

#ifndef EPROTO_ERROR_LOG
#define EPROTO_ERROR_LOG(fmt, ...) printf(fmt, ##__VA_ARGS__)
#endif

#endif  // EPROTO_CONFIG_H
