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

int network_init_channel(network_channel_t* channel, network_protocol_t protocol, const char* ip, uint16_t port, int is_server) {
    if (!channel) {
        return -1;
    }

    memset(channel, 0, sizeof(network_channel_t));
    channel->protocol = protocol;
    channel->is_server = is_server;
    channel->max_fd = -1;

    strncpy(channel->server_ip, ip, sizeof(channel->server_ip) - 1);
    channel->server_port = port;

    if (protocol == PROTOCOL_UDP) {
        channel->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    } else {
        channel->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    }

    if (channel->sockfd < 0) {
        printf("Failed to create socket\n");
        return -1;
    }

    int opt = 1;
    if (setsockopt(channel->sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        printf("Failed to set socket options\n");
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (is_server) {
        addr.sin_addr.s_addr = INADDR_ANY;
        if (bind(channel->sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            printf("Failed to bind socket to %s:%d\n", ip, port);
            close(channel->sockfd);
            return -1;
        }
        printf("Server bound to %s:%d\n", ip, port);

        if (protocol == PROTOCOL_TCP) {
            if (listen(channel->sockfd, MAX_CLIENTS) < 0) {
                printf("Failed to listen on socket\n");
                close(channel->sockfd);
                return -1;
            }
            printf("Server listening on %s:%d\n", ip, port);
        }
    } else {
        addr.sin_addr.s_addr = inet_addr(ip);
        channel->server_addr = addr;

        if (protocol == PROTOCOL_UDP) {
            if (connect(channel->sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                printf("Failed to connect UDP socket to %s:%d\n", ip, port);
                close(channel->sockfd);
                return -1;
            }
            printf("UDP client connected to %s:%d\n", ip, port);
        }
    }

    network_set_nonblocking(channel->sockfd);
    FD_ZERO(&channel->read_fds);
    FD_SET(channel->sockfd, &channel->read_fds);
    channel->max_fd = channel->sockfd;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        channel->client_fds[i] = -1;
    }

    return 0;
}

int network_send_data(network_channel_t* channel, uint8_t* data, uint16_t length) {
    if (!channel || !data || length == 0) {
        return -1;
    }

    if (channel->protocol == PROTOCOL_UDP) {
        if (channel->is_server) {
            printf("UDP Server broadcast mode - data sent (broadcast not supported, use client fds)\n");
            return length;
        } else {
            int sent = send(channel->sockfd, data, length, 0);
            if (sent < 0) {
                printf("Failed to send data via UDP\n");
                return -1;
            }
            return sent;
        }
    } else {
        printf("TCP send - need to specify client fd for TCP\n");
        return -1;
    }
}

int network_send_to_client(network_channel_t* channel, int client_fd, uint8_t* data, uint16_t length) {
    if (!channel || !data || length == 0 || client_fd < 0) {
        return -1;
    }

    int sent = send(client_fd, data, length, 0);
    if (sent < 0) {
        printf("Failed to send data to client fd %d\n", client_fd);
        return -1;
    }
    return sent;
}

int network_receive_data(network_channel_t* channel, uint8_t* data, uint16_t max_length, int* client_fd) {
    if (!channel || !data) {
        return -1;
    }

    fd_set read_fds;
    struct timeval tv;

    tv.tv_sec = 0;
    tv.tv_usec = 10000;

    read_fds = channel->read_fds;

    int activity = select(channel->max_fd + 1, &read_fds, NULL, NULL, &tv);

    if (activity < 0) {
        return 0;
    }

    if (activity == 0) {
        return 0;
    }

    if (FD_ISSET(channel->sockfd, &read_fds)) {
        if (channel->protocol == PROTOCOL_UDP) {
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            int received = recvfrom(channel->sockfd, data, max_length, 0, (struct sockaddr*)&client_addr, &addr_len);
            if (received > 0) {
                printf("UDP received %d bytes from %s:%d\n", received, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                if (client_fd) {
                    *client_fd = -1;
                }
                return received;
            }
            return 0;
        } else {
            if (channel->is_server) {
                int new_fd = accept(channel->sockfd, NULL, NULL);
                if (new_fd >= 0) {
                    network_set_nonblocking(new_fd);
                    printf("New TCP connection accepted, fd: %d\n", new_fd);
                    if (channel->client_count < MAX_CLIENTS) {
                        channel->client_fds[channel->client_count++] = new_fd;
                        FD_SET(new_fd, &channel->read_fds);
                        if (new_fd > channel->max_fd) {
                            channel->max_fd = new_fd;
                        }
                    } else {
                        printf("Max clients reached, rejecting connection\n");
                        close(new_fd);
                    }
                    return 0;
                }
            } else {
                uint8_t temp_buffer[MAX_DATA_SIZE];
                int received = recv(channel->sockfd, temp_buffer, sizeof(temp_buffer), 0);
                if (received > 0) {
                    if (received <= max_length) {
                        memcpy(data, temp_buffer, received);
                    }
                    if (client_fd) {
                        *client_fd = -1;
                    }
                    return received;
                } else if (received == 0) {
                    printf("Server closed connection\n");
                    return -1;
                }
                return 0;
            }
        }
    }

    if (channel->is_server && channel->protocol == PROTOCOL_TCP) {
        for (int i = 0; i < channel->client_count; i++) {
            int fd = channel->client_fds[i];
            if (fd >= 0 && FD_ISSET(fd, &read_fds)) {
                uint8_t temp_buffer[MAX_DATA_SIZE];
                int received = recv(fd, temp_buffer, sizeof(temp_buffer), 0);
                if (received > 0) {
                    if (received <= max_length) {
                        memcpy(data, temp_buffer, received);
                    }
                    if (client_fd) {
                        *client_fd = fd;
                    }
                    return received;
                } else if (received == 0) {
                    printf("Client fd %d disconnected\n", fd);
                    close(fd);
                    FD_CLR(fd, &channel->read_fds);
                    channel->client_fds[i] = -1;
                    return 0;
                }
            }
        }
    }

    return 0;
}

int network_accept_client(network_channel_t* channel) {
    if (!channel || channel->protocol != PROTOCOL_TCP || !channel->is_server) {
        return -1;
    }

    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int new_fd = accept(channel->sockfd, (struct sockaddr*)&client_addr, &addr_len);

    if (new_fd >= 0) {
        printf("New connection from %s:%d, fd: %d\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), new_fd);
        network_set_nonblocking(new_fd);
        if (channel->client_count < MAX_CLIENTS) {
            channel->client_fds[channel->client_count++] = new_fd;
            FD_SET(new_fd, &channel->read_fds);
            if (new_fd > channel->max_fd) {
                channel->max_fd = new_fd;
            }
        } else {
            printf("Max clients reached\n");
            close(new_fd);
            new_fd = -1;
        }
    }

    return new_fd;
}

int network_close_client(network_channel_t* channel, int client_fd) {
    if (!channel || client_fd < 0) {
        return -1;
    }

    for (int i = 0; i < channel->client_count; i++) {
        if (channel->client_fds[i] == client_fd) {
            close(client_fd);
            FD_CLR(client_fd, &channel->read_fds);
            channel->client_fds[i] = -1;
            printf("Closed client fd %d\n", client_fd);
            return 0;
        }
    }

    return -1;
}

void network_close_channel(network_channel_t* channel) {
    if (!channel) {
        return;
    }

    if (channel->is_server && channel->protocol == PROTOCOL_TCP) {
        for (int i = 0; i < channel->client_count; i++) {
            if (channel->client_fds[i] >= 0) {
                close(channel->client_fds[i]);
            }
        }
    }

    if (channel->sockfd >= 0) {
        close(channel->sockfd);
    }

    memset(channel, 0, sizeof(network_channel_t));
}

int network_set_nonblocking(int sockfd) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags < 0) {
        return -1;
    }
    return fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
}
