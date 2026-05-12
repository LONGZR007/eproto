CC = gcc
CFLAGS = -Wall -Wextra -g -I.

SRC_DIR = ../../src
INC_DIR = ../../inc

BUILD_DIR = build
BINDIR = bin

EPROTO_SRCS = eproto.c eproto_crc16.c eproto_packet_node.c eproto_ring_buffer.c eproto_frame_parser.c
EPROTO_OBJS = $(addprefix $(BUILD_DIR)/, $(EPROTO_SRCS:.c=.o))

LOCAL_SRCS = network_common.c
LOCAL_OBJS = $(addprefix $(BUILD_DIR)/, $(LOCAL_SRCS:.c=.o))

TARGETS = master_m1 slave_s2 slave_s3 slave_s4
BIN_TARGETS = $(addprefix $(BINDIR)/, $(TARGETS))

all: $(BUILD_DIR) $(BINDIR) $(BIN_TARGETS)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BINDIR):
	@mkdir -p $(BINDIR)

$(BINDIR)/master_m1: master_m1.c $(EPROTO_OBJS) $(LOCAL_OBJS)
	$(CC) $(CFLAGS) -I$(INC_DIR) -I.. -o $@ $^ -pthread

$(BINDIR)/slave_s2: slave.c $(EPROTO_OBJS) $(LOCAL_OBJS)
	$(CC) $(CFLAGS) -I$(INC_DIR) -I.. -DSLAVE_ADDR=2 -o $@ $^ -pthread

$(BINDIR)/slave_s3: slave.c $(EPROTO_OBJS) $(LOCAL_OBJS)
	$(CC) $(CFLAGS) -I$(INC_DIR) -I.. -DSLAVE_ADDR=3 -o $@ $^ -pthread

$(BINDIR)/slave_s4: slave.c $(EPROTO_OBJS) $(LOCAL_OBJS)
	$(CC) $(CFLAGS) -I$(INC_DIR) -I.. -DSLAVE_ADDR=4 -o $@ $^ -pthread

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -I$(INC_DIR) -I.. -c $< -o $@

$(BUILD_DIR)/%.o: %.c
	$(CC) $(CFLAGS) -I$(INC_DIR) -I.. -c $< -o $@

clean:
	@rm -rf $(BUILD_DIR) $(BINDIR)
