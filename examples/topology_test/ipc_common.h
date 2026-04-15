#ifndef IPC_COMMON_H
#define IPC_COMMON_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#define FIFO_PATH_A_TO_B "/tmp/eproto_a_to_b.fifo"
#define FIFO_PATH_B_TO_A "/tmp/eproto_b_to_a.fifo"
#define FIFO_PATH_A_TO_D "/tmp/eproto_a_to_d.fifo"
#define FIFO_PATH_D_TO_A "/tmp/eproto_d_to_a.fifo"
#define FIFO_PATH_B_TO_C "/tmp/eproto_b_to_c.fifo"
#define FIFO_PATH_C_TO_B "/tmp/eproto_c_to_b.fifo"
#define FIFO_PATH_B_TO_E "/tmp/eproto_b_to_e.fifo"
#define FIFO_PATH_E_TO_B "/tmp/eproto_e_to_b.fifo"
#define FIFO_PATH_C_TO_D "/tmp/eproto_c_to_d.fifo"
#define FIFO_PATH_D_TO_C "/tmp/eproto_d_to_c.fifo"
#define FIFO_PATH_D_TO_E "/tmp/eproto_d_to_e.fifo"
#define FIFO_PATH_E_TO_D "/tmp/eproto_e_to_d.fifo"

#define MAX_DATA_SIZE 1024

typedef struct {
    int fd;                 // FIFO文件描述符
    const char* fifo_path;  // FIFO路径
} ipc_channel_t;

void ipc_init_channel(ipc_channel_t* channel);
int ipc_open_fifo(ipc_channel_t* channel, const char* fifo_path);
int ipc_send_data(ipc_channel_t* channel, uint8_t* data, uint16_t length);
int ipc_receive_data(ipc_channel_t* channel, uint8_t* data, uint16_t max_length);
void ipc_close_channel(ipc_channel_t* channel);
void ipc_cleanup_fifos(void);

#endif  // IPC_COMMON_H
