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

#include "common.h"
#include <time.h>

extern uint8_t* g_received_file_data;
extern uint32_t g_received_file_size;
extern pthread_mutex_t g_file_mutex;
extern int g_transfer_complete;

extern uint8_t g_test_file_data[512];
extern uint32_t g_test_file_size;

extern void* device1_thread(void* arg);
extern void* device2_thread(void* arg);

static int verify_received_data(void) {
    pthread_mutex_lock(&g_file_mutex);

    printf("\n=== Verifying Received Data ===\n");
    printf("Expected size: %u, Received size: %u\n", g_test_file_size, g_received_file_size);

    if (g_received_file_size != g_test_file_size) {
        printf("FAILED: Size mismatch!\n");
        pthread_mutex_unlock(&g_file_mutex);
        return 0;
    }

    int mismatch_count = 0;
    for (uint32_t i = 0; i < g_received_file_size && i < 64; i++) {
        if (g_received_file_data[i] != g_test_file_data[i]) {
            if (mismatch_count < 10) {
                printf("Mismatch at offset %u: expected 0x%02X, got 0x%02X\n",
                       i, g_test_file_data[i], g_received_file_data[i]);
            }
            mismatch_count++;
        }
    }

    pthread_mutex_unlock(&g_file_mutex);

    if (mismatch_count == 0) {
        printf("SUCCESS: All %u bytes match!\n", g_received_file_size);
        return 1;
    } else {
        printf("FAILED: %d bytes mismatch!\n", mismatch_count);
        return 0;
    }
}

int main(void) {
    printf("=============================================\n");
    printf("  eProto File Transfer Demo\n");
    printf("  Upper Protocol over eProto\n");
    printf("=============================================\n\n");
    fflush(stdout);

    srand((unsigned int)time(NULL));

    thread_data_t* device1_data = (thread_data_t*)malloc(sizeof(thread_data_t));
    if (!device1_data) {
        printf("Failed to allocate memory for device 1 data\n");
        return 1;
    }
    memset(device1_data, 0, sizeof(thread_data_t));
    device1_data->device_address = 0x01;
    device1_data->device_name = "Device 1";

    thread_data_t* device2_data = (thread_data_t*)malloc(sizeof(thread_data_t));
    if (!device2_data) {
        printf("Failed to allocate memory for device 2 data\n");
        free(device1_data);
        return 1;
    }
    memset(device2_data, 0, sizeof(thread_data_t));
    device2_data->device_address = 0x02;
    device2_data->device_name = "Device 2";

    pthread_t thread1, thread2;

    printf("Starting threads...\n\n");
    fflush(stdout);

    if (pthread_create(&thread1, NULL, device1_thread, device1_data) != 0) {
        printf("Failed to create device 1 thread\n");
        free(device1_data);
        free(device2_data);
        return 1;
    }

    usleep(10000);

    if (pthread_create(&thread2, NULL, device2_thread, device2_data) != 0) {
        printf("Failed to create device 2 thread\n");
        free(device1_data);
        free(device2_data);
        return 1;
    }

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    free(device1_data);
    free(device2_data);

    int result = verify_received_data();

    pthread_mutex_lock(&g_file_mutex);
    if (g_received_file_data) {
        free(g_received_file_data);
        g_received_file_data = NULL;
    }
    pthread_mutex_unlock(&g_file_mutex);

    printf("\n=============================================\n");
    if (result) {
        printf("  TEST PASSED!\n");
    } else {
        printf("  TEST FAILED!\n");
    }
    printf("=============================================\n");

    return result ? 0 : 1;
}
