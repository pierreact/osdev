#!/bin/bash
cd "$(dirname "$0")/.."

# This script connects GDB to QEMU for debugging
# Run scripts/compile_qemu.sh in another terminal first (it starts with -s flag)

cat > /tmp/gdb_commands.txt << 'EOF'
# Connect to QEMU
target remote localhost:1234

# Set architecture
set architecture i386:x86-64

# Useful breakpoints (uncomment as needed)
# b *0x1000                    # Break at kernel start (16-bit)
# b setup_64_bits_paging_structures
# b *0x100000                  # Break at PML4T location

# Display info
define show_state
    info registers
    x/10i $pc
end

echo ========================================\n
echo GDB Connected to QEMU\n
echo ========================================\n
echo Commands:\n
echo   c          - continue\n
echo   si         - step instruction\n
echo   show_state - show registers and next instructions\n
echo   info registers - show all registers\n
echo   x/10i $pc  - show next 10 instructions\n
echo ========================================\n

# Show initial state
show_state
EOF

gdb -x /tmp/gdb_commands.txt
