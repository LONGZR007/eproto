/*
 * mtu_manager.h - MTU 管理器头文件
 * 提供实时 MTU 探测、分片、重组、转发的完整接口
 *
 * 可直接集成到 eProto 协议栈，通过 EPROTO_MTU_ENABLE 宏开关启用
 */

#ifndef MTU_MANAGER_H
#define MTU_MANAGER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ====================================
// 配置宏（可在 eproto_config.h 中覆盖）
// ====================================
#ifndef MTU_DEFAULT
#define MTU_DEFAULT              1280
#endif

#ifndef MTU_MIN
#define MTU_MIN                  64
#endif

#ifndef MTU_MAX
#define MTU_MAX                  65000
#endif

#ifndef MTU_PROBE_INTERVAL_MS
#define MTU_PROBE_INTERVAL_MS    10000
#endif

#ifndef MTU_REASSEMBLY_TIMEOUT_MS
#define MTU_REASSEMBLY_TIMEOUT_MS 5000
#endif

#ifndef MTU_MAX_RECORDS
#define MTU_MAX_RECORDS          32
#endif

#ifndef MTU_MAX_REASSEMBLY_CONTEXTS
#define MTU_MAX_REASSEMBLY_CONTEXTS 8
#endif

#ifndef MTU_PROBE_RETRY
#define MTU_PROBE_RETRY          3
#endif

// ====================================
// 扩展包类型标志（利用 bit2/bit3/bit4）
// ====================================
#define MTU_PACKET_TYPE_CONTROL      0x04  // bit2: MTU 控制包
#define MTU_PACKET_TYPE_FRAGMENT     0x08  // bit3: 分片数据包
#define MTU_PACKET_TYPE_FRAG_LAST    0x10  // bit4: 分片最后一包

// ====================================
// 数据结构
// ====================================

// MTU 探测类型
typedef enum {
    MTU_PROBE_REQUEST = 0,  // 探测请求
    MTU_PROBE_REPLY   = 1,  // 探测响应
    MTU_PROBE_TOO_BIG = 2   // MTU 太大，返回最大可接收值
} mtu_probe_type_t;

// PMTUD 探测包头（放在 MTU 控制包的 data 中）
typedef struct {
    uint16_t probe_mtu;     // 探测使用的 MTU 值
    uint16_t current_mtu;   // 探测方当前 MTU
    uint8_t  probe_type;    // mtu_probe_type_t
    uint8_t  reserved;
} __attribute__((packed)) mtu_probe_header_t;

// 分片头（放在分片数据包的 data 最前面）
typedef struct {
    uint16_t fragment_id;       // 同一个大包的唯一标识
    uint8_t  fragment_index;    // 当前分片序号 (0 ~ total-1)
    uint8_t  total_fragments;   // 总分片数
} __attribute__((packed)) mtu_fragment_header_t;

// MTU 数据库记录（每个目标地址一条）
typedef struct {
    uint8_t  target_addr;       // 目标设备地址
    uint16_t current_mtu;       // 当前协商后的 MTU
    uint16_t probe_mtu;         // 当前正在探测的 MTU（二分查找用）
    uint32_t last_probe_time;   // 上次探测时间戳
    uint8_t  probe_retry_count; // 当前探测重试次数
    bool     probe_active;      // 是否正在探测中
    uint8_t  consecutive_success;// 连续成功计数
    uint8_t  consecutive_timeout;// 连续超时计数
    bool     valid;             // 记录是否有效
} mtu_record_t;

// 重组上下文（同一个大包的所有分片）
typedef struct {
    uint16_t fragment_id;               // 分片 ID
    uint8_t  source_addr;               // 源地址
    uint8_t  total_fragments;           // 预期总分片数
    uint8_t  received_mask;             // 已接收分片位图（支持最多 8 分片）
    uint8_t* fragments[8];              // 各分片数据指针
    uint16_t fragment_lengths[8];       // 各分片数据长度
    uint32_t timestamp;                 // 重组开始时间戳
    bool     active;                    // 是否正在等待重组
} mtu_reassembly_ctx_t;

// 分片描述（用于输出）
typedef struct {
    uint16_t fragment_id;       // 分片 ID
    uint8_t  fragment_index;    // 分片序号
    uint8_t  total_fragments;   // 总分片数
    uint8_t* data;              // 分片数据（用户数据，不含分片头）
    uint16_t length;            // 分片数据长度
} mtu_fragment_t;

// 内存分配接口（由调用方提供，类似 eProto）
typedef struct {
    void* (*malloc)(size_t size);
    void  (*free)(void* ptr);
    uint32_t (*get_timestamp)(void);
    void  (*lock)(void);
    void  (*unlock)(void);
} mtu_user_funcs_t;

// MTU 管理器主结构体
typedef struct {
    mtu_user_funcs_t funcs;
    mtu_record_t records[MTU_MAX_RECORDS];
    mtu_reassembly_ctx_t reassembly[MTU_MAX_REASSEMBLY_CONTEXTS];
    uint16_t next_fragment_id;
    bool initialized;
} mtu_manager_t;

// ====================================
// 生命周期
// ====================================

/**
 * 初始化 MTU 管理器
 */
void mtu_manager_init(mtu_manager_t* mgr, const mtu_user_funcs_t* funcs);

/**
 * 销毁 MTU 管理器（清理所有重组上下文）
 */
void mtu_manager_destroy(mtu_manager_t* mgr);

// ====================================
// MTU 数据库操作
// ====================================

/**
 * 获取目标地址的当前 MTU
 * @return 当前 MTU，未找到返回 MTU_DEFAULT
 */
uint16_t mtu_get(mtu_manager_t* mgr, uint8_t target_addr);

/**
 * 设置目标地址的 MTU（外部协商成功后更新）
 */
void mtu_set(mtu_manager_t* mgr, uint8_t target_addr, uint16_t new_mtu);

/**
 * 记录一次成功发送（内部用于自动调优 MTU）
 */
void mtu_record_success(mtu_manager_t* mgr, uint8_t target_addr);

/**
 * 记录一次超时（内部用于自动降低 MTU）
 */
void mtu_record_timeout(mtu_manager_t* mgr, uint8_t target_addr);

// ====================================
// PMTUD 探测
// ====================================

/**
 * 构造 PMTUD 探测请求包（data 填充到 probe_mtu 大小）
 * @param mgr        MTU 管理器
 * @param target_addr 目标地址
 * @param probe_mtu  本次探测的 MTU 值
 * @param out_data   输出：应发送的 data 指针（调用方负责释放）
 * @param out_length 输出：data 长度
 * @return true=构造成功
 */
bool mtu_build_probe_request(mtu_manager_t* mgr, uint8_t target_addr,
                              uint16_t probe_mtu, uint8_t** out_data, uint16_t* out_length);

/**
 * 处理收到的 PMTUD 响应包
 * @param mgr           MTU 管理器
 * @param source_addr   发送方地址
 * @param header        探测包头
 */
void mtu_handle_probe_response(mtu_manager_t* mgr, uint8_t source_addr,
                               const mtu_probe_header_t* header);

/**
 * 周期 PMTUD 检查（在 eproto_process 中调用）
 * 检查是否需要发起新的探测、处理超时探测
 */
void mtu_probe_tick(mtu_manager_t* mgr);

/**
 * 构造 PMTUD 回复包（接收方用，对探测请求的响应）
 */
bool mtu_build_probe_reply(mtu_manager_t* mgr, uint8_t target_addr,
                            mtu_probe_type_t reply_type,
                            uint16_t report_mtu,
                            uint8_t** out_data, uint16_t* out_length);

// ====================================
// 分片与重组
// ====================================

/**
 * 判断是否需要分片
 */
static inline bool mtu_need_fragment(uint16_t data_len, uint16_t mtu) {
    // MTU 包含分片头(4)，所以用户数据最多 mtu - 4
    return data_len > (mtu - sizeof(mtu_fragment_header_t));
}

/**
 * 计算分片数量
 */
static inline uint8_t mtu_calc_fragment_count(uint16_t data_len, uint16_t mtu) {
    uint16_t max_user_per_pkt = mtu - sizeof(mtu_fragment_header_t);
    if (max_user_per_pkt == 0) return 0;
    return (uint8_t)((data_len + max_user_per_pkt - 1) / max_user_per_pkt);
}

/**
 * 执行分片，生成分片数组
 * @param mgr        MTU 管理器
 * @param data       原始数据
 * @param data_len   原始数据长度
 * @param mtu        目标 MTU
 * @param fragments  输出分片数组（由调用方分配，大小至少 mtu_calc_fragment_count 个）
 * @return 分片数量，0 表示不分片
 */
uint8_t mtu_fragment_split(mtu_manager_t* mgr, const uint8_t* data, uint16_t data_len,
                            uint16_t mtu, mtu_fragment_t* fragments);

/**
 * 为分片构造完整帧的 data 字段（含分片头）
 * @param mgr       MTU 管理器
 * @param frag      分片描述
 * @param out_data  输出完整 data（含分片头，调用方负责释放）
 * @param out_len   输出 data 总长度
 */
bool mtu_fragment_build_frame(mtu_manager_t* mgr, const mtu_fragment_t* frag,
                               uint8_t** out_data, uint16_t* out_len);

/**
 * 处理收到的分片包，尝试重组
 * @param mgr           MTU 管理器
 * @param source_addr   源地址
 * @param frame_data    帧的 data 字段（含分片头）
 * @param frame_len     data 长度
 * @param out_full_data 输出：重组后的完整数据（成功时返回，调用方负责释放）
 * @param out_full_len  输出：完整数据长度
 * @return  0=尚未收齐，等待中
 *          1=重组成功
 *         -1=重组失败（上下文溢出等）
 */
int mtu_fragment_reassemble(mtu_manager_t* mgr, uint8_t source_addr,
                             const uint8_t* frame_data, uint16_t frame_len,
                             uint8_t** out_full_data, uint16_t* out_full_len);

/**
 * 重组超时检查（在 eproto_process 中调用）
 */
void mtu_reassembly_tick(mtu_manager_t* mgr);

// ====================================
// 辅助：从帧中提取分片信息
// ====================================

static inline const mtu_fragment_header_t* mtu_get_fragment_header(const uint8_t* frame_data) {
    return (const mtu_fragment_header_t*)frame_data;
}

static inline const mtu_probe_header_t* mtu_get_probe_header(const uint8_t* frame_data) {
    return (const mtu_probe_header_t*)frame_data;
}

#ifdef __cplusplus
}
#endif

#endif /* MTU_MANAGER_H */
