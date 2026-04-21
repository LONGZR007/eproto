# eProto 转发加密/解密回调机制实现方案

## 1. 问题背景

在多设备通信场景中，不同设备间可能使用不同的加密密钥。当设备 A 通过设备 B 转发数据到设备 C 时，由于 A 和 B、B 和 C 之间的密钥不同，直接转发加密数据会导致 C 无法解析。因此，需要在转发过程中添加回调机制，允许用户在转发时进行解密和重新加密操作。

## 2. 解决方案

在 eProto 库中添加转发回调机制，允许用户在数据转发过程中对数据进行自定义处理（如解密和重新加密），并提供后处理回调用于内存管理。

## 3. 实现步骤

### 3.1 修改头文件 (`inc/eproto.h`)

#### 3.1.1 添加新的回调类型定义
```c
// 转发后处理回调函数类型
typedef void (*eproto_forward_post_func_t)(uint8_t source_addr, uint8_t dest_addr, 
                                         uint8_t* out_data, uint16_t out_length,
                                         void* private_data);

// 转发回调函数类型
typedef eproto_error_t (*eproto_forward_callback_t)(uint8_t source_addr, uint8_t dest_addr, 
                                                   uint8_t* data, uint16_t length, 
                                                   uint8_t** out_data, uint16_t* out_length,
                                                   eproto_forward_post_func_t* post_func,
                                                   void** private_data);
```

#### 3.1.2 更新总线管理结构体
```c
// 总线管理结构体
typedef struct {
    eproto_bus_t bus;                // 总线接口（实体）
    const char* name;                // 总线名称，用于日志和调试

    // 帧解析器
    eproto_frame_parser_t parser;
    // 接口函数
    eproto_status_callback_t status_callback;
    receive_callback_t receive_callback;
    eproto_forward_callback_t forward_callback;  // 新增：转发回调
    // 状态变量
    uint16_t next_packet_id;
    uint16_t last_id;  // 上次处理的包ID，用于重发包检测
    uint8_t crc_error_count;
    eproto_node_t* current_send_node;  // 当前正在发送的节点
#ifdef EPROTO_ENABLE_HANDSHAKE
    // 握手相关
    uint8_t handshake_required;  // 握手标志
#endif
    // 设备队列
    eproto_device_queues_t device_queues;
    // 目标设备地址数组
    uint8_t destination_devices[EPROTO_MAX_DESTINATION_DEVICES];
    // 目标设备地址数量
    uint8_t destination_device_count;
} eproto_bus_manager_t;
```

#### 3.1.3 更新 eproto_add_bus 函数声明
```c
/**
 * 向eProto实例添加总线
 * @param eproto            指向eProto实例的指针
 * @param self_addr         总线的自身地址
 * @param send_func         发送函数
 * @param rx_buffer         接收缓冲区
 * @param rx_buffer_size    接收缓冲区大小
 * @param name              总线名称，用于日志和调试
 * @param status_callback   状态回调函数
 * @param receive_callback   接收回调函数
 * @param forward_callback  转发回调函数
 * @return                  操作结果，EPROTO_OK表示成功，其他值表示错误
 */
eproto_error_t eproto_add_bus(eproto_t* eproto, uint8_t self_addr, eproto_bus_send_func_t send_func, uint8_t* rx_buffer,
                              uint16_t rx_buffer_size, const char* name,
                              eproto_status_callback_t status_callback, receive_callback_t receive_callback,
                              eproto_forward_callback_t forward_callback);
```

### 3.2 修改源文件 (`src/eproto.c`)

#### 3.2.1 更新 eproto_init 函数
```c
eproto_error_t eproto_init(eproto_t* eproto, eproto_user_functions_t* user_functions) {
    // ... 现有代码 ...
    
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
        eproto->bus_managers[i].current_send_node = NULL;
#ifdef EPROTO_ENABLE_HANDSHAKE
        eproto->bus_managers[i].handshake_required = 1;
#endif
        // 初始化目标设备地址数组
        eproto->bus_managers[i].destination_device_count = 0;

        // 初始化设备队列
        EPROTO_INIT_LIST_HEAD(&eproto->bus_managers[i].device_queues.send_queue);
        EPROTO_INIT_LIST_HEAD(&eproto->bus_managers[i].device_queues.wait_queue);
    }
    
    // ... 现有代码 ...
}
```

#### 3.2.2 更新 eproto_add_bus 函数
```c
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
```

#### 3.2.3 修改 eproto_forward_frame 函数
```c
static void eproto_forward_frame(eproto_t* eproto, eproto_bus_manager_t* current_bus_mgr,
                                eproto_frame_t* frame) {
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
                    frame->src_addr, frame->dst_addr,
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
                if (eproto->user_functions.lock)
                    eproto->user_functions.lock();

                // 将节点添加到发送队列
                list_add_tail(&forward_node->list, &bus_mgr->device_queues.send_queue);

                // 解锁
                if (eproto->user_functions.unlock)
                    eproto->user_functions.unlock();

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
    } else {
        // 单播包转发逻辑
        eproto_bus_manager_t* destination_bus_mgr = eproto_find_bus_by_destination(eproto, frame->dst_addr);
        if (destination_bus_mgr) {
            // 检查是否设置了转发回调
            uint8_t* forward_data = frame->data;
            uint16_t forward_length = frame->length;
            uint8_t* temp_data = NULL;
            void* private_data = NULL;  // 新增：局部变量存储私有数据
            eproto_forward_post_func_t post_func = NULL;
            
            if (destination_bus_mgr->forward_callback) {
                eproto_error_t error = destination_bus_mgr->forward_callback(
                    frame->src_addr, frame->dst_addr,
                    frame->data, frame->length,
                    &temp_data, &forward_length,
                    &post_func,
                    &private_data
                );
                
                if (error == EPROTO_OK && temp_data) {
                    forward_data = temp_data;
                }
            }

            // 创建新的数据包节点
            eproto_node_t* forward_node = eproto_packet_node_create(
                eproto->user_functions.malloc, eproto->user_functions.free, frame->src_addr, frame->dst_addr,
                frame->packet_id, forward_data, forward_length, NULL, NULL,
                0, frame->packet_type, 0, 0);

            if (forward_node) {
                // 加锁保护发送队列操作
                if (eproto->user_functions.lock)
                    eproto->user_functions.lock();

                // 将节点添加到发送队列
                list_add_tail(&forward_node->list, &destination_bus_mgr->device_queues.send_queue);

                // 解锁
                if (eproto->user_functions.unlock)
                    eproto->user_functions.unlock();

                EPROTO_INFO_LOG("%s: Forwarded packet to bus %02X\n", EPROTO_BUS_NAME(current_bus_mgr),
                                destination_bus_mgr->bus.self_addr);
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
}
```

## 4. 示例用法

```c
// 后处理回调实现
void my_forward_post_func(uint8_t source_addr, uint8_t dest_addr, 
                         uint8_t* out_data, uint16_t out_length,
                         void* private_data) {
    // 释放加密后的数据
    if (out_data) {
        free(out_data);
    }
}

// 转发回调实现
eproto_error_t my_forward_callback(uint8_t source_addr, uint8_t dest_addr, 
                                  uint8_t* data, uint16_t length, 
                                  uint8_t** out_data, uint16_t* out_length,
                                  eproto_forward_post_func_t* post_func,
                                  void** private_data) {
    // 1. 解密从 source_addr 收到的数据
    uint8_t* decrypted_data = decrypt_data(data, length, get_key_for_device(source_addr));
    
    // 2. 用 dest_addr 的密钥加密数据
    *out_data = encrypt_data(decrypted_data, length, get_key_for_device(dest_addr));
    *out_length = length; // 假设加密后长度不变
    
    // 3. 设置后处理回调，用于释放加密后的数据
    *post_func = my_forward_post_func;
    
    // 4. 释放解密后的数据
    free(decrypted_data);
    
    return EPROTO_OK;
}

// 添加总线时注册回调
eproto_add_bus(&g_eproto, 0x01, my_send_func, rx_buffer, sizeof(rx_buffer), "bus1",
               my_status_callback, my_receive_callback,
               my_forward_callback);
```

## 5. 注意事项

1. **内存管理**：用户在转发回调中动态分配的内存，应该在后处理回调中释放，避免内存泄漏。

2. **错误处理**：转发回调应返回错误码，库会根据错误码决定是否使用回调提供的数据。

3. **性能考虑**：回调函数应尽量简洁，避免在回调中执行耗时操作，以免影响通信性能。

4. **向后兼容**：如果用户不需要使用转发回调，可以将其设置为 NULL，保持原有代码不变。

5. **线程安全**：如果在多线程环境中使用，需要确保回调函数的线程安全性。

6. **参数验证**：用户在实现回调函数时，应验证输入参数的有效性，避免空指针等问题。

## 6. 测试建议

1. **基本功能测试**：测试转发回调是否正常工作，数据是否能正确转发。

2. **加密/解密测试**：测试在转发过程中进行加密/解密操作是否正常。

3. **内存泄漏测试**：测试长时间运行时是否存在内存泄漏。

4. **错误处理测试**：测试回调返回错误时的处理逻辑。

5. **性能测试**：测试添加转发回调后的性能影响。

通过以上实现，eProto 库将支持用户在转发过程中实现自定义的加密/解密逻辑，解决不同设备间使用不同密钥的问题，同时提供灵活的内存管理机制。