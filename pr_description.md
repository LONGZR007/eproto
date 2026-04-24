# 重构 eProto 总线管理结构

## 变更描述

此次变更对 eProto 库进行了架构性重构，主要包括以下内容：

1. **重新设计总线结构体**：将 `rx_buffer` 从 `eproto_bus_t` 移动到 `eproto_bus_manager_t`，使总线接口更加清晰。

2. **修改回调函数签名**：所有回调函数类型定义添加 `eproto_bus_t* bus` 参数，使回调函数能够访问总线上下文。

3. **调整总线添加方式**：将 `eproto_add_bus` 函数从多个单独参数改为接受 `eproto_bus_t` 指针，简化 API 调用。

4. **移除冗余字段**：从 `eproto_bus_manager_t` 中删除了 3 个回调函数字段，改为直接使用 `bus` 结构体中的回调函数。

5. **更新示例代码**：所有示例代码都已更新，确保与新的 API 接口兼容。

## 变更的文件

- `inc/eproto.h`：重新设计总线结构体，修改回调函数签名，调整 `eproto_add_bus` 函数接口
- `inc/eproto_def.h`：更新 `EPROTO_BUS_NAME` 宏定义
- `src/eproto.c`：更新实现以适配新的 API 接口
- `examples/forward_callback_test/`：更新示例代码
- `examples/simple_test/`：更新示例代码
- `examples/thread_example/`：更新示例代码
- `examples/topology_test/`：更新示例代码
- `examples/serial_three_devices/`：更新示例代码

## 测试结果

所有示例都已成功编译，验证了变更的正确性：

- `simple_test`：编译成功
- `forward_callback_test`：编译成功
- `thread_example`：编译成功
- `topology_test`：编译成功
- `serial_three_devices`：编译成功

## 影响

此次变更对现有代码有一定的破坏性，需要更新所有使用 eProto 库的代码，以适应新的 API 接口。但变更后代码结构更加清晰，API 更加简洁，有利于后续的维护和扩展。