# 握手功能修改计划

## 1. 仓库分析结论

当前握手功能的实现存在以下问题：
- 定义了独立的握手包类型（EPROTO_PACKET_TYPE_HANDSHAKE）
- 握手包不需要用户回复
- 清握手标志的时机不符合要求
- 缺乏针对不同总线的回调区分机制

## 2. 需要修改的文件

1. **/workspace/eproto/inc/frame_parser.h**
   - 删除 EPROTO_PACKET_TYPE_HANDSHAKE 枚举值
   - 保留 EPROTO_PACKET_TYPE_HANDSHAKE_FLAG 标志

2. **/workspace/eproto/src/eproto.c**
   - 修改 eproto_process_send_queue 函数中的握手包发送逻辑
   - 修改 eproto_process_handshake_packet 函数
   - 修改 eproto_process_protocol_ack_packet 函数
   - 修改 eproto_process_user_reply_packet 函数，处理握手回复
   - 添加握手回调函数

3. **/workspace/eproto/inc/eproto.h**
   - 可能需要调整状态回调相关定义

## 3. 修改步骤

### 步骤 1: 修改 frame_parser.h
- 删除 EPROTO_PACKET_TYPE_HANDSHAKE 枚举值
- 确保 EPROTO_PACKET_TYPE_HANDSHAKE_FLAG 标志保持不变

### 步骤 2: 修改 eproto_process_send_queue 函数
- 当需要握手时，创建 USER_SEND 类型的包并设置 HANDSHAKE_FLAG
- 设置 no_wait = 0，表示需要回复
- 创建固定的握手回调函数，使用私有参数区分不同总线

### 步骤 3: 修改握手包处理
- 移除 eproto_process_handshake_packet 函数
- 在 eproto_process_user_send_packet 函数中处理带 HANDSHAKE_FLAG 的包
- 收到握手包时调用状态回调，不调用接收回调
- 清握手标志

### 步骤 4: 修改握手回复处理
- 在 eproto_process_user_reply_packet 函数中处理带 HANDSHAKE_FLAG 的回复包
- 清握手标志
- 调用状态回调

### 步骤 5: 添加握手回调函数
- 创建固定的握手回调函数
- 使用私有参数区分不同总线
- 处理握手成功/失败的情况

## 4. 潜在依赖和考虑事项

- 确保握手标志与重发标志的处理逻辑一致
- 确保不同总线的握手回调能够正确区分
- 确保握手回复包的处理逻辑正确
- 确保状态回调函数能够正确处理握手相关的状态

## 5. 风险处理

- 可能影响现有的发送逻辑，需要仔细测试
- 可能影响现有的回调机制，需要确保兼容性
- 可能影响现有的状态处理，需要确保状态一致性

## 6. 测试计划

- 测试握手功能的基本流程
- 测试不同总线的握手回调区分
- 测试握手失败的处理
- 测试握手与重发的交互
- 测试握手与其他包类型的交互