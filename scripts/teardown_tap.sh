#!/bin/bash
# Remove the TAP device created by setup_tap.sh.
set -euo pipefail

if ! ip link show tap0 >/dev/null 2>&1; then
    echo "tap0 does not exist"
    exit 0
fi

sudo ip tuntap del dev tap0 mode tap
echo "tap0 removed"
