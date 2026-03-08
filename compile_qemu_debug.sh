#!/bin/bash
# Debug version - prevents automatic reboots on crashes/triple faults

OBJ=bin/os.bin
./compile.sh

rm -f /tmp/qemu.log
echo "========================================" 
echo "QEMU DEBUG Mode - logs in /tmp/qemu.log"
echo "Reboot disabled for debugging"
echo "Press Ctrl+C to exit"
echo "========================================"
qemu-system-x86_64 \
    -m 2G \
    -s \
    -monitor stdio \
    -boot order=c \
    -drive file=$OBJ,if=ide,index=0,format=raw \
    -D /tmp/qemu.log \
    -d int,cpu_reset,guest_errors \
    -no-reboot \
    -no-shutdown

