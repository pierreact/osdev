#!/bin/bash
cd "$(dirname "$0")/.."

echo "========================================"
echo "QEMU Debug Log - Last 100 lines"
echo "========================================"

if [ -f logs/qemu.log ]; then
    tail -100 logs/qemu.log
    echo ""
    echo "========================================"
    echo "Searching for errors/resets..."
    echo "========================================"
    grep -i "reset\|triple\|fault\|exception\|error" logs/qemu.log | tail -20
else
    echo "No log file found at logs/qemu.log"
    echo "Run scripts/compile_qemu.sh first"
fi
