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

#ifndef FILE_TRANSFER_COMMON_H
#define FILE_TRANSFER_COMMON_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include <semaphore.h>
#include "../../inc/eproto.h"
#include "../../inc/eproto_upper.h"

#define SHARED_BUFFER_SIZE 4096

#define PROTOCOL_HEADER_SIZE 2
#define PROTOCOL_FLAG_ENCRYPTED 0x01
#define PROTOCOL_FLAG_NEED_REPLY 0x02

#define KEY_BUS_1_2 0x5A

typedef struct {
    uint8_t device_address;
    eproto_t eproto_inst;
    eproto_upper_context_t upper_ctx;
    char* device_name;
    uint32_t timestamp;
    pthread_mutex_t timestamp_mutex;
    sem_t semaphore;
    int semaphore_initialized;
    int signal_flag;
    uint8_t rx_buffer[512];
    uint8_t rx_buffer2[512];
} thread_data_t;

extern uint8_t g_shared_buffer1[SHARED_BUFFER_SIZE];
extern uint8_t g_shared_buffer2[SHARED_BUFFER_SIZE];
extern uint16_t g_shared_buffer1_head;
extern uint16_t g_shared_buffer1_tail;
extern uint16_t g_shared_buffer2_head;
extern uint16_t g_shared_buffer2_tail;
extern pthread_mutex_t g_mutex1;
extern pthread_mutex_t g_mutex2;
extern pthread_mutex_t g_eproto_lock;
extern __thread thread_data_t* g_current_thread_data;

extern eproto_t* g_device1_eproto;
extern eproto_t* g_device2_eproto;
extern eproto_upper_context_t* g_device1_upper_ctx;
extern eproto_upper_context_t* g_device2_upper_ctx;

void* mock_malloc(size_t size);
void mock_free(void* ptr);
uint32_t mock_get_timestamp(void);
void mock_lock(void);
void mock_unlock(void);
void mock_wakeup(void);
void mock_status_callback(eproto_bus_t* bus, eproto_status_t status, uint8_t* data, uint16_t length);

eproto_signal_result_t device_signal_wait(uint32_t timestamp);
void device_signal_send(void);

void device1_bus_send(eproto_bus_t* bus, uint8_t* data, uint16_t length);
uint16_t device1_bus_receive(uint8_t* buffer, uint16_t size);
void device2_bus_send(eproto_bus_t* bus, uint8_t* data, uint16_t length);
uint16_t device2_bus_receive(uint8_t* buffer, uint16_t size);

void device1_receive_callback(eproto_bus_t* bus, uint8_t source_address, uint16_t packet_id, uint8_t* data, uint16_t length);
void device2_receive_callback(eproto_bus_t* bus, uint8_t source_address, uint16_t packet_id, uint8_t* data, uint16_t length);

void device_send_callback(eproto_send_status_t status, uint16_t packet_id, uint8_t* data, uint16_t length, void* private_data);

void upper_file_start_req_callback(uint16_t session_id, uint8_t status, void* user_data);
void upper_file_start_rsp_callback(uint16_t session_id, uint8_t status, void* user_data);
void upper_file_end_callback(uint16_t session_id, uint8_t status, void* user_data);
void upper_file_data_callback(uint16_t session_id, uint32_t offset, uint8_t* data, uint16_t length, void* user_data);
void upper_progress_callback(uint16_t session_id, uint32_t transferred, uint32_t total, void* user_data);

void* device1_thread(void* arg);
void* device2_thread(void* arg);

#endif
