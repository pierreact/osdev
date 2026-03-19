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
    -smp 4,sockets=2,cores=2,threads=1 \
    -object memory-backend-ram,id=mem0,size=1G \
    -object memory-backend-ram,id=mem1,size=1G \
    -numa node,nodeid=0,cpus=0-1,memdev=mem0 \
    -numa node,nodeid=1,cpus=2-3,memdev=mem1 \
    -s \
    -monitor stdio \
    -boot order=c \
    -drive file=$OBJ,if=ide,index=0,format=raw \
    -drive file=bin/fat32.img,if=ide,index=1,format=raw \
    -D /tmp/qemu.log \
    -d int,cpu_reset,guest_errors

