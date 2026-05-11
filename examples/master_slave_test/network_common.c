/*
 * MIT License
 *
 * Copyright (c) 2026 LONGZR007
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "network_common.h"
#include <time.h>

int network_init_channel(network_channel_t* channel, const char* ip, uint16_t port) {
    if (!channel) {
        return -1;
    }

    memset(channel, 0, sizeof(network_channel_t));

    strncpy(channel->server_ip, ip, sizeof(channel->server_ip) - 1);
    channel->server_port = port;

    // 创建UDP socket
    channel->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (channel->sockfd < 0) {
        printf("Failed to create socket\n");
        return -1;
    }

    // 设置SO_REUSEADDR选项
    int opt_reuse = 1;
    if (setsockopt(channel->sockfd, SOL_SOCKET, SO_REUSEADDR, &opt_reuse, sizeof(opt_reuse)) < 0) {
        printf("Warning: Failed to set SO_REUSEADDR\n");
    }

    // 设置SO_REUSEPORT选项（如果支持）
    if (setsockopt(channel->sockfd, SOL_SOCKET, SO_REUSEPORT, &opt_reuse, sizeof(opt_reuse)) < 0) {
        printf("Warning: Failed to set SO_REUSEPORT\n");
    }

    // 设置socket地址
    memset(&channel->server_addr, 0, sizeof(channel->server_addr));
    channel->server_addr.sin_family = AF_INET;
    channel->server_addr.sin_port = htons(port);
    channel->server_addr.sin_addr.s_addr = inet_addr(ip);

    // 绑定socket
    if (bind(channel->sockfd, (struct sockaddr*)&channel->server_addr, sizeof(channel->server_addr)) < 0) {
        printf("Failed to bind socket to %s:%d: %s\n", ip, port, strerror(errno));
        close(channel->sockfd);
        return -1;
    }
    printf("UDP socket bound to %s:%d\n", ip, port);

    // 设置为非阻塞
    int flags = fcntl(channel->sockfd, F_GETFL, 0);
    fcntl(channel->sockfd, F_SETFL, flags | O_NONBLOCK);

    return 0;
}

int network_send_data(network_channel_t* channel, uint8_t* data, uint16_t length) {
    if (!channel || !data || length == 0) {
        return -1;
    }

    // 发送到广播地址（所有绑定到相同地址和端口的设备都能收到）
    struct sockaddr_in dest_addr = channel->server_addr;

    int sent = sendto(channel->sockfd, data, length, 0,
                    (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    if (sent < 0) {
        printf("Failed to send data: %s\n", strerror(errno));
        return -1;
    }
    return sent;
}

int network_receive_data(network_channel_t* channel, uint8_t* data, uint16_t max_length) {
    if (!channel || !data) {
        return -1;
    }

    struct sockaddr_in sender_addr;
    socklen_t addr_len = sizeof(sender_addr);
    int received = recvfrom(channel->sockfd, data, max_length, 0,
                           (struct sockaddr*)&sender_addr, &addr_len);

    if (received > 0) {
        return received;
    } else if (received == 0) {
        return 0;
    } else {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // 非阻塞模式下没有数据
            return 0;
        }
        printf("Failed to receive data: %s\n", strerror(errno));
        return -1;
    }
}

void network_close_channel(network_channel_t* channel) {
    if (!channel) {
        return;
    }

    if (channel->sockfd >= 0) {
        close(channel->sockfd);
    }

    memset(channel, 0, sizeof(network_channel_t));
}
