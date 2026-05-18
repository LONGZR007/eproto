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

#ifndef FILE_TRANSFER_SERIAL_H
#define FILE_TRANSFER_SERIAL_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include "../../inc/eproto.h"
#include "../../inc/eproto_upper.h"

#define SERIAL_PORT "/dev/ttyUSB0"  // 默认串口设备
#define SERIAL_BAUD B115200        // 默认波特率
#define SERIAL_TIMEOUT_MS 100

extern pthread_mutex_t g_file_mutex;
extern uint8_t* g_received_file_data;
extern uint32_t g_received_file_size;
extern int g_transfer_complete;

void* mock_malloc(size_t size);
void mock_free(void* ptr);
uint32_t mock_get_timestamp(void);
void mock_lock(void);
void mock_unlock(void);
void mock_status_callback(eproto_bus_t* bus, eproto_status_t status, uint8_t* data, uint16_t length);
eproto_signal_result_t device_signal_wait(uint32_t timestamp);
void device_signal_send(void);

int setup_serial_port(const char* port, speed_t baud);
void close_serial_port(void);

void serial_bus_send(eproto_bus_t* bus, uint8_t* data, uint16_t length);
uint16_t serial_bus_receive(uint8_t* buffer, uint16_t size);

#endif // FILE_TRANSFER_SERIAL_H
