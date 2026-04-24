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
TEST_LOG="logs/tests.log"
QEMU_PID=""
TAP_CREATED_BY_TEST=0

mkdir -p logs
# Tee all output (PASS/FAIL lines, build, etc.) to logs/tests.log
exec > >(tee "$TEST_LOG") 2>&1

# Build first to ensure ISO is up to date
echo "Building..."
scripts/compile.sh

KEEPALIVE_PID=""

cleanup() {
    if [ -n "$KEEPALIVE_PID" ] && kill -0 "$KEEPALIVE_PID" 2>/dev/null; then
        kill "$KEEPALIVE_PID" 2>/dev/null
    fi
    if [ -n "$QEMU_PID" ] && kill -0 "$QEMU_PID" 2>/dev/null; then
        kill "$QEMU_PID" 2>/dev/null
        wait "$QEMU_PID" 2>/dev/null || true
    fi
    rm -f test/qemu.in test/qemu.out
    # Tear down tap0 only if we created it
    if [ "$TAP_CREATED_BY_TEST" = "1" ]; then
        echo "Tearing down tap0..."
        scripts/teardown_tap.sh >/dev/null 2>&1 || true
    fi
}
trap cleanup EXIT

if [ ! -f "$ISO" ]; then
    echo "FAIL: ISO not found: $ISO" >&2
    echo "Run ./compile.sh first." >&2
    exit 1
fi

# tap0 is required for L2 network testing (mandatory).
# If missing, create it now (sudo prompt happens up-front, before the
# unprivileged test loop). Tear down at exit if we created it.
if ! ip link show tap0 >/dev/null 2>&1; then
    echo "Creating tap0 for L2 testing (sudo required)..."
    if ! scripts/setup_tap.sh; then
        echo "FAIL: could not create tap0." >&2
        echo "Manually: sudo scripts/setup_tap.sh" >&2
        exit 1
    fi
    TAP_CREATED_BY_TEST=1
fi

# Verify tap0 is up with the expected IP
if ! ip addr show tap0 | grep -q "10.0.2.2"; then
    echo "FAIL: tap0 exists but missing 10.0.2.2/24" >&2
    echo "Recreate: scripts/teardown_tap.sh && scripts/setup_tap.sh" >&2
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
stdbuf -o0 cat test/qemu.out > "$SERIAL_LOG" &
CAT_PID=$!

# Keep a writer open on the input FIFO indefinitely so QEMU never sees EOF.
# sleep infinity holds its stdout open without writing anything; redirected
# to the FIFO it acts as a permanent (silent) writer.
sleep infinity > test/qemu.in &
KEEPALIVE_PID=$!

send_cmd() {
    local cmd="$1"
    local wait_secs="${2:-3}"
    echo "  [send_cmd] $cmd" >&2
    printf "%s\r" "$cmd" > test/qemu.in
    sleep "$wait_secs"
}

# Wait for shell prompt (boot complete)
echo "Waiting for boot..."
sleep 12

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

# Test: DPDK L2 reflector (apps/dpdk_l2) - per-core polled L2 app on APs.
# Run early, before the known-flaky shell tests below. Needs ~1 s per
# core (MAX_ITERATIONS) * 3 cores plus dispatch overhead.
send_cmd "sys.proc.run /CONF/DPDK_L2.INI" 8
check "DPDK L2 app ready on AP"   "\[dpdk_l2\] cpu=1 nic=.* ready"
check "DPDK L2 app stats printed" "\[dpdk_l2\] cpu=.* rx="

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

# Test: PCI NUMA shown in sys.pci.ls (already sent above)
check "PCI NUMA shown" "NUMA"

# Test: PCI vendor names loaded from ISO
check "PCI vendor names" "Red Hat"

# Test: NIC mode switch to per-core
send_cmd "sys.nic.mode per-core"
check "Mode set to per-core" "per-core"

# Test: NIC mode switch to per-numa
send_cmd "sys.nic.mode per-numa"
check "Mode set to per-numa" "per-numa"

# Test: Thread metadata shows NIC assignment (sys.thread.ls already sent)
check "Thread NIC assignment" "NIC"

# Test: PCI IDs loaded from ISO
check "PCI IDs loaded from ISO" "PCI-IDS:"

# Test: Demo app
send_cmd "sys.proc.run /CONF/DEMO_APP.INI" 5
check "Demo app ran" "finished"
check "Demo app NIC info" "MAC"

# Test: Data directory on ISO
send_cmd "sys.fs.ls /data"
check "PCI IDs file on ISO" "PCI"

# Test: L2 networking initialized
check "L2 initialized" "L2: mgmt"

# Test: L2 stats command
send_cmd "sys.net.stats"
check "L2 stats displayed" "rx_frames"

# Test: ARP table command
send_cmd "sys.net.arp"
check "ARP table displayed" "ARP"

# Stop the main QEMU instance before starting the L2 test instance.
kill "$KEEPALIVE_PID" 2>/dev/null || true
KEEPALIVE_PID=""
kill "$CAT_PID" 2>/dev/null || true
if kill -0 "$QEMU_PID" 2>/dev/null; then
    kill "$QEMU_PID" 2>/dev/null
    wait "$QEMU_PID" 2>/dev/null || true
fi
QEMU_PID=""
rm -f test/qemu.in test/qemu.out

# ============================================================================
# Phase 2: L2 network tests via TAP (separate QEMU run)
# ============================================================================
echo ""
echo "Phase 2: L2 network tests (TAP)..."

L2_SERIAL_LOG="test/serial_l2.log"
mkfifo test/qemu.in test/qemu.out

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
    -netdev tap,id=mgmt0,ifname=tap0,script=no,downscript=no \
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

stdbuf -o0 cat test/qemu.out > "$L2_SERIAL_LOG" &
CAT_PID=$!

# Re-establish keepalive writer for phase 2
sleep infinity > test/qemu.in &
KEEPALIVE_PID=$!

# Wait for boot
SERIAL_LOG_BACKUP="$SERIAL_LOG"
SERIAL_LOG="$L2_SERIAL_LOG"
echo "Waiting for L2 phase boot..."
sleep 12

# Debug: capture what's actually on tap0 during ARP test.
# arping and tcpdump need raw sockets (sudo) so they must be
# system-installed (apt-get install iputils-arping tcpdump), NOT
# from devbox nix store (root can't access NFS due to root_squash).
L2_TAP_LOG="logs/tap0_capture.txt"

if ! command -v arping >/dev/null 2>&1; then
    echo "FAIL: arping not found. Install: sudo apt-get install arping" >&2
    exit 1
fi
if ! command -v tcpdump >/dev/null 2>&1; then
    echo "FAIL: tcpdump not found. Install: sudo apt-get install tcpdump" >&2
    exit 1
fi

sudo tcpdump -i tap0 -n -c 20 > "$L2_TAP_LOG" 2>&1 &
TCPDUMP_PID=$!
sleep 1

sudo arping -c 3 -I tap0 -w 5 10.0.2.15 >> "$L2_TAP_LOG" 2>&1 || true
sleep 2

sudo kill $TCPDUMP_PID 2>/dev/null || true
wait $TCPDUMP_PID 2>/dev/null || true

echo "  [tap0 capture saved to $L2_TAP_LOG]"

# Now poll kernel-side and check
send_cmd "sys.net.stats"
send_cmd "sys.net.arp"
check "L2 ARP reply sent" "arp_reply: [1-9]"
check "Host in ARP table" "10.0.2.2"

SERIAL_LOG="$SERIAL_LOG_BACKUP"

# Summary
echo ""
echo "Results: $PASS passed, $FAIL failed"

# Cleanup of L2 phase QEMU happens via trap

if [ "$FAIL" -gt 0 ]; then
    echo "Serial output: $SERIAL_LOG and $L2_SERIAL_LOG"
    exit 1
fi
exit 0
