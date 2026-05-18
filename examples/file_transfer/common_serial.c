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

#include "common_serial.h"

static int serial_fd = -1;

pthread_mutex_t g_file_mutex = PTHREAD_MUTEX_INITIALIZER;
uint8_t* g_received_file_data = NULL;
uint32_t g_received_file_size = 0;
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

int setup_serial_port(const char* port, speed_t baud) {
    struct termios tio;

    serial_fd = open(port, O_RDWR | O_NOCTTY);
    if (serial_fd < 0) {
        perror("open serial port");
        return -1;
    }

    if (tcgetattr(serial_fd, &tio) != 0) {
        perror("tcgetattr");
        close(serial_fd);
        serial_fd = -1;
        return -1;
    }

    cfmakeraw(&tio);
    cfsetspeed(&tio, baud);
    tio.c_cflag &= ~CSIZE;
    tio.c_cflag |= CS8;
    tio.c_cflag &= ~PARENB;
    tio.c_cflag &= ~CSTOPB;
    tio.c_cflag |= CREAD | CLOCAL;
    tio.c_cc[VTIME] = SERIAL_TIMEOUT_MS / 100;
    tio.c_cc[VMIN] = 0;

    if (tcsetattr(serial_fd, TCSANOW, &tio) != 0) {
        perror("tcsetattr");
        close(serial_fd);
        serial_fd = -1;
        return -1;
    }

    printf("[Serial] Port %s opened at %d baud\n", port, baud);
    return 0;
}

void close_serial_port(void) {
    if (serial_fd >= 0) {
        close(serial_fd);
        serial_fd = -1;
    }
}

void serial_bus_send(eproto_bus_t* bus, uint8_t* data, uint16_t length) {
    (void)bus;
    if (serial_fd < 0) {
        printf("[Serial] Not opened!\n");
        return;
    }

    ssize_t sent = write(serial_fd, data, length);
    if (sent < 0) {
        perror("write serial");
    } else {
        printf("[Serial] Sent %zd bytes\n", sent);
    }
}

uint16_t serial_bus_receive(uint8_t* buffer, uint16_t size) {
    if (serial_fd < 0) {
        return 0;
    }

    ssize_t received = read(serial_fd, buffer, size);
    if (received > 0) {
        printf("[Serial] Received %zd bytes\n", received);
        return (uint16_t)received;
    } else if (received == 0) {
        return 0;
    } else {
        perror("read serial");
    }
    return 0;
}
