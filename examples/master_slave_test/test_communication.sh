#!/bin/bash

echo "=========================================="
echo "Master-Slave Network Topology Test"
echo "=========================================="
echo ""

BASE_DIR="$(cd "$(dirname "$0")" && pwd)"
BIN_DIR="$BASE_DIR/bin"
FIFO_DIR="/tmp/eproto_test_$$"

cleanup() {
    echo "Cleaning up..."
    rm -rf "$FIFO_DIR"
    kill $MASTER_PID 2>/dev/null
    kill $S2_PID 2>/dev/null
    kill $S3_PID 2>/dev/null
    kill $S4_PID 2>/dev/null
    wait 2>/dev/null
}

trap cleanup EXIT

if [ ! -d "$BIN_DIR" ]; then
    echo "Error: bin directory not found. Please run 'make' first."
    exit 1
fi

if [ ! -f "$BIN_DIR/master_m1" ]; then
    echo "Error: master_m1 executable not found. Please run 'make' first."
    exit 1
fi

if [ ! -f "$BIN_DIR/slave_s2" ] || [ ! -f "$BIN_DIR/slave_s3" ] || [ ! -f "$BIN_DIR/slave_s4" ]; then
    echo "Error: slave executables not found. Please run 'make' first."
    exit 1
fi

mkdir -p "$FIFO_DIR"
mkfifo "$FIFO_DIR/commands"

echo "Starting Master Device M1..."
cd "$BIN_DIR"
./master_m1 < "$FIFO_DIR/commands" > /tmp/master_m1.log 2>&1 &
MASTER_PID=$!
echo "Master M1 started (PID: $MASTER_PID)"

echo "Starting Slave Device S2..."
./slave_s2 > /tmp/slave_s2.log 2>&1 &
S2_PID=$!
echo "Slave S2 started (PID: $S2_PID)"

echo "Starting Slave Device S3..."
./slave_s3 > /tmp/slave_s3.log 2>&1 &
S3_PID=$!
echo "Slave S3 started (PID: $S3_PID)"

echo "Starting Slave Device S4..."
./slave_s4 > /tmp/slave_s4.log 2>&1 &
S4_PID=$!
echo "Slave S4 started (PID: $S4_PID)"

echo ""
echo "All devices started. Sending test commands..."
echo ""

sleep 2

exec 3>"$FIFO_DIR/commands"
 echo "send 2 1 AA BB CC" >&3
 sleep 1
 echo "send 3 1 DD EE FF" >&3
 sleep 1
 echo "send 4 1 11 22 33" >&3
 sleep 1
 echo "quit" >&3
 exec 3>&-
 echo "All commands sent"

 sleep 3

echo "=========================================="
echo "Test Results"
echo "=========================================="
echo ""

echo "--- Master M1 Log ---"
cat /tmp/master_m1.log
echo ""

echo "--- Slave S2 Log ---"
cat /tmp/slave_s2.log
echo ""

echo "--- Slave S3 Log ---"
cat /tmp/slave_s3.log
echo ""

echo "--- Slave S4 Log ---"
cat /tmp/slave_s4.log
echo ""

echo ""
echo "Test completed!"
