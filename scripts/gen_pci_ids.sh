#!/bin/bash
# Download pci.ids and generate data/pci.ids as a simple text file
# for runtime loading from the boot ISO.
#
# Format:
#   V XXXX Vendor Name
#   C XX XX Class Name
#
# The generated file is committed to git so the build does not need
# network access.
cd "$(dirname "$0")/.."
set -euo pipefail

URL="https://pci-ids.ucw.cz/v2.2/pci.ids"
CACHE=$(mktemp)
OUT=data/pci.ids

trap 'rm -f "$CACHE"' EXIT

mkdir -p data

echo "[gen_pci_ids] Downloading $URL..."
if command -v curl >/dev/null 2>&1; then
    curl -fsSL "$URL" -o "$CACHE"
elif command -v wget >/dev/null 2>&1; then
    wget -q "$URL" -O "$CACHE"
else
    echo "Error: need curl or wget" >&2
    exit 1
fi

echo "[gen_pci_ids] Generating $OUT..."

awk '
BEGIN {
    # Whitelist of vendor IDs to embed
    keep["1002"] = 1   # AMD/ATI GPU
    keep["1022"] = 1   # AMD CPU
    keep["1106"] = 1   # VIA
    keep["1131"] = 1   # Conexant/NXP
    keep["1180"] = 1   # Ricoh
    keep["11ab"] = 1   # Marvell
    keep["1234"] = 1   # Bochs (QEMU stdvga)
    keep["126f"] = 1   # Silicon Motion
    keep["144d"] = 1   # Samsung
    keep["14e4"] = 1   # Broadcom
    keep["15b3"] = 1   # Mellanox
    keep["168c"] = 1   # Qualcomm Atheros
    keep["1969"] = 1   # Atheros / Qualcomm Atheros
    keep["1af4"] = 1   # Red Hat virtio
    keep["1b21"] = 1   # ASMedia
    keep["1b36"] = 1   # Red Hat Q35 chipset
    keep["1b73"] = 1   # Fresco Logic
    keep["1c5c"] = 1   # SK hynix
    keep["1cc1"] = 1   # ADATA
    keep["1cc4"] = 1   # Solidigm
    keep["1d6b"] = 1   # Linux Foundation
    keep["1e0f"] = 1   # KIOXIA
    keep["8086"] = 1   # Intel
    keep["8087"] = 1   # Intel
    keep["10de"] = 1   # NVIDIA
    keep["10ec"] = 1   # Realtek
}

# Vendor line: "XXXX  Vendor Name"
/^[0-9a-fA-F]{4}  / {
    id = substr($0, 1, 4)
    if (!(id in keep)) next
    name = substr($0, 7)
    printf "V %s %s\n", id, name
    next
}

# Class line: "C XX  Class Name"
/^C [0-9a-fA-F]{2}  / {
    cls = substr($0, 3, 2)
    name = substr($0, 7)
    printf "C %s ff %s\n", cls, name
    last_cls = cls
    next
}

# Subclass line: "\tXX  Subclass Name"
/^\t[0-9a-fA-F]{2}  / {
    if (last_cls != "") {
        sub_id = substr($0, 2, 2)
        name = substr($0, 6)
        printf "C %s %s %s\n", last_cls, sub_id, name
    }
    next
}

{ next }
' "$CACHE" > "$OUT"

LINES=$(wc -l < "$OUT")
SIZE=$(wc -c < "$OUT")
echo "[gen_pci_ids] Wrote $OUT ($LINES lines, $SIZE bytes)"
