#ifndef EPROTO_DEF_H
#define EPROTO_DEF_H

// eProto - 嵌入式协议（Embedded Protocol）
// 内部定义，通常不需要用户修改

#include "eproto_config.h"

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
#define EPROTO_BUS_NAME(bus_mgr) ((bus_mgr)->name ? (bus_mgr)->name : "Unknown")

// 固定块内存分配器默认配置
#ifndef CONFIG_FIXED_BLOCK_POOLS
#define CONFIG_FIXED_BLOCK_POOLS \
    X(16, 1)
#endif

#endif  // EPROTO_DEF_H
