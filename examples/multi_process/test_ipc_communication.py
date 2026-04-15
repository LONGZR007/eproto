#!/usr/bin/env python3
import os
import subprocess
import time
import threading

# 测试进程间通信的脚本

class ProcessManager:
    def __init__(self, name, cmd):
        self.name = name
        self.cmd = cmd
        self.process = None
        self.stdout = []
        self.stderr = []
        self.running = False
    
    def start(self):
        """启动进程"""
        print(f"启动 {self.name}...")
        self.process = subprocess.Popen(
            self.cmd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        self.running = True
        
        # 启动线程读取输出
        threading.Thread(target=self._read_stdout, daemon=True).start()
        threading.Thread(target=self._read_stderr, daemon=True).start()
        
        # 等待进程启动
        time.sleep(2)
    
    def _read_stdout(self):
        """读取标准输出"""
        while self.running:
            try:
                line = self.process.stdout.readline()
                if line:
                    self.stdout.append(line)
                    print(f"{self.name}: {line.rstrip()}")
            except:
                break
    
    def _read_stderr(self):
        """读取标准错误"""
        while self.running:
            try:
                line = self.process.stderr.readline()
                if line:
                    self.stderr.append(line)
                    print(f"{self.name} (错误): {line.rstrip()}")
            except:
                break
    
    def send_command(self, command):
        """发送命令"""
        if self.process and self.running:
            try:
                self.process.stdin.write(command + '\n')
                self.process.stdin.flush()
                return True
            except:
                return False
        return False
    
    def stop(self):
        """停止进程"""
        if self.process and self.running:
            try:
                self.process.terminate()
                self.process.wait(timeout=5)
            except:
                pass
            self.running = False

def test_communication():
    """测试进程间通信"""
    print("=== 测试进程间通信 ===")
    
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
    
    # 创建进程管理器
    process_b = ProcessManager("进程B", ["./process_b"])
    process_a = ProcessManager("进程A", ["./process_a"])
    process_c = ProcessManager("进程C", ["./process_c"])
    
    # 启动进程
    process_b.start()
    process_a.start()
    process_c.start()
    
    # 等待所有进程启动完成
    time.sleep(3)
    
    # 检查进程状态
    all_running = True
    for pm in [process_a, process_b, process_c]:
        if not pm.running or pm.process.poll() is not None:
            print(f"警告: {pm.name} 可能未正常运行")
            all_running = False
    
    if not all_running:
        print("部分进程未正常运行，测试终止")
        for pm in [process_a, process_b, process_c]:
            pm.stop()
        return
    
    # 测试A → C 通信
    print("\n=== 测试A → C 通信 ===")
    process_a.send_command("send 3 1 11 22 33")
    
    # 等待消息传递
    time.sleep(5)
    
    # 测试C → A 回复
    print("\n=== 测试C → A 回复 ===")
    process_c.send_command("send_reply AA BB CC")
    
    # 等待回复传递
    time.sleep(5)
    
    # 停止所有进程
    print("\n=== 停止所有进程 ===")
    for pm in [process_a, process_b, process_c]:
        pm.stop()
    
    # 清理FIFO文件
    print("\n=== 清理FIFO文件 ===")
    for fifo in fifo_files:
        if os.path.exists(fifo):
            os.unlink(fifo)
            print(f"清理FIFO文件: {fifo}")
    
    print("\n=== 测试完成 ===")

if __name__ == "__main__":
    test_communication()
