#!/bin/bash
# Create a TAP device for L2 network testing. Requires sudo.
# Host side: tap0 at 10.0.2.2/24
# Isurus mgmt NIC: 10.0.2.15/24
set -euo pipefail

if ip link show tap0 >/dev/null 2>&1; then
    echo "tap0 already exists"
    exit 0
fi

sudo ip tuntap add dev tap0 mode tap user "$(whoami)"
sudo ip addr add 10.0.2.2/24 dev tap0
sudo ip link set tap0 up

echo "tap0 ready (10.0.2.2/24)"
