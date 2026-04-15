#!/usr/bin/env python3
import os
import sys
import time
import subprocess
import threading
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
class SimpleProcessManager:
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
            
            self.stdout_thread = threading.Thread(target=self._read_stream, 
                                                  args=(self.process.stdout, self.stdout_queue, "STDOUT"), 
                                                  daemon=True)
            self.stderr_thread = threading.Thread(target=self._read_stream, 
                                                  args=(self.process.stderr, self.stderr_queue, "STDERR"), 
                                                  daemon=True)
            self.stdout_thread.start()
            self.stderr_thread.start()
            
            time.sleep(3)
            
            if self.process.poll() is not None:
                print(f"{self.name} 启动后立即退出，退出码: {self.process.returncode}")
                self._print_output()
                return False
            
            print(f"{self.name} 启动成功，正在运行！")
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
                    print(f"[{self.name}] {line.rstrip()}")
                else:
                    break
            except Exception as e:
                print(f"读取 {self.name} {stream_name} 时出错: {e}")
                break
    
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
    print("eProto 多进程 FIFO 测试 - 简化版")
    print("="*60)
    
    script_dir = os.path.dirname(os.path.abspath(__file__))
    os.chdir(script_dir)
    
    print("\n清理旧的FIFO文件...")
    cleanup_fifos()
    
    process_b = SimpleProcessManager("进程B", ["./process_b"])
    process_a = SimpleProcessManager("进程A", ["./process_a"])
    process_c = SimpleProcessManager("进程C", ["./process_c"])
    
    try:
        if not process_b.start():
            print("进程B启动失败，测试终止")
            return
        
        time.sleep(2)
        
        if not process_a.start():
            print("进程A启动失败，测试终止")
            return
        
        time.sleep(2)
        
        if not process_c.start():
            print("进程C启动失败，测试终止")
            return
        
        print("\n" + "="*60)
        print("所有进程已成功启动！")
        print("="*60)
        print("\n等待10秒观察进程运行状态...")
        time.sleep(10)
        
        print("\n" + "="*60)
        print("测试完成！进程运行正常。")
        print("="*60)
        
    except KeyboardInterrupt:
        print("\n\n用户中断测试")
    finally:
        print("\n" + "="*60)
        print("清理资源")
        print("="*60)
        for pm in [process_a, process_b, process_c]:
            pm.stop()
        
        print("\n清理FIFO文件...")
        cleanup_fifos()
        
        print("\n测试结束")

if __name__ == "__main__":
    main()
