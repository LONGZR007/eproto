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

#include "serial_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>

void serial_init_channel(serial_channel_t *channel) {
    if (channel) {
        channel->fd = -1;
        channel->port[0] = '\0';
        channel->baud_rate = 0;
    }
}

static speed_t get_baud_rate(int baud_rate) {
    switch (baud_rate) {
        case 9600:
            return B9600;
        case 19200:
            return B19200;
        case 38400:
            return B38400;
        case 57600:
            return B57600;
        case 115200:
            return B115200;
        default:
            return B9600;
    }
}

int serial_open(serial_channel_t *channel, const char *port, int baud_rate) {
    if (!channel || !port) {
        return -1;
    }

    channel->fd = open(port, O_RDWR | O_NOCTTY | O_NDELAY);
    if (channel->fd < 0) {
        perror("Failed to open serial port");
        return -1;
    }

    struct termios options;
    if (tcgetattr(channel->fd, &options) < 0) {
        perror("Failed to get serial port attributes");
        close(channel->fd);
        channel->fd = -1;
        return -1;
    }

    speed_t speed = get_baud_rate(baud_rate);
    cfsetispeed(&options, speed);
    cfsetospeed(&options, speed);

    options.c_cflag |= (CLOCAL | CREAD);
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CRTSCTS;

    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_iflag &= ~(IXON | IXOFF | IXANY);
    options.c_oflag &= ~OPOST;

    options.c_cc[VMIN] = 1;
    options.c_cc[VTIME] = 0;

    if (tcsetattr(channel->fd, TCSANOW, &options) < 0) {
        perror("Failed to set serial port attributes");
        close(channel->fd);
        channel->fd = -1;
        return -1;
    }

    strncpy(channel->port, port, sizeof(channel->port) - 1);
    channel->port[sizeof(channel->port) - 1] = '\0';
    channel->baud_rate = baud_rate;

    return 0;
}

void serial_close(serial_channel_t *channel) {
    if (channel && channel->fd >= 0) {
        close(channel->fd);
        channel->fd = -1;
    }
}

int serial_send_data(serial_channel_t *channel, uint8_t *data, uint16_t length) {
    if (!channel || !data || channel->fd < 0) {
        return -1;
    }

    int bytes_sent = write(channel->fd, data, length);
    if (bytes_sent < 0) {
        perror("Failed to send data");
        return -1;
    }

    return bytes_sent;
}

int serial_receive_data(serial_channel_t *channel, uint8_t *buffer, uint16_t buffer_size) {
    if (!channel || !buffer || channel->fd < 0) {
        return -1;
    }

    int bytes_received = read(channel->fd, buffer, buffer_size);
    if (bytes_received < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("Failed to receive data");
        }
        return 0;
    }

    return bytes_received;
}

int serial_parse_args(int argc, char *argv[], char *port, int *baud_rate) {
    if (!port || !baud_rate) {
        return -1;
    }

    if (argc < 3) {
        fprintf(stderr, "Usage: %s <serial_port> <baud_rate>\n", argv[0]);
        return -1;
    }

    strncpy(port, argv[1], 64);
    *baud_rate = atoi(argv[2]);

    return 0;
}