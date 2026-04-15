# 握手功能修改计划

## 1. 需求分析

根据用户要求，需要对 eProto 协议的握手功能进行以下修改：

1. **改用握手标志**：与重发标志（`EPROTO_PACKET_TYPE_RETRANSMIT_FLAG`）一样的处理逻辑
2. **主动发送握手包类型**：类型为 `USER_SEND | HANDSHAKE_FLAG`
3. **握手包改为需要用户回复的包**：不再是协议层自动回复，而是需要用户应用层回复
4. **清握手标志时机**：
   - 收到对方主动发的握手包
   - 收到对方的握手回复包
5. **固定回调函数**：发送握手包时创建一个固定回调函数，不同总线可以使用私有参数区分
6. **回调处理**：收到主动发送的握手包不可调用接收回调函数，要调用状态回调函数

## 2. 代码分析

### 2.1 当前实现

- **frame_parser.h**：定义了 `EPROTO_PACKET_TYPE_HANDSHAKE_FLAG` (0x40) 作为握手标志
- **eproto.h**：在 `eproto_bus_manager_t` 结构体中有 `handshake_required` 字段表示是否需要握手
- **eproto.c**：
  - `eproto_process_user_send_packet`：处理握手包，发送协议 ACK 并清除握手标志
  - `eproto_process_user_reply_packet`：处理握手回复包，发送协议 ACK 并清除握手标志
  - `eproto_process_protocol_ack_packet`：收到协议 ACK 时也会清除握手标志

### 2.2 需要修改的文件

1. **/workspace/eproto/src/eproto.c**：
   - 修改 `eproto_send_ex` 函数，添加握手检查和握手包发送逻辑
   - 修改 `eproto_process_protocol_ack_packet` 函数，移除收到协议 ACK 时清除握手标志的逻辑
   - 添加 `handshake_callback` 函数，用于处理握手回复
   - 确保握手包的处理逻辑正确

2. **/workspace/eproto/inc/frame_parser.h**：
   - 确保 `EPROTO_PACKET_TYPE_HANDSHAKE_FLAG` 定义正确

## 3. 详细修改计划

### 3.1 步骤 1：添加握手回调函数

在 `eproto.c` 中添加 `handshake_callback` 函数，用于处理握手回复：

```c
// 握手回调函数
static void handshake_callback(eproto_error_t error, uint16_t packet_id, uint8_t* data, uint16_t length, void* private_data) {
    eproto_bus_manager_t* bus_mgr = (eproto_bus_manager_t*)private_data;
    
    if (error == EPROTO_SEND_SUCCESS) {
        EPROTO_INFO_LOG("%s: Handshake reply received successfully\n", EPROTO_BUS_NAME(bus_mgr));
    } else {
        EPROTO_WARNING_LOG("%s: Handshake reply failed, error: %d\n", EPROTO_BUS_NAME(bus_mgr), error);
    }
}
```

### 3.2 步骤 2：修改 eproto_send_ex 函数

在 `eproto_send_ex` 函数中，添加握手检查和握手包发送逻辑：

1. 检查是否需要握手
2. 如果需要，创建握手包节点并添加到发送队列
3. 使用 `handshake_callback` 作为回调函数，总线管理器作为私有数据

### 3.3 步骤 3：修改 eproto_process_protocol_ack_packet 函数

移除 `eproto_process_protocol_ack_packet` 函数中收到协议 ACK 时清除握手标志的逻辑，只在收到握手包或握手回复包时清除。

### 3.4 步骤 4：修改 eproto_process_user_send_packet 函数

确保 `eproto_process_user_send_packet` 函数正确处理握手包：

1. 检查是否是握手包
2. 如果是，发送协议 ACK
3. 清除握手标志
4. 调用状态回调函数
5. 不调用接收回调函数
6. 等待用户应用层回复

### 3.5 步骤 5：修改 eproto_process_user_reply_packet 函数

确保 `eproto_process_user_reply_packet` 函数正确处理握手回复包：

1. 检查是否是握手回复包
2. 如果是，发送协议 ACK
3. 清除握手标志
4. 调用状态回调函数
5. 处理等待队列中的握手节点

## 4. 风险和注意事项

1. **兼容性**：修改后需要确保与现有代码兼容，特别是握手包的处理逻辑
2. **回调函数**：确保握手回调函数正确处理不同总线的情况
3. **测试**：需要测试握手功能的各种场景，包括正常握手、握手失败、超时等
4. **错误处理**：确保在握手过程中出现错误时能够正确处理

## 5. 测试计划

1. **单元测试**：测试握手标志的设置和清除逻辑
2. **集成测试**：测试完整的握手流程，包括发送握手包、接收握手包、发送握手回复、接收握手回复
3. **边界测试**：测试握手超时、握手失败等边界情况

## 6. 预期结果

1. 握手功能使用标志位实现，与重发标志的处理逻辑一致
2. 主动发送握手包时使用 `USER_SEND | HANDSHAKE_FLAG` 类型
3. 握手包需要用户应用层回复
4. 握手标志在收到对方主动发的握手包和收到对方的握手回复包时清除
5. 发送握手包时使用固定回调函数，不同总线通过私有参数区分
6. 收到主动发送的握手包时调用状态回调函数，不调用接收回调函数
