#!/bin/bash

# 测试拓扑多进程通信功能

echo "=== 测试 eProto 拓扑多进程通信 ==="
echo ""

# 清理之前的 FIFO 文件
echo "清理之前的 FIFO 文件..."
rm -f /tmp/eproto_*.fifo

# 启动进程 A 到后台
echo "启动进程 A..."
./process_a &
A_PID=$!
sleep 2

# 启动进程 B 到后台
echo "启动进程 B..."
./process_b &
B_PID=$!
sleep 2

# 启动进程 C 到后台
echo "启动进程 C..."
./process_c &
C_PID=$!
sleep 2

# 启动进程 D 到后台
echo "启动进程 D..."
./process_d &
D_PID=$!
sleep 2

# 启动进程 E 到后台
echo "启动进程 E..."
./process_e &
E_PID=$!
sleep 2

echo ""
echo "所有进程已启动，开始测试通信..."
echo ""

# 测试直接通信：A -> B
echo "测试 A 到 B 的直接通信..."
echo "send 2 1 11 22 33" > /proc/$A_PID/fd/0
sleep 3

# 测试直接通信：B -> C
echo "测试 B 到 C 的直接通信..."
echo "send 3 1 44 55 66" > /proc/$B_PID/fd/0
sleep 3

# 测试直接通信：C -> D
echo "测试 C 到 D 的直接通信..."
echo "send 4 1 77 88 99" > /proc/$C_PID/fd/0
sleep 3

# 测试直接通信：D -> E
echo "测试 D 到 E 的直接通信..."
echo "send 5 1 AA BB CC" > /proc/$D_PID/fd/0
sleep 3

# 测试直接通信：D -> A（直接通信）
echo "测试 D 到 A 的直接通信..."
echo "send 1 1 12 34 56" > /proc/$D_PID/fd/0
sleep 3

# 测试直接通信：A -> D（直接通信）
echo "测试 A 到 D 的直接通信..."
echo "send 4 1 78 9A BC" > /proc/$A_PID/fd/0
sleep 3

# 测试转发通信：A -> E（通过 B 转发）
echo "测试 A 到 E 的转发通信..."
echo "send 5 1 DD EE FF" > /proc/$A_PID/fd/0
sleep 3

# 测试回复功能：A -> B 并请求回复
echo "测试 A 到 B 的回复功能..."
echo "send 2 1 00 11 22" > /proc/$A_PID/fd/0
sleep 3

# B 发送回复
echo "B 发送回复..."
echo "send_reply 33 44 55" > /proc/$B_PID/fd/0
sleep 3

# 停止所有进程
echo ""
echo "测试完成，停止所有进程..."
kill $A_PID $B_PID $C_PID $D_PID $E_PID

# 清理 FIFO 文件
echo "清理 FIFO 文件..."
rm -f /tmp/eproto_*.fifo

echo ""
echo "测试结束！"
