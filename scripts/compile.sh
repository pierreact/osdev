#!/bin/bash
cd "$(dirname "$0")/.."
set -euo pipefail

OBJ=bin/os.bin
ISO_ROOT=bin/iso
ISO_IMAGE=bin/os.iso

if command -v devbox >/dev/null 2>&1; then
    RUNNER=(devbox run --)
else
    RUNNER=()
fi

run_with_env() {
    if [ ${#RUNNER[@]} -gt 0 ]; then
        "${RUNNER[@]}" "$@"
    else
        "$@"
    fi
}

run_with_env make -C src clean
run_with_env make -C src

echo "[compile] Building user applications..."
run_with_env make -C apps clean
run_with_env make -C apps

mkdir -p bin
echo "[compile] Building raw boot image..."
# Build fixed-size raw image without a pipe (avoids pipefail/SIGPIPE exits).
run_with_env dd if=/dev/zero of="$OBJ" bs=512 count=204624
run_with_env dd if=src/bootsector.bin of="$OBJ" conv=notrunc
run_with_env dd if=src/kernel.bin of="$OBJ" bs=512 seek=1 conv=notrunc

# Create optional data disk image used by the IDE/FAT32 driver.
echo "[compile] Preparing optional FAT32 data disk..."
if ! run_with_env scripts/create_fat32_disk.sh; then
    echo "[compile] Warning: data disk creation failed; continuing (boot does not depend on it)." >&2
fi

# Prepare ISO tree (El Torito: whole os.bin is the bootable disk image).
# GRUB chainload + loopback cannot work here: INT 13h would read CD sectors,
# not the file bytes after sector 0 of os.bin inside the ISO.
echo "[compile] Preparing ISO tree..."
rm -rf "$ISO_ROOT"
mkdir -p "$ISO_ROOT/boot"
mkdir -p "$ISO_ROOT/bin"
cp "$OBJ" "$ISO_ROOT/boot/os.bin"
cp apps/demo_app/demo_app "$ISO_ROOT/bin/demo_app" 2>/dev/null || true

if run_with_env command -v xorriso >/dev/null 2>&1; then
    # No-emulation El Torito: BIOS loads boot-load-size × 512 bytes to 0:7C00h.
    # Bootsector is sector 0; kernel follows at sector 1.  The bootsector detects
    # CD boot (DL >= 0xE0) and copies the kernel from 0x7E00 to 0x1000 — no INT 13h.
    # For raw-disk boot (-hda os.bin), DL = 0x80 and the CHS loop is used instead.
    KERNEL_BYTES=$(wc -c < src/kernel.bin | tr -d ' ')
    SECS=$(( (KERNEL_BYTES + 511) / 512 ))
    MARGIN=32
    KSIZE=$((SECS + MARGIN))
    BOOT_LOAD_SIZE=$((KSIZE + 1))
    echo "[compile] Generating BIOS ISO via xorriso (no-emul boot, ${BOOT_LOAD_SIZE} sectors, KSIZE=${KSIZE})..."
    run_with_env xorriso -as mkisofs \
        -o "$ISO_IMAGE" \
        -r -J \
        -V ISURUS_OS \
        -b boot/os.bin \
        -no-emul-boot \
        -boot-load-size "$BOOT_LOAD_SIZE" \
        "$ISO_ROOT"
elif run_with_env command -v grub-mkrescue >/dev/null 2>&1; then
    echo "[compile] Warning: xorriso not found; falling back to grub-mkrescue (ISO boot may not load kernel)." >&2
    mkdir -p "$ISO_ROOT/boot/grub"
    cp iso/boot/grub/grub.cfg "$ISO_ROOT/boot/grub/grub.cfg"
    run_with_env grub-mkrescue -o "$ISO_IMAGE" "$ISO_ROOT"
else
    echo "Error: xorriso not found (needed for correct BIOS ISO boot)." >&2
    echo "Install xorriso (see devbox.json). grub-mkrescue alone cannot boot this kernel from ISO." >&2
    exit 1
fi

echo "Built raw image: $OBJ"
echo "Built BIOS ISO:  $ISO_IMAGE"

