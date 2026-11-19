/*
 * mtu_manager.c - MTU 管理器实现
 * 提供实时 MTU 探测、分片、重组功能
 */

#include "mtu_manager.h"
#include <string.h>
#include <stdio.h>

// ====================================
// 日志宏（可由调用方覆盖）
// ====================================
#ifndef MTU_LOG
#define MTU_LOG(fmt, ...) do { fprintf(stderr, "[MTU] " fmt "\n", ##__VA_ARGS__); } while(0)
#endif

#ifndef MTU_WARN
#define MTU_WARN(fmt, ...) do { fprintf(stderr, "[MTU-WARN] " fmt "\n", ##__VA_ARGS__); } while(0)
#endif

#ifndef MTU_ERROR
#define MTU_ERROR(fmt, ...) do { fprintf(stderr, "[MTU-ERROR] " fmt "\n", ##__VA_ARGS__); } while(0)
#endif

// ====================================
// 内部辅助
// ====================================

static uint32_t now(mtu_manager_t* mgr) {
    if (mgr->funcs.get_timestamp) return mgr->funcs.get_timestamp();
    return 0;
}

static void* alloc(mtu_manager_t* mgr, size_t sz) {
    return mgr->funcs.malloc ? mgr->funcs.malloc(sz) : NULL;
}

static void dealloc(mtu_manager_t* mgr, void* p) {
    if (mgr->funcs.free) mgr->funcs.free(p);
}

static void lock(mtu_manager_t* mgr) {
    if (mgr->funcs.lock) mgr->funcs.lock();
}

static void unlock(mtu_manager_t* mgr) {
    if (mgr->funcs.unlock) mgr->funcs.unlock();
}

// ====================================
// 生命周期
// ====================================

void mtu_manager_init(mtu_manager_t* mgr, const mtu_user_funcs_t* funcs) {
    if (!mgr) return;
    memset(mgr, 0, sizeof(*mgr));
    if (funcs) mgr->funcs = *funcs;

    // 初始化所有 MTU 记录为无效状态
    for (int i = 0; i < MTU_MAX_RECORDS; i++) {
        mgr->records[i].valid = false;
        mgr->records[i].current_mtu = MTU_DEFAULT;
    }

    // 初始化重组上下文
    for (int i = 0; i < MTU_MAX_REASSEMBLY_CONTEXTS; i++) {
        mgr->reassembly[i].active = false;
        mgr->reassembly[i].received_mask = 0;
        for (int j = 0; j < 8; j++) {
            mgr->reassembly[i].fragments[j] = NULL;
            mgr->reassembly[i].fragment_lengths[j] = 0;
        }
    }

    mgr->next_fragment_id = 1;
    mgr->initialized = true;
}

void mtu_manager_destroy(mtu_manager_t* mgr) {
    if (!mgr) return;
    lock(mgr);

    // 释放所有重组上下文中的分片数据
    for (int i = 0; i < MTU_MAX_REASSEMBLY_CONTEXTS; i++) {
        for (int j = 0; j < 8; j++) {
            if (mgr->reassembly[i].fragments[j]) {
                dealloc(mgr, mgr->reassembly[i].fragments[j]);
                mgr->reassembly[i].fragments[j] = NULL;
            }
        }
        mgr->reassembly[i].active = false;
    }

    mgr->initialized = false;
    unlock(mgr);
}

// ====================================
// MTU 数据库
// ====================================

static mtu_record_t* find_record(mtu_manager_t* mgr, uint8_t target_addr) {
    for (int i = 0; i < MTU_MAX_RECORDS; i++) {
        if (mgr->records[i].valid && mgr->records[i].target_addr == target_addr) {
            return &mgr->records[i];
        }
    }
    return NULL;
}

static mtu_record_t* ensure_record(mtu_manager_t* mgr, uint8_t target_addr) {
    mtu_record_t* r = find_record(mgr, target_addr);
    if (r) return r;

    // 找一个空位
    for (int i = 0; i < MTU_MAX_RECORDS; i++) {
        if (!mgr->records[i].valid) {
            r = &mgr->records[i];
            memset(r, 0, sizeof(*r));
            r->target_addr = target_addr;
            r->current_mtu = MTU_DEFAULT;
            r->valid = true;
            return r;
        }
    }

    // 没有空位，复用第一条（最老的简单策略）
    MTU_WARN("MTU record table full, reusing first entry");
    memset(&mgr->records[0], 0, sizeof(mgr->records[0]));
    r = &mgr->records[0];
    r->target_addr = target_addr;
    r->current_mtu = MTU_DEFAULT;
    r->valid = true;
    return r;
}

uint16_t mtu_get(mtu_manager_t* mgr, uint8_t target_addr) {
    lock(mgr);
    mtu_record_t* r = find_record(mgr, target_addr);
    uint16_t val = r ? r->current_mtu : MTU_DEFAULT;
    unlock(mgr);
    return val;
}

void mtu_set(mtu_manager_t* mgr, uint8_t target_addr, uint16_t new_mtu) {
    if (new_mtu < MTU_MIN) new_mtu = MTU_MIN;
    if (new_mtu > MTU_MAX) new_mtu = MTU_MAX;

    lock(mgr);
    mtu_record_t* r = ensure_record(mgr, target_addr);
    r->current_mtu = new_mtu;
    MTU_LOG("Set MTU to %u for addr 0x%02X", new_mtu, target_addr);
    unlock(mgr);
}

void mtu_record_success(mtu_manager_t* mgr, uint8_t target_addr) {
    lock(mgr);
    mtu_record_t* r = ensure_record(mgr, target_addr);
    r->consecutive_timeout = 0;
    r->consecutive_success++;

    // 连续 5 次成功，尝试上调 MTU
    if (r->consecutive_success >= 5 && !r->probe_active) {
        uint16_t new_mtu = r->current_mtu + (r->current_mtu / 10);  // +10%
        if (new_mtu > r->current_mtu && new_mtu <= MTU_MAX) {
            // 触发一次探测确认能否提高
            r->probe_active = true;
            r->probe_mtu = new_mtu;
            r->probe_retry_count = 0;
            r->last_probe_time = now(mgr);
            MTU_LOG("Auto probe up to %u for 0x%02X", new_mtu, target_addr);
        }
        r->consecutive_success = 0;
    }
    unlock(mgr);
}

void mtu_record_timeout(mtu_manager_t* mgr, uint8_t target_addr) {
    lock(mgr);
    mtu_record_t* r = ensure_record(mgr, target_addr);
    r->consecutive_success = 0;
    r->consecutive_timeout++;

    // 连续 3 次超时，MTU 减半
    if (r->consecutive_timeout >= 3 && r->current_mtu > MTU_MIN) {
        r->current_mtu = (uint16_t)(r->current_mtu / 2);
        if (r->current_mtu < MTU_MIN) r->current_mtu = MTU_MIN;
        MTU_WARN("Auto-lowered MTU to %u for 0x%02X after timeouts", r->current_mtu, target_addr);
        r->consecutive_timeout = 0;
    }
    unlock(mgr);
}

// ====================================
// PMTUD 探测
// ====================================

bool mtu_build_probe_request(mtu_manager_t* mgr, uint8_t target_addr,
                              uint16_t probe_mtu, uint8_t** out_data, uint16_t* out_length) {
    if (!out_data || !out_length) return false;

    uint16_t current = mtu_get(mgr, target_addr);
    if (probe_mtu < MTU_MIN) probe_mtu = MTU_MIN;
    if (probe_mtu > MTU_MAX) probe_mtu = MTU_MAX;

    // data = probe_header + 填充数据 (总长度 = probe_mtu)
    size_t hdr_size = sizeof(mtu_probe_header_t);
    if (probe_mtu < hdr_size + 4) {
        *out_length = (uint16_t)hdr_size;
    } else {
        *out_length = probe_mtu;
    }

    uint8_t* buf = (uint8_t*)alloc(mgr, *out_length);
    if (!buf) return false;

    mtu_probe_header_t* hdr = (mtu_probe_header_t*)buf;
    hdr->probe_mtu   = probe_mtu;
    hdr->current_mtu = current;
    hdr->probe_type  = MTU_PROBE_REQUEST;
    hdr->reserved    = 0;

    // 填充剩余部分为随机/递增模式，方便探测
    for (size_t i = hdr_size; i < *out_length; i++) {
        buf[i] = (uint8_t)(i & 0xFF);
    }

    *out_data = buf;

    // 更新数据库中的探测状态
    lock(mgr);
    mtu_record_t* r = ensure_record(mgr, target_addr);
    r->probe_active = true;
    r->probe_mtu = probe_mtu;
    r->probe_retry_count = 0;
    r->last_probe_time = now(mgr);
    unlock(mgr);

    MTU_LOG("Probe REQUEST MTU=%u -> 0x%02X", probe_mtu, target_addr);
    return true;
}

bool mtu_build_probe_reply(mtu_manager_t* mgr, uint8_t target_addr,
                            mtu_probe_type_t reply_type,
                            uint16_t report_mtu,
                            uint8_t** out_data, uint16_t* out_length) {
    if (!out_data || !out_length) return false;

    size_t hdr_size = sizeof(mtu_probe_header_t);
    uint8_t* buf = (uint8_t*)alloc(mgr, hdr_size);
    if (!buf) return false;

    uint16_t current = mtu_get(mgr, target_addr);
    mtu_probe_header_t* hdr = (mtu_probe_header_t*)buf;
    hdr->probe_mtu   = report_mtu;
    hdr->current_mtu = current;
    hdr->probe_type  = (uint8_t)reply_type;
    hdr->reserved    = 0;

    *out_data = buf;
    *out_length = (uint16_t)hdr_size;
    return true;
}

void mtu_handle_probe_response(mtu_manager_t* mgr, uint8_t source_addr,
                               const mtu_probe_header_t* header) {
    if (!header) return;
    MTU_LOG("Probe RESPONSE from 0x%02X: type=%u, report=%u, peer_current=%u",
            source_addr, header->probe_type, header->probe_mtu, header->current_mtu);

    lock(mgr);
    mtu_record_t* r = ensure_record(mgr, source_addr);

    if (header->probe_type == MTU_PROBE_TOO_BIG) {
        // 对方说 MTU 太大，更新为对方报告的最大值（取较小值）
        uint16_t new_mtu = (header->probe_mtu < r->current_mtu) ? header->probe_mtu : r->current_mtu;
        if (new_mtu < MTU_MIN) new_mtu = MTU_MIN;
        r->current_mtu = new_mtu;
        MTU_LOG("PMTUD down-adjust MTU to %u (TOO_BIG)", new_mtu);
    } else if (header->probe_type == MTU_PROBE_REPLY) {
        // 探测成功，协商为双方较小值
        uint16_t peer_max = header->probe_mtu;
        uint16_t new_mtu = (peer_max < r->probe_mtu) ? peer_max : r->probe_mtu;
        if (new_mtu < MTU_MIN) new_mtu = MTU_MIN;
        if (new_mtu > MTU_MAX) new_mtu = MTU_MAX;

        // 只有当新 MTU > 当前 MTU 时才更新（探测提升时）
        // 或者比当前小（协商确认）
        r->current_mtu = new_mtu;
        MTU_LOG("PMTUD agreed MTU=%u with 0x%02X", new_mtu, source_addr);
    }

    r->probe_active = false;
    r->probe_retry_count = 0;
    r->consecutive_success = 0;
    r->consecutive_timeout = 0;
    unlock(mgr);
}

void mtu_probe_tick(mtu_manager_t* mgr) {
    if (!mgr) return;

    uint32_t t = now(mgr);
    lock(mgr);

    for (int i = 0; i < MTU_MAX_RECORDS; i++) {
        mtu_record_t* r = &mgr->records[i];
        if (!r->valid) continue;

        // 探测超时：重试
        if (r->probe_active) {
            if (t - r->last_probe_time > MTU_REASSEMBLY_TIMEOUT_MS) {
                r->probe_retry_count++;
                if (r->probe_retry_count >= MTU_PROBE_RETRY) {
                    MTU_WARN("PMTUD probe failed for 0x%02X after %d retries, keep MTU=%u",
                             r->target_addr, MTU_PROBE_RETRY, r->current_mtu);
                    r->probe_active = false;
                } else {
                    r->last_probe_time = t;
                    MTU_LOG("PMTUD probe retry %d/%d for 0x%02X",
                            r->probe_retry_count, MTU_PROBE_RETRY, r->target_addr);
                }
            }
            continue;
        }

        // 周期性主动探测
        if (t - r->last_probe_time > MTU_PROBE_INTERVAL_MS) {
            r->probe_active = true;
            r->probe_mtu = r->current_mtu;  // 先探测当前值能否通过
            r->probe_retry_count = 0;
            r->last_probe_time = t;
            MTU_LOG("Auto PMTUD probe for 0x%02X", r->target_addr);
            // 注意：这里只设置状态，真正发送探测包由调用方在 eproto_process 中检查
        }
    }

    unlock(mgr);
}

// ====================================
// 分片
// ====================================

uint8_t mtu_fragment_split(mtu_manager_t* mgr, const uint8_t* data, uint16_t data_len,
                            uint16_t mtu, mtu_fragment_t* fragments) {
    if (!data || data_len == 0 || !fragments) return 0;
    if (mtu <= sizeof(mtu_fragment_header_t) + 1) return 0;

    uint16_t max_user = mtu - (uint16_t)sizeof(mtu_fragment_header_t);
    uint8_t total = (uint8_t)((data_len + max_user - 1) / max_user);
    if (total == 1) {
        fragments[0].fragment_id = 0;  // 不分片，id 不重要
        fragments[0].fragment_index = 0;
        fragments[0].total_fragments = 1;
        fragments[0].data = (uint8_t*)data;
        fragments[0].length = data_len;
        return 1;
    }

    uint16_t frag_id;
    lock(mgr);
    frag_id = mgr->next_fragment_id++;
    if (mgr->next_fragment_id == 0) mgr->next_fragment_id = 1;
    unlock(mgr);

    uint16_t offset = 0;
    for (uint8_t i = 0; i < total; i++) {
        uint16_t chunk = (i == total - 1) ? (data_len - offset) : max_user;
        fragments[i].fragment_id = frag_id;
        fragments[i].fragment_index = i;
        fragments[i].total_fragments = total;
        fragments[i].data = (uint8_t*)(data + offset);
        fragments[i].length = chunk;
        offset += chunk;
    }

    MTU_LOG("Split %u bytes into %d fragments (MTU=%u)", data_len, total, mtu);
    return total;
}

bool mtu_fragment_build_frame(mtu_manager_t* mgr, const mtu_fragment_t* frag,
                               uint8_t** out_data, uint16_t* out_len) {
    if (!frag || !out_data || !out_len) return false;

    uint16_t hdr_size = (uint16_t)sizeof(mtu_fragment_header_t);
    uint16_t total_size = hdr_size + frag->length;

    uint8_t* buf = (uint8_t*)alloc(mgr, total_size);
    if (!buf) return false;

    mtu_fragment_header_t* hdr = (mtu_fragment_header_t*)buf;
    hdr->fragment_id      = frag->fragment_id;
    hdr->fragment_index   = frag->fragment_index;
    hdr->total_fragments  = frag->total_fragments;

    if (frag->length > 0) {
        memcpy(buf + hdr_size, frag->data, frag->length);
    }

    *out_data = buf;
    *out_len = total_size;
    return true;
}

// ====================================
// 重组
// ====================================

static mtu_reassembly_ctx_t* find_or_create_ctx(mtu_manager_t* mgr, uint16_t fragment_id, uint8_t source_addr) {
    // 先找已存在的上下文
    for (int i = 0; i < MTU_MAX_REASSEMBLY_CONTEXTS; i++) {
        mtu_reassembly_ctx_t* c = &mgr->reassembly[i];
        if (c->active && c->fragment_id == fragment_id && c->source_addr == source_addr) {
            return c;
        }
    }

    // 找一个空槽位
    for (int i = 0; i < MTU_MAX_REASSEMBLY_CONTEXTS; i++) {
        if (!mgr->reassembly[i].active) {
            return &mgr->reassembly[i];
        }
    }

    // 清理最老的（简化策略：释放第一个并复用）
    MTU_WARN("Reassembly table full, evicting oldest");
    mtu_reassembly_ctx_t* oldest = &mgr->reassembly[0];
    for (int j = 0; j < 8; j++) {
        if (oldest->fragments[j]) {
            dealloc(mgr, oldest->fragments[j]);
            oldest->fragments[j] = NULL;
        }
    }
    return oldest;
}

int mtu_fragment_reassemble(mtu_manager_t* mgr, uint8_t source_addr,
                             const uint8_t* frame_data, uint16_t frame_len,
                             uint8_t** out_full_data, uint16_t* out_full_len) {
    if (!frame_data || frame_len < sizeof(mtu_fragment_header_t)) return -1;
    if (!out_full_data || !out_full_len) return -1;

    const mtu_fragment_header_t* hdr = (const mtu_fragment_header_t*)frame_data;

    // 不支持超过 8 片的重组上下文位图（可扩展）
    if (hdr->total_fragments > 8) {
        MTU_ERROR("Reassembly: too many fragments %d (>8)", hdr->total_fragments);
        return -1;
    }

    uint16_t user_data_len = frame_len - (uint16_t)sizeof(mtu_fragment_header_t);
    const uint8_t* user_data = frame_data + sizeof(mtu_fragment_header_t);

    lock(mgr);

    mtu_reassembly_ctx_t* ctx = find_or_create_ctx(mgr, hdr->fragment_id, source_addr);

    // 重置上下文（如果之前被复用过）
    if (!ctx->active || ctx->fragment_id != hdr->fragment_id || ctx->source_addr != source_addr) {
        memset(ctx, 0, sizeof(*ctx));
        ctx->fragment_id = hdr->fragment_id;
        ctx->source_addr = source_addr;
        ctx->total_fragments = hdr->total_fragments;
        ctx->received_mask = 0;
        ctx->timestamp = now(mgr);
        ctx->active = true;
    }

    // 存储分片数据
    if (hdr->fragment_index >= hdr->total_fragments) {
        unlock(mgr);
        MTU_ERROR("Reassembly: fragment index %d >= total %d", hdr->fragment_index, hdr->total_fragments);
        return -1;
    }

    uint8_t idx = hdr->fragment_index;

    // 如果已有相同分片，跳过（重传）
    if (ctx->received_mask & (1 << idx)) {
        MTU_LOG("Reassembly: duplicate fragment %d, skip", idx);
        unlock(mgr);
        return 0;
    }

    // 分配并存储
    ctx->fragments[idx] = (uint8_t*)alloc(mgr, user_data_len);
    if (!ctx->fragments[idx]) {
        unlock(mgr);
        MTU_ERROR("Reassembly: malloc failed");
        return -1;
    }
    memcpy(ctx->fragments[idx], user_data, user_data_len);
    ctx->fragment_lengths[idx] = user_data_len;
    ctx->received_mask |= (1 << idx);

    MTU_LOG("Reassembly: got fragment %d/%d (id=%u), mask=0x%02X",
            idx + 1, hdr->total_fragments, hdr->fragment_id, ctx->received_mask);

    // 检查是否所有分片到齐
    uint8_t expected_mask = (uint8_t)((1 << ctx->total_fragments) - 1);
    if (ctx->received_mask == expected_mask) {
        // 重组完成
        uint16_t total_len = 0;
        for (int i = 0; i < ctx->total_fragments; i++) {
            total_len += ctx->fragment_lengths[i];
        }

        uint8_t* full = (uint8_t*)alloc(mgr, total_len);
        if (!full) {
            unlock(mgr);
            MTU_ERROR("Reassembly: final malloc failed");
            return -1;
        }

        uint16_t offset = 0;
        for (int i = 0; i < ctx->total_fragments; i++) {
            memcpy(full + offset, ctx->fragments[i], ctx->fragment_lengths[i]);
            offset += ctx->fragment_lengths[i];
            dealloc(mgr, ctx->fragments[i]);
            ctx->fragments[i] = NULL;
        }

        ctx->active = false;

        *out_full_data = full;
        *out_full_len = total_len;

        MTU_LOG("Reassembly complete: %u bytes from %d fragments", total_len, ctx->total_fragments);
        unlock(mgr);
        return 1;  // 重组成功
    }

    unlock(mgr);
    return 0;  // 尚未收齐
}

void mtu_reassembly_tick(mtu_manager_t* mgr) {
    if (!mgr) return;

    uint32_t t = now(mgr);
    lock(mgr);

    for (int i = 0; i < MTU_MAX_REASSEMBLY_CONTEXTS; i++) {
        mtu_reassembly_ctx_t* c = &mgr->reassembly[i];
        if (!c->active) continue;

        if (t - c->timestamp > MTU_REASSEMBLY_TIMEOUT_MS) {
            MTU_WARN("Reassembly timeout: id=%u from 0x%02X, mask=0x%02X, expected=%d",
                     c->fragment_id, c->source_addr, c->received_mask, c->total_fragments);

            // 释放所有已接收分片
            for (int j = 0; j < 8; j++) {
                if (c->fragments[j]) {
                    dealloc(mgr, c->fragments[j]);
                    c->fragments[j] = NULL;
                }
            }
            c->active = false;
            c->received_mask = 0;
        }
    }

    unlock(mgr);
}
