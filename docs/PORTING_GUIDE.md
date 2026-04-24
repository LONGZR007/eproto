# eProto 移植指南

## 1. 移植概述

eProto 是一个设计为可移植的嵌入式通信协议库，通过抽象层设计支持各种硬件平台和操作系统。本指南将帮助您将 eProto 移植到您的目标平台。

### 1.1 移植目标

- 适配不同的硬件平台（如 ARM、x86、RISC-V 等）
- 适配不同的操作系统（如 Linux、FreeRTOS、裸机系统等）
- 适配不同的通信总线（如 UART、SPI、I2C、CAN 等）

### 1.2 移植难度

eProto 的移植难度适中，主要需要实现以下接口：

- 内存分配接口
- 时间戳接口
- 信号处理接口
- 锁机制接口
- 总线接口

## 2. 移植准备

### 2.1 硬件要求

- 目标硬件平台
- 通信总线（如 UART、SPI、I2C 等）
- 足够的内存空间（至少几百字节）

### 2.2 软件要求

- 编译器（如 GCC、Keil、IAR 等）
- 构建系统（如 Makefile、CMake 等）
- 目标平台的 SDK 或库

### 2.3 资源准备

- eProto 源代码
- 目标平台的开发环境
- 目标平台的硬件文档

## 3. 移植步骤

### 3.1 第一步：配置协议

1. **复制配置文件**：
   ```bash
   cp inc/eproto_config_example.h inc/eproto_config.h
   ```

2. **修改配置选项**：根据目标平台的资源情况，修改 `eproto_config.h` 中的配置选项：
   - `EPROTO_MAX_BUS_COUNT`：最大总线数量
   - `EPROTO_MAX_DESTINATION_DEVICES`：每个总线的最大目标设备数量
   - `EPROTO_BUFFER_SIZE`：缓冲区大小
   - `EPROTO_MAX_PACKET_SIZE`：最大数据包大小
   - `EPROTO_DEFAULT_TIMEOUT`：默认超时时间
   - `EPROTO_DEFAULT_RETRY_COUNT`：默认重发次数
   - `EPROTO_ENABLE_HANDSHAKE`：是否启用握手功能

### 3.2 第二步：实现系统接口

需要实现 `eproto_user_functions_t` 结构体中的接口函数：

#### 3.2.1 内存分配接口

```c
void* my_malloc(size_t size) {
    // 实现内存分配
    return malloc(size);
}

void my_free(void* ptr) {
    // 实现内存释放
    free(ptr);
}
```

#### 3.2.2 时间戳接口

```c
uint32_t my_get_timestamp(void) {
    // 实现时间戳获取，返回毫秒
    return get_system_time_ms();
}
```

#### 3.2.3 信号处理接口

```c
eproto_signal_result_t my_signal_wait(uint32_t timestamp) {
    // 实现信号等待
    // 返回 EPROTO_SIGNAL_DATA、EPROTO_SIGNAL_TIMEOUT 或 EPROTO_SIGNAL_NO_PROGRESS
}

void my_signal_send(void) {
    // 实现信号发送
}
```

#### 3.2.4 锁机制接口

```c
void my_lock(void) {
    // 实现锁获取
}

void my_unlock(void) {
    // 实现锁释放
}
```

### 3.3 第三步：实现总线接口

需要实现 `eproto_bus_t` 结构体中的接口函数：

```c
// 总线接口
void my_bus_send(eproto_bus_t* bus, uint8_t* data, uint16_t length) {
    (void)bus;  // 避免未使用参数警告
    // 实现数据发送
    uart_send(data, length);
}

uint16_t my_bus_receive(uint8_t* buffer, uint16_t size) {
    // 实现数据接收
    return uart_receive(buffer, size);
}
```

### 3.4 第四步：集成到项目

1. **添加头文件**：将 eProto 的头文件添加到项目中
2. **添加源文件**：将 eProto 的源文件添加到项目中
3. **配置编译选项**：确保编译器能够正确编译 eProto 的代码
4. **链接库**：如果需要，链接必要的库

### 3.5 第五步：测试和验证

1. **基本功能测试**：测试初始化、发送和接收功能
2. **可靠性测试**：测试在各种条件下的通信可靠性
3. **性能测试**：测试通信性能和资源使用情况

## 4. 硬件平台适配

### 4.1 ARM 平台

#### Cortex-M 系列

- **内存分配**：可以使用 `malloc`/`free` 或实现静态内存分配
- **时间戳**：使用 SysTick 定时器或其他硬件定时器
- **信号处理**：使用 CMSIS 中的信号量或事件标志
- **锁机制**：使用 CMSIS 中的互斥锁
- **总线接口**：根据硬件平台实现 UART、SPI、I2C 等接口

#### Cortex-A 系列

- **内存分配**：使用标准 C 库的 `malloc`/`free`
- **时间戳**：使用 `gettimeofday` 或硬件定时器
- **信号处理**：使用 POSIX 信号量
- **锁机制**：使用 POSIX 互斥锁
- **总线接口**：根据硬件平台实现各种总线接口

### 4.2 RISC-V 平台

- **内存分配**：使用标准 C 库的 `malloc`/`free` 或实现静态内存分配
- **时间戳**：使用硬件定时器
- **信号处理**：根据操作系统实现信号处理
- **锁机制**：根据操作系统实现锁机制
- **总线接口**：根据硬件平台实现各种总线接口

### 4.3 x86 平台

- **内存分配**：使用标准 C 库的 `malloc`/`free`
- **时间戳**：使用 `gettimeofday`
- **信号处理**：使用 POSIX 信号量
- **锁机制**：使用 POSIX 互斥锁
- **总线接口**：实现串口、网络等接口

## 5. 操作系统适配

### 5.1 裸机系统

- **内存分配**：实现静态内存分配或简单的内存池
- **时间戳**：使用硬件定时器
- **信号处理**：使用标志位或简单的事件机制
- **锁机制**：如果是单线程系统，可以为空实现
- **总线接口**：直接操作硬件寄存器

### 5.2 FreeRTOS

- **内存分配**：使用 `pvPortMalloc`/`vPortFree`
- **时间戳**：使用 `xTaskGetTickCount`
- **信号处理**：使用 `xSemaphoreTake`/`xSemaphoreGive`
- **锁机制**：使用 `xSemaphoreTake`/`xSemaphoreGive` 或 `taskENTER_CRITICAL`/`taskEXIT_CRITICAL`
- **总线接口**：使用 FreeRTOS 的队列或直接操作硬件

### 5.3 Linux

- **内存分配**：使用标准 C 库的 `malloc`/`free`
- **时间戳**：使用 `gettimeofday`
- **信号处理**：使用 POSIX 信号量
- **锁机制**：使用 POSIX 互斥锁
- **总线接口**：使用 Linux 设备驱动

### 5.4 其他 RTOS

- **内存分配**：使用 RTOS 提供的内存分配函数
- **时间戳**：使用 RTOS 提供的时间函数
- **信号处理**：使用 RTOS 提供的信号量或事件机制
- **锁机制**：使用 RTOS 提供的互斥锁
- **总线接口**：根据 RTOS 提供的接口实现

## 6. 通信总线适配

### 6.1 UART 总线

- **发送函数**：调用 UART 发送函数，如 `HAL_UART_Transmit` 或直接操作寄存器
- **接收函数**：调用 UART 接收函数，如 `HAL_UART_Receive` 或直接操作寄存器
- **注意事项**：处理波特率、数据位、停止位、校验位等参数

### 6.2 SPI 总线

- **发送函数**：调用 SPI 发送函数，如 `HAL_SPI_Transmit` 或直接操作寄存器
- **接收函数**：调用 SPI 接收函数，如 `HAL_SPI_Receive` 或直接操作寄存器
- **注意事项**：处理 SPI 模式、时钟极性、时钟相位等参数

### 6.3 I2C 总线

- **发送函数**：调用 I2C 发送函数，如 `HAL_I2C_Master_Transmit` 或直接操作寄存器
- **接收函数**：调用 I2C 接收函数，如 `HAL_I2C_Master_Receive` 或直接操作寄存器
- **注意事项**：处理 I2C 地址、速率等参数

### 6.4 CAN 总线

- **发送函数**：调用 CAN 发送函数，如 `HAL_CAN_AddTxMessage` 或直接操作寄存器
- **接收函数**：调用 CAN 接收函数，如 `HAL_CAN_GetRxMessage` 或直接操作寄存器
- **注意事项**：处理 CAN ID、数据长度等参数

### 6.5 网络总线

- **发送函数**：使用 socket 发送数据
- **接收函数**：使用 socket 接收数据
- **注意事项**：处理网络协议、IP 地址、端口等参数

## 7. 测试和验证

### 7.1 基本功能测试

1. **初始化测试**：测试 eProto 实例的初始化
2. **发送测试**：测试数据发送功能
3. **接收测试**：测试数据接收功能
4. **回复测试**：测试数据回复功能
5. **广播测试**：测试广播功能

### 7.2 可靠性测试

1. **超时测试**：测试超时处理机制
2. **重发测试**：测试重发机制
3. **CRC 错误测试**：测试 CRC 错误处理
4. **丢包测试**：测试丢包情况下的处理
5. **多设备测试**：测试多设备通信

### 7.3 性能测试

1. **吞吐量测试**：测试最大数据传输速率
2. **延迟测试**：测试数据传输延迟
3. **内存使用测试**：测试内存使用情况
4. **CPU 使用率测试**：测试 CPU 使用率

## 8. 常见问题和解决方案

### 8.1 内存分配问题

**问题**：内存分配失败或内存泄漏

**解决方案**：
- 确保内存分配函数正确实现
- 合理设置缓冲区大小
- 及时释放不再使用的内存
- 考虑使用静态内存分配

### 8.2 时间戳问题

**问题**：时间戳不准确或不单调

**解决方案**：
- 确保时间戳函数返回准确的时间
- 确保时间戳是单调递增的
- 考虑使用硬件定时器

### 8.3 信号处理问题

**问题**：信号丢失或重复

**解决方案**：
- 确保信号处理函数正确实现
- 考虑使用更可靠的信号机制
- 避免在信号处理函数中执行耗时操作

### 8.4 锁机制问题

**问题**：死锁或竞态条件

**解决方案**：
- 确保锁函数正确实现
- 避免嵌套锁
- 确保锁的获取和释放配对
- 考虑使用无锁数据结构

### 8.5 总线接口问题

**问题**：数据传输错误或丢失

**解决方案**：
- 确保总线接口函数正确实现
- 检查硬件连接
- 调整总线参数（如波特率、时钟频率等）
- 考虑使用硬件流控制

## 9. 最佳实践

### 9.1 内存管理

- **使用静态内存分配**：在资源受限的系统中，使用静态内存分配可以避免内存碎片
- **合理设置缓冲区大小**：根据实际需求设置缓冲区大小，避免过大或过小
- **实现内存池**：对于频繁分配和释放的内存，考虑实现内存池

### 9.2 时间管理

- **使用硬件定时器**：硬件定时器通常更准确和可靠
- **校准时间戳**：定期校准时间戳，确保时间的准确性
- **考虑时区**：如果需要，考虑时区问题

### 9.3 信号处理

- **使用事件标志**：在裸机系统中，使用事件标志可以简化信号处理
- **避免忙等**：使用信号量或其他机制避免忙等，减少 CPU 使用率
- **考虑优先级**：合理设置信号处理的优先级

### 9.4 锁机制

- **最小化锁的范围**：只在必要时获取锁，减少锁的持有时间
- **避免锁的嵌套**：嵌套锁容易导致死锁
- **使用无锁设计**：对于简单的数据结构，考虑使用无锁设计

### 9.5 总线接口

- **实现错误处理**：在总线接口中实现错误处理，提高系统的可靠性
- **添加缓冲区**：在总线接口中添加缓冲区，提高数据传输的效率
- **考虑中断**：对于高速总线，考虑使用中断处理

## 10. 示例移植

### 10.1 裸机系统移植示例

```c
// 内存分配接口
void* my_malloc(size_t size) {
    // 使用静态内存池
    static uint8_t memory_pool[1024];
    static size_t next_free = 0;
    
    if (next_free + size <= sizeof(memory_pool)) {
        void* ptr = &memory_pool[next_free];
        next_free += size;
        return ptr;
    }
    return NULL;
}

void my_free(void* ptr) {
    // 简单实现，不做实际释放
    (void)ptr;
}

// 时间戳接口
uint32_t my_get_timestamp(void) {
    // 使用 SysTick 定时器
    return HAL_GetTick();
}

// 信号处理接口
eproto_signal_result_t my_signal_wait(uint32_t timestamp) {
    uint32_t current_time = my_get_timestamp();
    if (timestamp > current_time) {
        // 简单延时
        uint32_t delay = timestamp - current_time;
        for (uint32_t i = 0; i < delay * 1000; i++) {
            __NOP();
        }
        return EPROTO_SIGNAL_TIMEOUT;
    }
    return EPROTO_SIGNAL_DATA;
}

void my_signal_send(void) {
    // 空实现
}

// 锁机制接口
void my_lock(void) {
    // 单线程系统，空实现
}

void my_unlock(void) {
    // 单线程系统，空实现
}

// 总线接口
void my_bus_send(eproto_bus_t* bus, uint8_t* data, uint16_t length) {
    (void)bus;  // 避免未使用参数警告
    HAL_UART_Transmit(&huart1, data, length, HAL_MAX_DELAY);
}

uint16_t my_bus_receive(uint8_t* buffer, uint16_t size) {
    uint16_t count = 0;
    while (count < size) {
        if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE)) {
            buffer[count++] = (uint8_t)(huart1.Instance->DR & 0xFF);
        } else {
            break;
        }
    }
    return count;
}
```

### 10.2 FreeRTOS 移植示例

```c
// 内存分配接口
void* my_malloc(size_t size) {
    return pvPortMalloc(size);
}

void my_free(void* ptr) {
    vPortFree(ptr);
}

// 时间戳接口
uint32_t my_get_timestamp(void) {
    return xTaskGetTickCount();
}

// 信号处理接口
static SemaphoreHandle_t xSemaphore = NULL;

eproto_signal_result_t my_signal_wait(uint32_t timestamp) {
    uint32_t current_time = xTaskGetTickCount();
    if (timestamp > current_time) {
        TickType_t xTicksToWait = timestamp - current_time;
        if (xSemaphoreTake(xSemaphore, xTicksToWait) == pdTRUE) {
            return EPROTO_SIGNAL_DATA;
        } else {
            return EPROTO_SIGNAL_TIMEOUT;
        }
    }
    return EPROTO_SIGNAL_DATA;
}

void my_signal_send(void) {
    xSemaphoreGive(xSemaphore);
}

// 锁机制接口
static SemaphoreHandle_t xMutex = NULL;

void my_lock(void) {
    xSemaphoreTake(xMutex, portMAX_DELAY);
}

void my_unlock(void) {
    xSemaphoreGive(xMutex);
}

// 总线接口
void my_bus_send(eproto_bus_t* bus, uint8_t* data, uint16_t length) {
    (void)bus;  // 避免未使用参数警告
    HAL_UART_Transmit(&huart1, data, length, HAL_MAX_DELAY);
}

uint16_t my_bus_receive(uint8_t* buffer, uint16_t size) {
    uint16_t count = 0;
    while (count < size) {
        if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE)) {
            buffer[count++] = (uint8_t)(huart1.Instance->DR & 0xFF);
        } else {
            break;
        }
    }
    return count;
}

// 初始化信号量和互斥锁
void vSetupProtocols(void) {
    xSemaphore = xSemaphoreCreateBinary();
    xMutex = xSemaphoreCreateMutex();
}
```

### 10.3 Linux 移植示例

```c
// 内存分配接口
void* my_malloc(size_t size) {
    return malloc(size);
}

void my_free(void* ptr) {
    free(ptr);
}

// 时间戳接口
uint32_t my_get_timestamp(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

// 信号处理接口
static sem_t semaphore;

eproto_signal_result_t my_signal_wait(uint32_t timestamp) {
    uint32_t current_time = my_get_timestamp();
    if (timestamp > current_time) {
        struct timespec ts;
        uint32_t timeout_ms = timestamp - current_time;
        
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += timeout_ms / 1000;
        ts.tv_nsec += (timeout_ms % 1000) * 1000000;
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_sec++;
            ts.tv_nsec -= 1000000000;
        }
        
        if (sem_timedwait(&semaphore, &ts) == 0) {
            return EPROTO_SIGNAL_DATA;
        } else {
            return EPROTO_SIGNAL_TIMEOUT;
        }
    }
    return EPROTO_SIGNAL_DATA;
}

void my_signal_send(void) {
    sem_post(&semaphore);
}

// 锁机制接口
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void my_lock(void) {
    pthread_mutex_lock(&mutex);
}

void my_unlock(void) {
    pthread_mutex_unlock(&mutex);
}

// 总线接口
int serial_fd = -1;

void my_bus_send(eproto_bus_t* bus, uint8_t* data, uint16_t length) {
    (void)bus;  // 避免未使用参数警告
    write(serial_fd, data, length);
}

uint16_t my_bus_receive(uint8_t* buffer, uint16_t size) {
    return read(serial_fd, buffer, size);
}

// 初始化串口和信号量
void setup_protocols(void) {
    // 打开串口
    serial_fd = open("/dev/ttyUSB0", O_RDWR | O_NOCTTY | O_NDELAY);
    if (serial_fd < 0) {
        perror("Open serial port failed");
        exit(1);
    }
    
    // 配置串口
    struct termios options;
    tcgetattr(serial_fd, &options);
    cfsetispeed(&options, B115200);
    cfsetospeed(&options, B115200);
    options.c_cflag |= (CLOCAL | CREAD);
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    tcsetattr(serial_fd, TCSANOW, &options);
    
    // 初始化信号量
    sem_init(&semaphore, 0, 0);
}
```

## 11. 总结

本移植指南提供了将 eProto 移植到不同硬件平台和操作系统的详细步骤和建议。通过正确实现系统接口和总线接口，您可以在各种嵌入式系统中使用 eProto 实现可靠的设备间通信。

在移植过程中，建议：

1. 仔细阅读本指南，了解移植的基本步骤和注意事项
2. 参考示例移植代码，学习如何实现各种接口
3. 根据目标平台的特点，调整配置选项和实现方法
4. 进行充分的测试，确保移植的正确性和可靠性
5. 遵循最佳实践，提高系统的性能和稳定性

通过成功移植 eProto，您可以在您的嵌入式系统中实现高效、可靠的通信功能，为系统的整体性能和可靠性做出贡献。