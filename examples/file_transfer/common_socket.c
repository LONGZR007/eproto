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

#include "common_socket.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

static int server_socket = -1;
static int client_socket = -1;

uint8_t* g_received_file_data = NULL;
uint32_t g_received_file_size = 0;
pthread_mutex_t g_file_mutex = PTHREAD_MUTEX_INITIALIZER;
int g_transfer_complete = 0;

void* mock_malloc(size_t size) {
    return malloc(size);
}

void mock_free(void* ptr) {
    free(ptr);
}

uint32_t mock_get_timestamp(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

void mock_lock(void) {
}

void mock_unlock(void) {
}

void mock_wakeup(void) {
}

void mock_status_callback(eproto_bus_t* bus, eproto_status_t status, uint8_t* data, uint16_t length) {
    (void)bus;
    (void)data;
    (void)length;
    printf("[Status] status = %d\n", status);
}

eproto_signal_result_t device_signal_wait(uint32_t timestamp) {
    (void)timestamp;
    return EPROTO_SIGNAL_DATA;
}

void device_signal_send(void) {
}

int setup_server_socket(void) {
    struct sockaddr_un addr;

    server_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("socket");
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    unlink(SOCKET_PATH);

    if (bind(server_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_socket);
        server_socket = -1;
        return -1;
    }

    if (listen(server_socket, 1) < 0) {
        perror("listen");
        close(server_socket);
        server_socket = -1;
        return -1;
    }

    printf("[Server] Waiting for connection...\n");

    client_socket = accept(server_socket, NULL, NULL);
    if (client_socket < 0) {
        perror("accept");
        close(server_socket);
        server_socket = -1;
        return -1;
    }

    printf("[Server] Connected!\n");
    return 0;
}

int setup_client_socket(void) {
    struct sockaddr_un addr;

    client_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (client_socket < 0) {
        perror("socket");
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    printf("[Client] Connecting...\n");

    int retry_count = 0;
    while (retry_count < 10) {
        if (connect(client_socket, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            printf("[Client] Connected!\n");
            return 0;
        }
        retry_count++;
        usleep(500000);
    }

    perror("connect");
    close(client_socket);
    client_socket = -1;
    return -1;
}

void close_sockets(void) {
    if (client_socket >= 0) {
        close(client_socket);
        client_socket = -1;
    }
    if (server_socket >= 0) {
        close(server_socket);
        server_socket = -1;
        unlink(SOCKET_PATH);
    }
}

void server_bus_send(eproto_bus_t* bus, uint8_t* data, uint16_t length) {
    (void)bus;
    if (client_socket < 0) {
        printf("[Server] Not connected!\n");
        return;
    }

    ssize_t sent = send(client_socket, data, length, 0);
    if (sent < 0) {
        perror("send");
    } else {
        printf("[Server] Sent %zd bytes\n", sent);
    }
}

uint16_t server_bus_receive(uint8_t* buffer, uint16_t size) {
    if (client_socket < 0) {
        return 0;
    }

    ssize_t received = recv(client_socket, buffer, size, 0);
    if (received > 0) {
        printf("[Server] Received %zd bytes\n", received);
        return (uint16_t)received;
    } else if (received == 0) {
        printf("[Server] Connection closed\n");
    } else {
        perror("recv");
    }
    return 0;
}

void client_bus_send(eproto_bus_t* bus, uint8_t* data, uint16_t length) {
    (void)bus;
    if (client_socket < 0) {
        printf("[Client] Not connected!\n");
        return;
    }

    ssize_t sent = send(client_socket, data, length, 0);
    if (sent < 0) {
        perror("send");
    } else {
        printf("[Client] Sent %zd bytes\n", sent);
    }
}

uint16_t client_bus_receive(uint8_t* buffer, uint16_t size) {
    if (client_socket < 0) {
        return 0;
    }

    ssize_t received = recv(client_socket, buffer, size, 0);
    if (received > 0) {
        printf("[Client] Received %zd bytes\n", received);
        return (uint16_t)received;
    } else if (received == 0) {
        printf("[Client] Connection closed\n");
    } else {
        perror("recv");
    }
    return 0;
}
