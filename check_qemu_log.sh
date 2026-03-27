#!/bin/bash

echo "========================================"
echo "QEMU Debug Log - Last 100 lines"
echo "========================================"

if [ -f ./qemu.log ]; then
    tail -100 ./qemu.log
    echo ""
    echo "========================================"
    echo "Searching for errors/resets..."
    echo "========================================"
    grep -i "reset\|triple\|fault\|exception\|error" ./qemu.log | tail -20
else
    echo "No log file found at ./qemu.log"
    echo "Run ./compile_qemu.sh first"
fi
