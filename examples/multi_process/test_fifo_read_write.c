#include "ipc_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// 测试FIFO读写功能的程序

int main(void) {
    printf("=== 测试FIFO读写功能 ===\n\n");

    // 清理之前的FIFO文件
    ipc_cleanup_fifos();

    // 初始化通道
    ipc_channel_t tx_channel, rx_channel;
    ipc_init_channel(&tx_channel);
    ipc_init_channel(&rx_channel);

    // 打开FIFO通道
    printf("打开FIFO通道...\n");
    if (ipc_open_fifo(&tx_channel, FIFO_PATH_A_TO_B) < 0) {
        printf("打开TX FIFO失败\n");
        return 1;
    }

    if (ipc_open_fifo(&rx_channel, FIFO_PATH_B_TO_A) < 0) {
        printf("打开RX FIFO失败\n");
        ipc_close_channel(&tx_channel);
        return 1;
    }

    // 测试数据
    uint8_t test_data[] = {0x11, 0x22, 0x33, 0x44, 0x55};
    uint8_t rx_buffer[1024];

    // 发送数据
    printf("\n发送测试数据: ");
    for (int i = 0; i < sizeof(test_data); i++) {
        printf("%02X ", test_data[i]);
    }
    printf("\n");

    int sent = ipc_send_data(&tx_channel, test_data, sizeof(test_data));
    if (sent < 0) {
        printf("发送失败\n");
    } else {
        printf("成功发送 %d 字节\n", sent);
    }

    // 接收数据
    printf("\n等待接收数据...\n");
    int received = 0;
    int attempts = 0;

    while (received == 0 && attempts < 10) {
        received = ipc_receive_data(&rx_channel, rx_buffer, sizeof(rx_buffer));
        if (received > 0) {
            printf("成功接收 %d 字节: ", received);
            for (int i = 0; i < received; i++) {
                printf("%02X ", rx_buffer[i]);
            }
            printf("\n");
        } else {
            attempts++;
            usleep(100000);  // 等待100ms
        }
    }

    if (received == 0) {
        printf("接收超时\n");
    }

    // 关闭通道
    ipc_close_channel(&tx_channel);
    ipc_close_channel(&rx_channel);

    // 清理FIFO文件
    ipc_cleanup_fifos();

    printf("\n=== 测试完成 ===\n");
    return 0;
}
