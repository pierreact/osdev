#!/bin/bash
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
echo "QEMU Running - logs in ./qemu.log"
echo "Press Ctrl+C to exit"
echo "========================================"
# Bootsector writes COM1 trace to ./bootserial.log (tail -f in another terminal).
QEMU_ARGS=(
    -machine q35
    -m 2G
    -serial file:./bootserial.log
    -smp "4,sockets=2,cores=2,threads=1"
    -object "memory-backend-ram,id=mem0,size=1G"
    -object "memory-backend-ram,id=mem1,size=1G"
    -numa "node,nodeid=0,cpus=0-1,memdev=mem0"
    -numa "node,nodeid=1,cpus=2-3,memdev=mem1"
    -netdev user,id=mgmt0
    -device virtio-net-pci,netdev=mgmt0,romfile=
    -netdev user,id=inter0
    -device virtio-net-pci,netdev=inter0,romfile=
    -netdev user,id=dpdk0
    -device virtio-net-pci,netdev=dpdk0,romfile=
    -netdev user,id=dpdk1
    -device virtio-net-pci,netdev=dpdk1,romfile=
    -s
    -monitor stdio
    -boot order=d
    -cdrom "$ISO"
    -D ./qemu.log
    -d "int,cpu_reset,guest_errors"
)

if [ -f "$DATA_DISK" ]; then
    QEMU_ARGS+=(-drive "file=$DATA_DISK,if=ide,index=1,format=raw")
else
    echo "Optional data disk not found: $DATA_DISK (continuing without it)"
fi

qemu-system-x86_64 "${QEMU_ARGS[@]}"
