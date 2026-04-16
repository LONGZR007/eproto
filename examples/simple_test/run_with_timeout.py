#!/usr/bin/env python3
import subprocess
import sys
import time

# 运行命令并设置超时时间
def run_with_timeout(cmd, timeout=30):
    """运行命令并设置超时时间"""
    process = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    start_time = time.time()
    
    while process.poll() is None:
        if time.time() - start_time > timeout:
            process.kill()
            process.wait()
            print(f"Command timed out after {timeout} seconds")
            return False, "Timeout"
        time.sleep(0.1)
    
    stdout, stderr = process.communicate()
    return process.returncode == 0, stdout + stderr

if __name__ == "__main__":
    # 运行simple_test
    success, output = run_with_timeout("./simple_test")
    print(output)
    if success:
        print("\nTest passed!")
        sys.exit(0)
    else:
        print("\nTest failed!")
        sys.exit(1)