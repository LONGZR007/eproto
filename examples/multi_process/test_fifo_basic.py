#!/usr/bin/env python3
import os
import time

# 测试FIFO基本功能的脚本

def test_fifo_creation():
    """测试FIFO文件创建和打开"""
    print("=== 测试FIFO基本功能 ===")
    
    # 定义FIFO路径
    fifo_files = [
        "/tmp/eproto_a_to_b.fifo",
        "/tmp/eproto_b_to_a.fifo",
        "/tmp/eproto_b_to_c.fifo",
        "/tmp/eproto_c_to_b.fifo"
    ]
    
    # 清理之前的FIFO文件
    for fifo in fifo_files:
        if os.path.exists(fifo):
            os.unlink(fifo)
            print(f"删除旧的FIFO文件: {fifo}")
    
    # 测试FIFO创建
    print("\n=== 测试FIFO创建 ===")
    for fifo in fifo_files:
        try:
            os.mkfifo(fifo, 0o666)
            print(f"成功创建FIFO: {fifo}")
        except OSError as e:
            print(f"创建FIFO {fifo} 失败: {e}")
    
    # 检查FIFO文件是否存在
    print("\n=== 检查FIFO文件 ===")
    for fifo in fifo_files:
        if os.path.exists(fifo):
            print(f"FIFO文件存在: {fifo}")
            print(f"文件类型: {os.stat(fifo).st_mode}")
        else:
            print(f"FIFO文件不存在: {fifo}")
    
    # 测试FIFO打开（非阻塞模式）
    print("\n=== 测试FIFO打开 ===")
    import fcntl
    for fifo in fifo_files:
        try:
            fd = os.open(fifo, os.O_RDWR | os.O_NONBLOCK)
            print(f"成功打开FIFO: {fifo}")
            os.close(fd)
        except OSError as e:
            print(f"打开FIFO {fifo} 失败: {e}")
    
    # 清理FIFO文件
    print("\n=== 清理FIFO文件 ===")
    for fifo in fifo_files:
        if os.path.exists(fifo):
            os.unlink(fifo)
            print(f"清理FIFO文件: {fifo}")
    
    print("\n=== 测试完成 ===")

if __name__ == "__main__":
    test_fifo_creation()
