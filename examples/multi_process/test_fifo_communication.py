#!/usr/bin/env python3
import os
import subprocess
import time
import signal

# 测试FIFO通信的脚本

def run_process(name, cmd):
    """启动进程并返回进程对象"""
    print(f"启动 {name}...")
    process = subprocess.Popen(
        cmd,
        shell=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )
    # 等待进程启动
    time.sleep(2)
    return process

def read_output(process, name, timeout=10):
    """读取进程输出"""
    try:
        stdout, stderr = process.communicate(timeout=timeout)
        print(f"{name} 输出:")
        print(stdout)
        if stderr:
            print(f"{name} 错误:")
            print(stderr)
    except subprocess.TimeoutExpired:
        print(f"{name} 运行超时")

def test_communication():
    """测试进程间通信"""
    print("=== 测试FIFO进程间通信 ===")
    
    # 清理之前的FIFO文件
    fifo_files = [
        "/tmp/eproto_a_to_b.fifo",
        "/tmp/eproto_b_to_a.fifo",
        "/tmp/eproto_b_to_c.fifo",
        "/tmp/eproto_c_to_b.fifo"
    ]
    for fifo in fifo_files:
        if os.path.exists(fifo):
            os.unlink(fifo)
            print(f"删除旧的FIFO文件: {fifo}")
    
    # 启动进程B（中间节点）
    process_b = run_process("进程B", "./process_b")
    
    # 启动进程A
    process_a = run_process("进程A", "./process_a")
    
    # 启动进程C
    process_c = run_process("进程C", "./process_c")
    
    # 等待所有进程启动完成
    time.sleep(3)
    
    # 检查进程状态
    for name, process in [("A", process_a), ("B", process_b), ("C", process_c)]:
        if process.poll() is not None:
            print(f"警告: 进程{name}已退出，退出码: {process.returncode}")
            read_output(process, f"进程{name}")
    
    # 测试进程A发送消息到进程C
    print("\n=== 测试A → C 通信 ===")
    # 向进程A发送命令
    process_a.stdin = open(process_a.stdin.fileno(), 'w')
    process_a.stdin.write("send 3 1 11 22 33\n")
    process_a.stdin.flush()
    
    # 等待消息传递
    time.sleep(5)
    
    # 测试进程C发送回复
    print("\n=== 测试C → A 回复 ===")
    # 向进程C发送命令
    process_c.stdin = open(process_c.stdin.fileno(), 'w')
    process_c.stdin.write("send_reply AA BB CC\n")
    process_c.stdin.flush()
    
    # 等待回复传递
    time.sleep(5)
    
    # 停止所有进程
    print("\n=== 停止所有进程 ===")
    for process in [process_a, process_b, process_c]:
        if process.poll() is None:
            process.send_signal(signal.SIGINT)
            time.sleep(1)
    
    # 读取所有进程的输出
    read_output(process_a, "进程A")
    read_output(process_b, "进程B")
    read_output(process_c, "进程C")
    
    # 清理FIFO文件
    for fifo in fifo_files:
        if os.path.exists(fifo):
            os.unlink(fifo)
            print(f"清理FIFO文件: {fifo}")
    
    print("\n=== 测试完成 ===")

if __name__ == "__main__":
    test_communication()
