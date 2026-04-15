#!/usr/bin/env python3
import subprocess
import time
import os
import signal
import sys

def cleanup_sockets():
    """清理旧的socket文件"""
    sockets = [
        "/tmp/eproto_a_to_b.sock",
        "/tmp/eproto_b_to_a.sock", 
        "/tmp/eproto_b_to_c.sock",
        "/tmp/eproto_c_to_b.sock"
    ]
    for sock in sockets:
        try:
            os.unlink(sock)
        except OSError:
            pass

def main():
    print("=== eProto 多进程通信测试 ===\n")
    
    # 清理旧的socket文件
    cleanup_sockets()
    
    # 改变工作目录
    os.chdir("/workspace/eproto/examples/multi_process")
    
    processes = []
    
    try:
        # 1. 启动进程B（服务器）
        print("启动进程B...")
        proc_b = subprocess.Popen(
            ["./process_b"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1
        )
        processes.append(proc_b)
        time.sleep(2)
        
        # 2. 启动进程A
        print("启动进程A...")
        proc_a = subprocess.Popen(
            ["./process_a"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1
        )
        processes.append(proc_a)
        time.sleep(2)
        
        # 3. 启动进程C
        print("启动进程C...")
        proc_c = subprocess.Popen(
            ["./process_c"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1
        )
        processes.append(proc_c)
        time.sleep(2)
        
        print("\n=== 所有进程启动成功 ===")
        print("\n现在您可以手动测试了！")
        print("请在三个不同的终端中运行：")
        print("  终端1: ./process_b")
        print("  终端2: ./process_a")
        print("  终端3: ./process_c")
        print("\n测试命令示例：")
        print("  从A发送到B: send 2 0 11 22 33")
        print("  从A发送到C: send 3 0 44 55 66")
        print("  从B发送到C: send 3 0 AA BB CC")
        print("  从A发送需要回复的消息到C: send 3 1 DD EE FF")
        print("  从C回复A: send_reply 99 88 77")
        print("\n按Ctrl+C停止测试并清理进程...")
        
        # 等待用户中断
        while True:
            time.sleep(1)
            
    except KeyboardInterrupt:
        print("\n\n正在停止所有进程...")
    finally:
        # 清理所有进程
        for proc in processes:
            try:
                proc.terminate()
                proc.wait(timeout=2)
            except:
                try:
                    proc.kill()
                except:
                    pass
        
        # 清理socket文件
        cleanup_sockets()
        print("测试完成，已清理所有资源。")

if __name__ == "__main__":
    main()
