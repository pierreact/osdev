#!/bin/bash
# L2 network test: send ARP from host via tap0, verify Isurus responds.
# Requires: tap0 up (scripts/setup_tap.sh), QEMU running with TAP mode.
#
# Usage: ./test/test_l2.sh [serial_log]
# If serial_log is provided, checks it for L2 stats/ARP table.
# Otherwise runs standalone ARP test only.
set -euo pipefail

SERIAL_LOG="${1:-}"
PASS=0
FAIL=0

check_result() {
    local desc="$1"
    local ok="$2"
    if [ "$ok" = "1" ]; then
        echo "  PASS: $desc"
        PASS=$((PASS + 1))
    else
        echo "  FAIL: $desc"
        FAIL=$((FAIL + 1))
    fi
}

echo "L2 Network Tests"
echo "================"

# Check tap0 exists
if ! ip link show tap0 >/dev/null 2>&1; then
    echo "FAIL: tap0 not found. Run: scripts/setup_tap.sh"
    exit 1
fi
check_result "tap0 exists" "1"

# Check tap0 has the expected IP
if ip addr show tap0 | grep -q "10.0.2.2"; then
    check_result "tap0 has 10.0.2.2" "1"
else
    check_result "tap0 has 10.0.2.2" "0"
fi

# Send ARP request for Isurus management IP
echo "  Sending ARP for 10.0.2.15..."
if arping -c 1 -I tap0 -w 5 10.0.2.15 >/dev/null 2>&1; then
    check_result "ARP reply from 10.0.2.15" "1"
else
    check_result "ARP reply from 10.0.2.15" "0"
fi

# Verify host learned Isurus MAC (check ARP cache)
if ip neigh show dev tap0 | grep -q "10.0.2.15"; then
    check_result "Isurus MAC in host ARP cache" "1"
else
    check_result "Isurus MAC in host ARP cache" "0"
fi

# If serial log provided, check Isurus-side state
if [ -n "$SERIAL_LOG" ] && [ -f "$SERIAL_LOG" ]; then
    echo "  Checking serial log: $SERIAL_LOG"

    if grep -q "L2: mgmt" "$SERIAL_LOG"; then
        check_result "L2 init in serial log" "1"
    else
        check_result "L2 init in serial log" "0"
    fi
fi

echo ""
echo "Results: $PASS passed, $FAIL failed"

if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
exit 0
