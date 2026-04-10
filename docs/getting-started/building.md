# Building and Running Isurus

## Prerequisites

**Required tools:** nasm, gcc, ld (binutils), xorriso, qemu-system-x86_64

**Optional:** mtools, dosfstools (for FAT32 data disk), gdb (for debugging)

### Using Devbox (recommended)

The repository includes a `devbox.json` that provides all required tools:

```bash
devbox shell
```

### Manual installation (Debian/Ubuntu)

```bash
apt-get install nasm gcc make binutils xorriso qemu-system-x86 mtools dosfstools gdb
```

## Build and run

```bash
scripts/compile_qemu.sh          # Build + boot (VGA window + QEMU monitor on stdio)
scripts/compile_qemu_serial.sh   # Build + boot (OS shell on stdio, no VGA)
scripts/compile_qemu_debug.sh    # Build + boot (no reboot on crash, for debugging)
```

Each script runs `scripts/compile.sh` (build) then launches QEMU.

## Build only

```bash
scripts/compile.sh
```

Produces:
- `bin/os.bin` -- raw boot image (bootsector + kernel)
- `bin/os.iso` -- BIOS-bootable ISO (El Torito no-emulation boot via xorriso)
- `bin/fat32.img` -- optional FAT32 data disk (if `scripts/create_fat32_disk.sh` succeeds)

## QEMU configuration

The QEMU scripts use Q35 machine type with:
- 2GB RAM, 2 NUMA nodes (1GB each)
- 4 CPUs (2 sockets, 2 cores)
- 4 virtio-net-pci devices (2 BSP NICs + 1 per NUMA node)
- ISO boot (`-boot order=d`)

## Running tests

```bash
./test/run_tests.sh
```

Boots QEMU headless, sends shell commands over serial, verifies output. Requires a built ISO at `bin/os.iso`.

## Checking logs

```bash
scripts/check_qemu_log.sh  # View QEMU exception/interrupt log
cat logs/serial.log         # Kernel serial output
```

See [debugging.md](../developer/debugging.md) for interpreting boot traces and using GDB.
