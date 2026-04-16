#!/usr/bin/env python3
import subprocess
import time
import os
import signal

def run_with_timeout(command, timeout=5):
    """
    运行命令并在超时后杀掉进程
    
    Args:
        command: 要运行的命令（列表形式）
        timeout: 超时时间（秒）
    """
    print(f"启动命令: {' '.join(command)}")
    print(f"设置超时: {timeout}秒")
    
    # 启动进程
    process = subprocess.Popen(
        command,
        text=True
    )
    
    try:
        # 等待进程完成或超时
        process.communicate(timeout=timeout)
        
        # 进程正常完成
        print(f"\n进程退出码: {process.returncode}")
        
    except subprocess.TimeoutExpired:
        # 超时，杀掉进程
        print(f"\n=== 超时！进程运行超过 {timeout} 秒 ===")
        print("杀掉进程...")
        
        # 杀掉进程组（包括所有子进程）
        try:
            os.killpg(os.getpgid(process.pid), signal.SIGTERM)
            time.sleep(1)  # 给进程一些时间来终止
            
            # 如果进程还在运行，强制杀掉
            if process.poll() is None:
                os.killpg(os.getpgid(process.pid), signal.SIGKILL)
                print("进程已被强制杀掉")
            else:
                print("进程已被终止")
        except Exception as e:
            print(f"杀掉进程时出错: {e}")

if __name__ == "__main__":
    # 运行 thread_example
    executable = "./thread_example"
    
    # 检查文件是否存在
    if not os.path.exists(executable):
        print(f"错误: 找不到可执行文件 {executable}")
        exit(1)
    
    # 确保文件可执行
    if not os.access(executable, os.X_OK):
        print(f"错误: 文件 {executable} 没有执行权限")
        exit(1)
    
    # 运行命令
    run_with_timeout([executable])
