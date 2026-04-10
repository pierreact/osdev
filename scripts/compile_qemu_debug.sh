#!/bin/bash
# Debug version - prevents automatic reboots on crashes/triple faults
cd "$(dirname "$0")/.."
set -euo pipefail

ISO=bin/os.iso
DATA_DISK=bin/fat32.img
scripts/compile.sh

if [ ! -f "$ISO" ]; then
    echo "Error: ISO not found after compile: $ISO" >&2
    exit 1
fi

mkdir -p logs
rm -f logs/qemu.log
echo "========================================"
echo "QEMU DEBUG Mode - logs in logs/qemu.log"
echo "Serial output: logs/serial.log"
echo "Reboot disabled for debugging"
echo "Press Ctrl+C to exit"
echo "========================================"
QEMU_ARGS=(
    -machine q35
    -m 2G
    -serial file:logs/serial.log
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
    -D logs/qemu.log
    -d in_asm,int,cpu_reset,guest_errors
    -dfilter 0x600+0x200
    -no-reboot
    -no-shutdown
)

if [ -f "$DATA_DISK" ]; then
    QEMU_ARGS+=(-drive "file=$DATA_DISK,if=ide,index=1,format=raw")
else
    echo "Optional data disk not found: $DATA_DISK (continuing without it)"
fi

qemu-system-x86_64 "${QEMU_ARGS[@]}"

