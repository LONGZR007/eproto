#!/usr/bin/env python3
import os
import subprocess
import time

# 测试进程启动和FIFO打开的脚本

def test_process_startup():
    """测试进程启动和FIFO打开"""
    print("=== 测试进程启动和FIFO打开 ===")
    
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
    
    # 测试进程B启动
    print("\n=== 启动进程B ===")
    process_b = subprocess.Popen(
        ["./process_b"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        preexec_fn=os.setsid  # 创建新的会话，避免受控制终端影响
    )
    
    # 等待进程启动
    time.sleep(3)
    
    # 检查进程状态
    if process_b.poll() is not None:
        print("进程B启动失败，退出码:", process_b.returncode)
        stdout, stderr = process_b.communicate()
        print("输出:", stdout)
        print("错误:", stderr)
    else:
        print("进程B启动成功，正在运行")
    
    # 测试进程A启动
    print("\n=== 启动进程A ===")
    process_a = subprocess.Popen(
        ["./process_a"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        preexec_fn=os.setsid
    )
    
    # 等待进程启动
    time.sleep(3)
    
    # 检查进程状态
    if process_a.poll() is not None:
        print("进程A启动失败，退出码:", process_a.returncode)
        stdout, stderr = process_a.communicate()
        print("输出:", stdout)
        print("错误:", stderr)
    else:
        print("进程A启动成功，正在运行")
    
    # 测试进程C启动
    print("\n=== 启动进程C ===")
    process_c = subprocess.Popen(
        ["./process_c"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        preexec_fn=os.setsid
    )
    
    # 等待进程启动
    time.sleep(3)
    
    # 检查进程状态
    if process_c.poll() is not None:
        print("进程C启动失败，退出码:", process_c.returncode)
        stdout, stderr = process_c.communicate()
        print("输出:", stdout)
        print("错误:", stderr)
    else:
        print("进程C启动成功，正在运行")
    
    # 检查FIFO文件是否存在
    print("\n=== 检查FIFO文件 ===")
    for fifo in fifo_files:
        if os.path.exists(fifo):
            print(f"FIFO文件存在: {fifo}")
        else:
            print(f"FIFO文件不存在: {fifo}")
    
    # 停止所有进程
    print("\n=== 停止所有进程 ===")
    for process in [process_a, process_b, process_c]:
        if process.poll() is None:
            os.killpg(os.getpgid(process.pid), 15)  # 发送SIGTERM到进程组
            time.sleep(1)
    
    # 清理FIFO文件
    print("\n=== 清理FIFO文件 ===")
    for fifo in fifo_files:
        if os.path.exists(fifo):
            os.unlink(fifo)
            print(f"清理FIFO文件: {fifo}")
    
    print("\n=== 测试完成 ===")

if __name__ == "__main__":
    test_process_startup()
