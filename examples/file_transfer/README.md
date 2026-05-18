# eProto 串口文件传输示例

基于 eProto 协议的串口文件传输程序，支持本地编译和 ARM 交叉编译。

## 文件说明

| 文件 | 说明 |
|------|------|
| `common_serial.h` / `common_serial.c` | 串口通信封装 |
| `sender.c` | 文件发送程序 |
| `receiver.c` | 文件接收程序 |
| `Makefile` | 支持本地和交叉编译的 Makefile |
| `eproto_config.h` | eProto 协议配置 |

## 本地编译 (x86)

直接运行 `make`：

```bash
cd /workspace/examples/file_transfer
make clean
make
```

编译后会在 `bin/` 目录下生成：
- `bin/file_transfer_sender` - 发送程序
- `bin/file_transfer_receiver` - 接收程序

## 交叉编译 (ARM)

需要设置 `CROSS_COMPILE` 变量指定交叉工具链前缀。

### 示例1: 使用 Linaro 工具链

```bash
CROSS_COMPILE=/workspace/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf- make arm
```

### 示例2: 使用系统自带的 ARM 工具链

```bash
# 先安装工具链 (Ubuntu/Debian)
sudo apt-get install gcc-arm-linux-gnueabihf

# 交叉编译
CROSS_COMPILE=arm-linux-gnueabihf- make arm
```

编译后会在 `bin-arm/` 目录下生成：
- `bin-arm/file_transfer_sender`
- `bin-arm/file_transfer_receiver`

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
./bin/file_transfer_receiver /dev/ttyUSB0
```

**终端2 - 启动发送程序：**
```bash
./bin/file_transfer_sender /dev/ttyUSB1
```

如果使用默认串口 `/dev/ttyUSB0`，可以直接运行：
```bash
./bin/file_transfer_receiver
./bin/file_transfer_sender
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
./bin/file_transfer_receiver /tmp/ttyV0

# 终端3：发送
./bin/file_transfer_sender /tmp/ttyV1
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

## Makefile 目标

| 目标 | 说明 |
|------|------|
| `all` (默认) | 本地编译 |
| `arm` | ARM 交叉编译 |
| `clean` | 清理本地编译产物 |
| `clean-arm` | 清理交叉编译产物 |
| `clean-all` | 清理所有编译产物 |

## 注意事项

1. 确保串口设备有读写权限
   ```bash
   sudo usermod -a -G dialout $USER
   ```
2. 使用时两个程序启动顺序不要求，但建议先启动接收方
3. 传输的文件大小受限于可用内存
4. 建议在发送数据块之间留足够的延迟
5. 交叉编译前需确保工具链已正确安装
