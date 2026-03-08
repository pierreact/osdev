#!/bin/bash
OBJ=bin/os.bin
./compile.sh

rm -f /tmp/qemu.log
echo "========================================" 
echo "QEMU Running - logs in /tmp/qemu.log"
echo "Press Ctrl+C to exit"
echo "========================================"
qemu-system-x86_64 \
    -m 2G \
    -s \
    -monitor stdio \
    -boot order=c \
    -drive file=$OBJ,if=ide,index=0,format=raw \
    -drive file=bin/fat32.img,if=ide,index=1,format=raw \
    -D /tmp/qemu.log \
    -d int,cpu_reset,guest_errors

