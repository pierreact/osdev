#!/bin/bash
# Creates an optional FAT32 data disk image (non-boot media).

IMG=bin/fat32.img

# Skip if image already exists (delete it to regenerate).
if [ -f "$IMG" ]; then
    echo "FAT32 image already exists: $IMG"
    exit 0
fi

mkdir -p bin

echo "Creating FAT32 data disk image..."

# Create 32MB blank image
dd if=/dev/zero of="$IMG" bs=512 count=65536 2>/dev/null

# Format as FAT32
if [ "$(uname)" = "Darwin" ]; then
    newfs_msdos -F 32 -S 512 -s 8 -v MYOS "$IMG"
else
    mkfs.fat -F 32 -S 512 -s 8 -n MYOS "$IMG"
fi

# Create the lorem ipsum test file
LOREM_FILE=$(mktemp)
cat > "$LOREM_FILE" << 'LOREM_EOF'
Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed do eiusmod
tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim
veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea
commodo consequat.

Duis aute irure dolor in reprehenderit in voluptate velit esse cillum
dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non
proident, sunt in culpa qui officia deserunt mollit anim id est laborum.
LOREM_EOF

# Copy file into the FAT32 image using mtools
mcopy -i "$IMG" "$LOREM_FILE" ::LOREM.TXT

rm -f "$LOREM_FILE"

echo "FAT32 data disk image created: $IMG"
