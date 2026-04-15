#include "ipc_common.h"

void ipc_init_channel(ipc_channel_t* channel) {
    if (channel) {
        channel->fd = -1;
        channel->fifo_path = NULL;
    }
}

int ipc_open_fifo(ipc_channel_t* channel, const char* fifo_path) {
    if (!channel || !fifo_path) {
        fprintf(stderr, "Invalid arguments\n");
        return -1;
    }

    // 创建FIFO（如果不存在）
    if (mkfifo(fifo_path, 0666) < 0) {
        if (errno != EEXIST) {
            perror("mkfifo failed");
            return -1;
        }
        // FIFO已存在，继续
    }

    // 以非阻塞模式打开FIFO
    int fd = open(fifo_path, O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        perror("open fifo failed");
        return -1;
    }

    channel->fd = fd;
    channel->fifo_path = fifo_path;
    printf("FIFO %s opened successfully\n", fifo_path);
    return 0;
}

int ipc_send_data(ipc_channel_t* channel, uint8_t* data, uint16_t length) {
    if (!channel || !data || channel->fd < 0) {
        fprintf(stderr, "Invalid arguments or channel not open\n");
        return -1;
    }

    if (length > MAX_DATA_SIZE) {
        fprintf(stderr, "Data length exceeds maximum size\n");
        return -1;
    }

    ssize_t sent = write(channel->fd, data, length);
    if (sent < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("write failed");
            return -1;
        }
        // 非阻塞模式下没有可写空间，返回0
        return 0;
    }

    return (int)sent;
}

int ipc_receive_data(ipc_channel_t* channel, uint8_t* data, uint16_t max_length) {
    if (!channel || !data || channel->fd < 0) {
        fprintf(stderr, "Invalid arguments or channel not open\n");
        return -1;
    }

    if (max_length > MAX_DATA_SIZE) {
        fprintf(stderr, "Maximum length exceeds limit\n");
        return -1;
    }

    ssize_t received = read(channel->fd, data, max_length);
    if (received < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("read failed");
            return -1;
        }
        // 非阻塞模式下没有可读数据，返回0
        return 0;
    }

    return (int)received;
}

void ipc_close_channel(ipc_channel_t* channel) {
    if (channel && channel->fd >= 0) {
        close(channel->fd);
        channel->fd = -1;
        channel->fifo_path = NULL;
    }
}

void ipc_cleanup_fifos(void) {
    unlink(FIFO_PATH_A_TO_B);
    unlink(FIFO_PATH_B_TO_A);
    unlink(FIFO_PATH_A_TO_D);
    unlink(FIFO_PATH_D_TO_A);
    unlink(FIFO_PATH_B_TO_C);
    unlink(FIFO_PATH_C_TO_B);
    unlink(FIFO_PATH_B_TO_E);
    unlink(FIFO_PATH_E_TO_B);
    unlink(FIFO_PATH_C_TO_D);
    unlink(FIFO_PATH_D_TO_C);
    unlink(FIFO_PATH_D_TO_E);
    unlink(FIFO_PATH_E_TO_D);
    printf("FIFO files cleaned up\n");
}
