# eProto 串口文件传输示例

基于 eProto 协议的串口文件传输程序。

## 文件说明

| 文件 | 说明 |
|------|------|
| `common_serial.h` / `common_serial.c` | 串口通信封装 |
| `sender.c` | 文件发送程序 |
| `receiver.c` | 文件接收程序 |
| `Makefile.serial` | 串口版本的 Makefile |

## 编译

```bash
cd /workspace/examples/file_transfer
make -f Makefile.serial
```

编译完成后会生成：
- `file_transfer_sender` - 发送程序
- `file_transfer_receiver` - 接收程序

## 使用方法

### 硬件连接

需要使用 USB-to-TTL 转换模块连接两台设备，或者在一台设备上使用两个串口通过虚拟串口连接测试。

串口连接配置：
- 波特率：115200 bps
- 数据位：8
- 停止位：1
- 校验位：无
- 流控：无

### 在两个终端分别运行

**终端1 - 启动接收程序：**
```bash
./file_transfer_receiver /dev/ttyUSB0
```

**终端2 - 启动发送程序：**
```bash
./file_transfer_sender /dev/ttyUSB1
```

如果使用默认串口 `/dev/ttyUSB0`，可以直接运行：
```bash
./file_transfer_receiver
./file_transfer_sender
```

### 虚拟串口测试（单台设备）

如果没有硬件，也可以使用 `socat` 创建虚拟串口进行测试：

```bash
# 终端1：创建虚拟串口对
socat -d -d pty,raw,echo=0,link=/tmp/ttyV0 pty,raw,echo=0,link=/tmp/ttyV1
```

然后分别在其他终端运行：
```bash
# 终端2：接收
./file_transfer_receiver /tmp/ttyV0

# 终端3：发送
./file_transfer_sender /tmp/ttyV1
```

## 传输流程

1. 发送方创建测试文件（512字节）
2. 发送 FILE_START_REQ 包（包含文件名、文件大小）
3. 接收方回送 FILE_START_RSP
4. 发送方分包发送 FILE_DATA（每包有偏移和序列号）
5. 接收方接收并拼接到缓冲区
6. 发送方发送 FILE_END 包（包含完整文件 CRC）
7. 接收方校验 CRC 并保存文件

## 自定义文件传输

如果要传输真实文件，修改 `sender.c` 中的 `create_test_file` 函数：

```c
static void create_test_file(void) {
    // 读取真实文件
    FILE* f = fopen("your_file.bin", "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        g_test_file_size = ftell(f);
        fseek(f, 0, SEEK_SET);
        g_test_file_data = (uint8_t*)malloc(g_test_file_size);
        fread(g_test_file_data, 1, g_test_file_size, f);
        fclose(f);
        g_test_filename = "your_file.bin";
    }
}
```

## 注意事项

1. 确保串口设备有读写权限
   ```bash
   sudo usermod -a -G dialout $USER
   ```
2. 使用时两个程序启动顺序不要求，但建议先启动接收方
3. 传输的文件大小受限于可用内存
4. 建议在发送数据块之间留足够的延迟
