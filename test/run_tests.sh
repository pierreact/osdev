#!/bin/bash
# Boot Isurus in QEMU headless, send commands over serial, verify output.
# Exit 0 on success, 1 on failure.
#
# Usage: ./test/run_tests.sh [path/to/os.iso]
# Requires: qemu-system-x86_64
set -euo pipefail

ISO="${1:-bin/os.iso}"
TIMEOUT=30
SERIAL_LOG="test/serial_output.log"
QEMU_PID=""

cleanup() {
    if [ -n "$QEMU_PID" ] && kill -0 "$QEMU_PID" 2>/dev/null; then
        kill "$QEMU_PID" 2>/dev/null
        wait "$QEMU_PID" 2>/dev/null || true
    fi
    rm -f test/qemu.in test/qemu.out
}
trap cleanup EXIT

if [ ! -f "$ISO" ]; then
    echo "FAIL: ISO not found: $ISO" >&2
    echo "Run ./compile.sh first." >&2
    exit 1
fi

# Create named pipes for serial I/O
# QEMU -serial pipe:NAME opens NAME.in (guest reads) and NAME.out (guest writes)
rm -f test/qemu.in test/qemu.out
mkfifo test/qemu.in test/qemu.out

# Boot QEMU headless with serial on pipes
qemu-system-x86_64 \
    -machine q35 \
    -m 2G \
    -display none \
    -monitor none \
    -serial pipe:test/qemu \
    -smp "4,sockets=2,cores=2,threads=1" \
    -object "memory-backend-ram,id=mem0,size=1G" \
    -object "memory-backend-ram,id=mem1,size=1G" \
    -numa "node,nodeid=0,cpus=0-1,memdev=mem0" \
    -numa "node,nodeid=1,cpus=2-3,memdev=mem1" \
    -device pxb-pcie,id=pcie.1,bus_nr=10,bus=pcie.0,numa_node=0 \
    -device pxb-pcie,id=pcie.2,bus_nr=20,bus=pcie.0,numa_node=1 \
    -device pcie-root-port,id=rp1,bus=pcie.1,chassis=1 \
    -device pcie-root-port,id=rp2,bus=pcie.1,chassis=2 \
    -device pcie-root-port,id=rp3,bus=pcie.1,chassis=3 \
    -device pcie-root-port,id=rp4,bus=pcie.2,chassis=4 \
    -device pcie-root-port,id=rp5,bus=pcie.2,chassis=5 \
    -device pcie-root-port,id=rp6,bus=pcie.2,chassis=6 \
    -netdev user,id=mgmt0 \
    -device virtio-net-pci,netdev=mgmt0,romfile=,bus=rp1 \
    -netdev user,id=inter0 \
    -device virtio-net-pci,netdev=inter0,romfile=,bus=rp2 \
    -netdev user,id=dpdk0 \
    -device virtio-net-pci,netdev=dpdk0,romfile=,bus=rp3 \
    -netdev user,id=dpdk1 \
    -device virtio-net-pci,netdev=dpdk1,romfile=,bus=rp4 \
    -netdev user,id=dpdk2 \
    -device virtio-net-pci,netdev=dpdk2,romfile=,bus=rp5 \
    -boot order=d \
    -cdrom "$ISO" \
    -no-reboot &
QEMU_PID=$!

# Capture serial output in background (QEMU writes to .out)
cat test/qemu.out > "$SERIAL_LOG" &
CAT_PID=$!

# Send a command and wait for output
send_cmd() {
    local cmd="$1"
    local wait_secs="${2:-3}"
    # Send command + newline
    printf "%s\r" "$cmd" > test/qemu.in
    sleep "$wait_secs"
}

# Wait for shell prompt (boot complete)
echo "Waiting for boot..."
sleep 8

PASS=0
FAIL=0

check() {
    local desc="$1"
    local pattern="$2"
    if grep -q "$pattern" "$SERIAL_LOG"; then
        echo "  PASS: $desc"
        PASS=$((PASS + 1))
    else
        echo "  FAIL: $desc (expected: $pattern)"
        FAIL=$((FAIL + 1))
    fi
}

echo "Running tests..."

# Test: shell prompt appeared (boot succeeded)
check "Boot complete (shell prompt)" "> "

# Test: TSS and SYSCALL infrastructure initialized during boot
check "TSS loaded" "TSS: BSP TSS loaded"

# Test: BSP task scheduler initialized
check "Task scheduler initialized" "TASK: BSP task scheduler"

# Test: Shell task created and dropped to ring 3
check "Shell task created" "TASK: created 'shell'"
check "Dropped to ring 3" "TASK: dropping to ring 3"

# Test: sys.cpu.ring (shell runs in ring 3)
send_cmd "sys.cpu.ring"
check "Shell runs in ring 3" "ring 3"

# Test: sys.cpu.ls
send_cmd "sys.cpu.ls"
check "CPU count detected" "CPU(s):"
check "CPUs online" "Online CPU(s):"
check "LAPIC base" "LAPIC base:"
check "NUMA node(s)" "NUMA node(s):"

# Test: sys.mem.free
send_cmd "sys.mem.free"
check "Heap info displayed" "Heap"

# Test: sys.mem.test
send_cmd "sys.mem.test"
check "Memory test passed" "Test complete"

# Test: sys.mem.info
send_cmd "sys.mem.info"
check "MMAP entries" "MMAP"

# Test: sys.acpi.ls
send_cmd "sys.acpi.ls"
check "ACPI tables listed" "MADT"

# Test: sys.pci.ls (Q35 + 4 virtio-net devices)
send_cmd "sys.pci.ls"
check "PCI devices found" "1af4"

# Test: sys.nic.ls
send_cmd "sys.nic.ls"
check "NIC interface listed" "virtio-net"

# Test: sys.nic.mode
send_cmd "sys.nic.mode"
check "NIC mode displayed" "mode:"

# Test: sys.thread.ls
send_cmd "sys.thread.ls"
check "Thread metadata displayed" "CPU"

# Test: sys.disk.ls (AHCI devices)
send_cmd "sys.disk.ls"
check "AHCI devices listed" "sata"

# Test: VFS / ISO filesystem
send_cmd "sys.fs.ls /"
check "ISO root listed" "BOOT"

# Test: demo app binary on ISO
send_cmd "sys.fs.ls /bin"
check "Demo app on ISO" "DEMO_APP"

# Summary
echo ""
echo "Results: $PASS passed, $FAIL failed"

# Cleanup
kill "$CAT_PID" 2>/dev/null || true

if [ "$FAIL" -gt 0 ]; then
    echo "Serial output saved to: $SERIAL_LOG"
    exit 1
fi
exit 0
