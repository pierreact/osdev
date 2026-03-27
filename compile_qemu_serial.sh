#!/bin/bash
# Interactive serial mode: OS shell on stdio, no VGA window.
# Use this to interact with the OS over COM1.
set -euo pipefail

ISO=bin/os.iso
DATA_DISK=bin/fat32.img
./compile.sh

if [ ! -f "$ISO" ]; then
    echo "Error: ISO not found after compile: $ISO" >&2
    exit 1
fi

rm -f ./qemu.log
echo "========================================"
echo "QEMU Serial Mode - OS shell on stdio"
echo "Press Ctrl-A X to exit QEMU"
echo "========================================"
QEMU_ARGS=(
    -m 2G
    -serial stdio
    -display none
    -monitor none
    -smp "4,sockets=2,cores=2,threads=1"
    -object "memory-backend-ram,id=mem0,size=1G"
    -object "memory-backend-ram,id=mem1,size=1G"
    -numa "node,nodeid=0,cpus=0-1,memdev=mem0"
    -numa "node,nodeid=1,cpus=2-3,memdev=mem1"
    -boot order=d
    -cdrom "$ISO"
    -D ./qemu.log
    -d "int,cpu_reset,guest_errors"
)

if [ -f "$DATA_DISK" ]; then
    QEMU_ARGS+=(-drive "file=$DATA_DISK,if=ide,index=1,format=raw")
fi

qemu-system-x86_64 "${QEMU_ARGS[@]}"
