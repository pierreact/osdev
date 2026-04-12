#!/bin/bash
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
echo "QEMU Running - logs in logs/qemu.log"
echo "Serial output: logs/serial.log"
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
    -device pxb-pcie,id=pcie.1,bus_nr=10,bus=pcie.0,numa_node=0
    -device pxb-pcie,id=pcie.2,bus_nr=20,bus=pcie.0,numa_node=1
    -device pcie-root-port,id=rp1,bus=pcie.1,chassis=1
    -device pcie-root-port,id=rp2,bus=pcie.1,chassis=2
    -device pcie-root-port,id=rp3,bus=pcie.1,chassis=3
    -device pcie-root-port,id=rp4,bus=pcie.2,chassis=4
    -device pcie-root-port,id=rp5,bus=pcie.2,chassis=5
    -device pcie-root-port,id=rp6,bus=pcie.2,chassis=6
    -netdev user,id=mgmt0
    -device virtio-net-pci,netdev=mgmt0,romfile=,bus=rp1
    -netdev user,id=inter0
    -device virtio-net-pci,netdev=inter0,romfile=,bus=rp2
    -netdev user,id=dpdk0
    -device virtio-net-pci,netdev=dpdk0,romfile=,bus=rp3
    -netdev user,id=dpdk1
    -device virtio-net-pci,netdev=dpdk1,romfile=,bus=rp4
    -netdev user,id=dpdk2
    -device virtio-net-pci,netdev=dpdk2,romfile=,bus=rp5
    -s
    -monitor stdio
    -boot order=d
    -cdrom "$ISO"
    -D logs/qemu.log
    -d "int,cpu_reset,guest_errors"
)

if [ -f "$DATA_DISK" ]; then
    QEMU_ARGS+=(-drive "file=$DATA_DISK,if=ide,index=1,format=raw")
else
    echo "Optional data disk not found: $DATA_DISK (continuing without it)"
fi

qemu-system-x86_64 "${QEMU_ARGS[@]}"
