
# 多进程eProto通信测试

## 概述

这个示例展示了如何使用 eProto 协议在多个进程之间进行通信，包括消息转发和回复功能。

## 架构

- **进程A**：设备地址 0x01，总线 1（连接进程B）
- **进程B**：设备地址 0x02，总线 2（连接进程A），总线 3（连接进程C），负责转发消息
- **进程C**：设备地址 0x03，总线 4（连接进程B）

## 编译

```bash
cd /workspace/eproto/examples/multi_process
make clean
make
```

## 运行

### 启动顺序

1. **先启动进程B（作为服务器）
2. **再启动进程A和进程C（作为客户端）

### 终端1 - 启动进程B：
```bash
./process_b
```

### 终端2 - 启动进程A：
```bash
./process_a
```

### 终端3 - 启动进程C：
```bash
./process_c
```

## 命令行使用

### 通用命令

所有进程都支持以下命令：

#### 1. 发送消息
```
send <device_addr> <need_reply> <data...>
```

- `device_addr`: 目标设备地址（十六进制或十进制）
- `need_reply`: 是否需要回复（0=不需要，1=需要）
- `data...`: 要发送的数据（十六进制格式）

示例：
```
send 2 0 11 22 33    # 发送给设备2，不需要回复，数据 0x11 0x22 0x33
send 3 1 AA BB CC    # 发送给设备3，需要回复，数据 0xAA 0xBB 0xCC
```

#### 2. 发送回复
```
send_reply <data...>
```

仅当收到需要回复的消息后，使用此命令发送回复。

示例：
```
send_reply DD EE FF    # 发送回复数据 0xDD 0xEE 0xFF
```

#### 3. 显示帮助
```
help
```

#### 4. 退出程序
```
quit
```

## 测试场景

### 场景1：进程A给进程B发送消息
在进程A终端输入：
```
send 2 0 11 22 33
```
进程B应该显示收到的消息。

### 场景2：进程B给进程C发送消息
在进程B终端输入：
```
send 3 0 44 55 66
```
进程C应该显示收到的消息。

### 场景3：进程A通过进程B转发给进程C发送消息
在进程A终端输入：
```
send 3 0 77 88 99
```
进程B显示转发信息，进程C显示收到的消息。

### 场景4：需要回复的消息
在进程A终端输入：
```
send 3 1 AA BB CC
```
进程C显示收到需要回复的消息。
在进程C终端输入：
```
send_reply DD EE FF
```
进程A显示收到的回复。

## 快速测试指南

1. 打开3个终端窗口
2. 在终端1中：`cd /workspace/eproto/examples/multi_process && ./process_b`
3. 在终端2中：`cd /workspace/eproto/examples/multi_process && ./process_a`
4. 在终端3中：`cd /workspace/eproto/examples/multi_process && ./process_c`
5. 在进程A中输入：`send 3 0 11 22 33`
6. 观察进程B和进程C的输出

## 注意事项

1. 确保按正确顺序启动进程（B → A → C）
2. 数据使用十六进制格式输入
3. 设备地址可以使用十进制或十六进制格式
4. 退出程序时使用 `quit` 命令，不要直接用 Ctrl+C，否则 socket 文件可能不会被正确清理
5. 如果遇到问题，可以手动清理 socket 文件：
```bash
rm -f /tmp/eproto_*.sock
```

## 编译状态

✅ 所有程序已成功编译，零警告！


