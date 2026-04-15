#!/usr/bin/env python3
import os
import sys
import time
import subprocess
import threading
import signal
from queue import Queue, Empty

# 清理FIFO文件
def cleanup_fifos():
    fifo_files = [
        "/tmp/eproto_a_to_b.fifo",
        "/tmp/eproto_b_to_a.fifo",
        "/tmp/eproto_b_to_c.fifo",
        "/tmp/eproto_c_to_b.fifo"
    ]
    for fifo in fifo_files:
        if os.path.exists(fifo):
            try:
                os.unlink(fifo)
                print(f"已清理FIFO: {fifo}")
            except Exception as e:
                print(f"清理FIFO {fifo} 时出错: {e}")

# 进程管理器
class ProcessManager:
    def __init__(self, name, cmd):
        self.name = name
        self.cmd = cmd
        self.process = None
        self.stdout_queue = Queue()
        self.stderr_queue = Queue()
        self.running = False
        self.stdout_thread = None
        self.stderr_thread = None
    
    def start(self):
        print(f"\n{'='*60}")
        print(f"启动 {self.name}...")
        print(f"{'='*60}")
        
        # 确保可执行文件存在
        if not os.path.exists(self.cmd[0]):
            print(f"错误: 找不到可执行文件 {self.cmd[0]}")
            return False
        
        try:
            self.process = subprocess.Popen(
                self.cmd,
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                bufsize=1,
                universal_newlines=True
            )
            self.running = True
            
            # 启动线程读取输出
            self.stdout_thread = threading.Thread(target=self._read_stream, 
                                                  args=(self.process.stdout, self.stdout_queue, "STDOUT"), 
                                                  daemon=True)
            self.stderr_thread = threading.Thread(target=self._read_stream, 
                                                  args=(self.process.stderr, self.stderr_queue, "STDERR"), 
                                                  daemon=True)
            self.stdout_thread.start()
            self.stderr_thread.start()
            
            # 等待进程启动
            time.sleep(2)
            
            # 检查进程是否还在运行
            if self.process.poll() is not None:
                print(f"{self.name} 启动后立即退出，退出码: {self.process.returncode}")
                self._print_output()
                return False
            
            print(f"{self.name} 启动成功！")
            return True
        except Exception as e:
            print(f"启动 {self.name} 时出错: {e}")
            return False
    
    def _read_stream(self, stream, queue, stream_name):
        while self.running:
            try:
                line = stream.readline()
                if line:
                    queue.put((time.time(), line.rstrip()))
                else:
                    break
            except Exception as e:
                print(f"读取 {self.name} {stream_name} 时出错: {e}")
                break
    
    def send_command(self, command):
        if not self.running or not self.process:
            print(f"{self.name} 未运行，无法发送命令")
            return False
        
        try:
            print(f"\n>>> 向 {self.name} 发送命令: {command}")
            self.process.stdin.write(command + '\n')
            self.process.stdin.flush()
            return True
        except Exception as e:
            print(f"向 {self.name} 发送命令时出错: {e}")
            return False
    
    def _print_output(self):
        print(f"\n--- {self.name} 输出 ---")
        while not self.stdout_queue.empty():
            try:
                ts, line = self.stdout_queue.get_nowait()
                print(f"[{self.name}] {line}")
            except Empty:
                break
        
        print(f"\n--- {self.name} 错误输出 ---")
        while not self.stderr_queue.empty():
            try:
                ts, line = self.stderr_queue.get_nowait()
                print(f"[{self.name} (ERROR)] {line}")
            except Empty:
                break
    
    def wait_and_print(self, seconds=3):
        print(f"\n等待 {seconds} 秒...")
        time.sleep(seconds)
        self._print_output()
    
    def stop(self):
        if self.process and self.running:
            print(f"\n停止 {self.name}...")
            try:
                self.running = False
                self.process.terminate()
                try:
                    self.process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    self.process.kill()
                    self.process.wait()
                print(f"{self.name} 已停止")
            except Exception as e:
                print(f"停止 {self.name} 时出错: {e}")
        
        self._print_output()

def main():
    print("="*60)
    print("eProto 多进程 FIFO 通信测试")
    print("="*60)
    
    # 切换到脚本所在目录
    script_dir = os.path.dirname(os.path.abspath(__file__))
    os.chdir(script_dir)
    
    # 清理FIFO文件
    print("\n清理旧的FIFO文件...")
    cleanup_fifos()
    
    # 创建进程管理器
    process_b = ProcessManager("进程B", ["./process_b"])
    process_a = ProcessManager("进程A", ["./process_a"])
    process_c = ProcessManager("进程C", ["./process_c"])
    
    try:
        # 先启动进程B（中间节点）
        if not process_b.start():
            print("进程B启动失败，测试终止")
            return
        
        process_b.wait_and_print(2)
        
        # 启动进程A
        if not process_a.start():
            print("进程A启动失败，测试终止")
            return
        
        process_a.wait_and_print(2)
        
        # 启动进程C
        if not process_c.start():
            print("进程C启动失败，测试终止")
            return
        
        process_c.wait_and_print(2)
        
        # 测试1: A → B → C 发送消息
        print("\n" + "="*60)
        print("测试1: A 向 C 发送消息")
        print("="*60)
        process_a.send_command("send 3 1 11 22 33")
        process_a.wait_and_print(5)
        process_b.wait_and_print(3)
        process_c.wait_and_print(3)
        
        # 测试2: C 回复 A
        print("\n" + "="*60)
        print("测试2: C 回复 A")
        print("="*60)
        process_c.send_command("send_reply AA BB CC")
        process_c.wait_and_print(5)
        process_b.wait_and_print(3)
        process_a.wait_and_print(3)
        
        # 测试3: C 向 A 发送消息
        print("\n" + "="*60)
        print("测试3: C 向 A 发送消息")
        print("="*60)
        process_c.send_command("send 1 0 44 55 66")
        process_c.wait_and_print(5)
        process_b.wait_and_print(3)
        process_a.wait_and_print(3)
        
        print("\n" + "="*60)
        print("所有测试完成！")
        print("="*60)
        
    except KeyboardInterrupt:
        print("\n\n用户中断测试")
    finally:
        # 停止所有进程
        print("\n" + "="*60)
        print("清理资源")
        print("="*60)
        for pm in [process_a, process_b, process_c]:
            pm.stop()
        
        # 清理FIFO文件
        print("\n清理FIFO文件...")
        cleanup_fifos()
        
        print("\n测试结束")

if __name__ == "__main__":
    main()
