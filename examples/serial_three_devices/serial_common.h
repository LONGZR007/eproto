#ifndef SERIAL_COMMON_H
#define SERIAL_COMMON_H

#include <stdint.h>

typedef struct {
    int fd;
    char port[64];
    int baud_rate;
} serial_channel_t;

/**
 * @brief 初始化串口通道
 * @param channel 串口通道结构体
 */
void serial_init_channel(serial_channel_t *channel);

/**
 * @brief 打开串口
 * @param channel 串口通道结构体
 * @param port 串口号
 * @param baud_rate 波特率
 * @return 成功返回0，失败返回-1
 */
int serial_open(serial_channel_t *channel, const char *port, int baud_rate);

/**
 * @brief 关闭串口
 * @param channel 串口通道结构体
 */
void serial_close(serial_channel_t *channel);

/**
 * @brief 发送数据
 * @param channel 串口通道结构体
 * @param data 数据缓冲区
 * @param length 数据长度
 * @return 发送成功返回发送的字节数，失败返回-1
 */
int serial_send_data(serial_channel_t *channel, uint8_t *data, uint16_t length);

/**
 * @brief 接收数据
 * @param channel 串口通道结构体
 * @param buffer 接收缓冲区
 * @param buffer_size 缓冲区大小
 * @return 接收成功返回接收的字节数，失败返回-1
 */
int serial_receive_data(serial_channel_t *channel, uint8_t *buffer, uint16_t buffer_size);

/**
 * @brief 解析命令行参数
 * @param argc 参数数量
 * @param argv 参数数组
 * @param port 串口号
 * @param baud_rate 波特率
 * @return 成功返回0，失败返回-1
 */
int serial_parse_args(int argc, char *argv[], char *port, int *baud_rate);

#endif /* SERIAL_COMMON_H */