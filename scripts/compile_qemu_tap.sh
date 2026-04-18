#!/bin/bash
# L2 testing mode: management NIC on tap0 for host-to-VM raw Ethernet access.
# Requires: scripts/setup_tap.sh run first (creates tap0 at 10.0.2.2/24).
# Serial on stdio. Packet capture written to logs/mgmt0.pcap.
cd "$(dirname "$0")/.."
set -euo pipefail

ISO=bin/os.iso
DATA_DISK=bin/fat32.img

scripts/compile.sh

if [ ! -f "$ISO" ]; then
    echo "Error: ISO not found after compile: $ISO" >&2
    exit 1
fi

if ! ip link show tap0 >/dev/null 2>&1; then
    echo "Error: tap0 not found. Run: scripts/setup_tap.sh" >&2
    exit 1
fi

mkdir -p logs
rm -f logs/qemu.log logs/mgmt0.pcap

echo "========================================"
echo "QEMU TAP Mode - mgmt NIC on tap0"
echo "  Host:    10.0.2.2/24 (tap0)"
echo "  Isurus:  10.0.2.15/24 (mgmt NIC 0)"
echo "  Capture: logs/mgmt0.pcap"
echo "  Serial:  stdio"
echo "  Press Ctrl-A X to exit QEMU"
echo "========================================"

QEMU_ARGS=(
    -machine q35
    -m 2G
    -serial stdio
    -display none
    -monitor none
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
    # Management NIC on tap0 (L2 accessible from host)
    -netdev tap,id=mgmt0,ifname=tap0,script=no,downscript=no
    -device virtio-net-pci,netdev=mgmt0,romfile=,bus=rp1
    # Remaining NICs on user-mode networking
    -netdev user,id=inter0
    -device virtio-net-pci,netdev=inter0,romfile=,bus=rp2
    -netdev user,id=dpdk0
    -device virtio-net-pci,netdev=dpdk0,romfile=,bus=rp3
    -netdev user,id=dpdk1
    -device virtio-net-pci,netdev=dpdk1,romfile=,bus=rp4
    -netdev user,id=dpdk2
    -device virtio-net-pci,netdev=dpdk2,romfile=,bus=rp5
    -boot order=d
    -cdrom "$ISO"
    -D logs/qemu.log
    -d "int,cpu_reset,guest_errors"
    # Packet capture on management NIC
    -object filter-dump,id=dump0,netdev=mgmt0,file=logs/mgmt0.pcap
)

if [ -f "$DATA_DISK" ]; then
    QEMU_ARGS+=(-drive "file=$DATA_DISK,if=ide,index=1,format=raw")
fi

qemu-system-x86_64 "${QEMU_ARGS[@]}"
