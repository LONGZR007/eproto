# eProto 转发加密/解密回调机制使用指南

## 1. 为什么需要转发回调机制

在多设备通信场景中，不同设备间可能使用不同的加密密钥。当设备 A 通过设备 B 转发数据到设备 C 时，由于 A 和 B、B 和 C 之间的密钥不同，直接转发加密数据会导致 C 无法解析。

例如：
- 设备 A 用密钥 K1 加密数据发送给设备 B
- 设备 B 直接转发加密数据给设备 C
- 设备 C 用密钥 K2 解密数据，由于密钥不匹配，解密失败

因此，需要在转发过程中添加回调机制，允许用户在转发时进行解密和重新加密操作，确保数据能够正确传递。

## 2. 如何使用转发回调机制

### 2.0 回调调用机制说明
当数据需要转发时，系统会调用**目标总线**的 `forward_callback` 函数。目标总线是指数据最终要发送到的总线，它最了解与下游设备的通信要求（如加密密钥、数据格式等）。

例如，当设备 A 通过总线 1 发送数据到设备 C，而设备 C 连接在总线 3 上时：
1. 总线 1 接收来自设备 A 的数据
2. 系统发现需要通过总线 3 转发数据
3. 系统调用总线 3 的 `forward_callback` 函数
4. 总线 3 的回调函数处理数据（如解密、重新加密）
5. 处理后的数据通过总线 3 发送到设备 C

### 2.1 定义回调函数

#### 2.1.1 后处理回调函数
```c
// 后处理回调函数，用于释放资源
void my_forward_post_func(uint8_t source_addr, uint8_t dest_addr, 
                         uint8_t* out_data, uint16_t out_length,
                         void* private_data) {
    // 释放加密后的数据
    if (out_data) {
        free(out_data);
    }
}
```

#### 2.1.2 转发回调函数
```c
// 转发回调函数，用于解密和重新加密数据
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
```

### 2.2 注册回调函数

在添加总线时注册转发回调函数：

```c
// 添加总线时注册回调
eproto_add_bus(&g_eproto, 0x01, my_send_func, rx_buffer, sizeof(rx_buffer), "bus1",
               my_status_callback, my_receive_callback,
               my_forward_callback);
```

### 2.3 回调函数参数说明

#### 转发回调函数参数
- `source_addr`: 数据来源总线的地址（数据从哪个总线接收）
- `dest_addr`: 数据目标总线的地址（数据将从哪个总线发送，即当前回调所属的总线）
- `data`: 原始数据
- `length`: 原始数据长度
- `out_data`: 输出数据（加密后的数据）
- `out_length`: 输出数据长度
- `post_func`: 后处理回调函数
- `private_data`: 私有数据，可用于传递额外信息给后处理回调

#### 后处理回调函数参数
- `source_addr`: 数据来源总线的地址
- `dest_addr`: 数据目标总线的地址
- `out_data`: 转发的数据
- `out_length`: 转发的数据长度
- `private_data`: 从转发回调传递的私有数据

## 3. 注意事项

1. **内存管理**：用户在转发回调中动态分配的内存，应该在后处理回调中释放，避免内存泄漏。

2. **错误处理**：转发回调应返回错误码，库会根据错误码决定是否使用回调提供的数据。

3. **性能考虑**：回调函数应尽量简洁，避免在回调中执行耗时操作，以免影响通信性能。

4. **向后兼容**：如果用户不需要使用转发回调，可以将其设置为 NULL，保持原有代码不变。

5. **线程安全**：如果在多线程环境中使用，需要确保回调函数的线程安全性。

6. **参数验证**：用户在实现回调函数时，应验证输入参数的有效性，避免空指针等问题。

## 4. 应用场景示例

### 场景1：多设备加密通信

当多个设备使用不同的加密密钥时，可以使用转发回调机制在转发过程中进行解密和重新加密：

1. 设备 A 用密钥 K1 加密数据
2. 设备 B 接收数据后，通过转发回调解密数据（使用 K1）
3. 设备 B 用密钥 K2 重新加密数据
4. 设备 C 接收数据后，用密钥 K2 解密数据

### 场景2：数据转换

除了加密/解密，转发回调还可以用于数据格式转换：

1. 设备 A 发送 JSON 格式数据
2. 设备 B 接收数据后，通过转发回调将 JSON 转换为 Protobuf 格式
3. 设备 B 转发转换后的数据
4. 设备 C 接收 Protobuf 格式数据

## 5. 测试建议

1. **基本功能测试**：测试转发回调是否正常工作，数据是否能正确转发。

2. **加密/解密测试**：测试在转发过程中进行加密/解密操作是否正常。

3. **内存泄漏测试**：测试长时间运行时是否存在内存泄漏。

4. **错误处理测试**：测试回调返回错误时的处理逻辑。

5. **性能测试**：测试添加转发回调后的性能影响。

通过使用转发回调机制，eProto 库可以支持用户在转发过程中实现自定义的加密/解密逻辑，解决不同设备间使用不同密钥的问题，同时提供灵活的内存管理机制。