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

The ISO also contains `apps/demo_app/demo_app` (at `/bin/demo_app`) and `data/pci.ids` (at `/data/pci.ids`).

## QEMU configuration

The QEMU scripts use Q35 machine type with:
- 2GB RAM, 2 NUMA nodes (1GB each)
- 4 CPUs (2 sockets, 2 cores)
- 5 virtio-net-pci devices (2 BSP + 3 AP pool) on 6 pcie-root-port devices via 2 pxb-pcie host bridges
- ISO boot (`-boot order=d`)

## Running tests

```bash
./test/run_tests.sh
```

Boots QEMU headless, sends shell commands over serial, verifies output. The
test runner auto-creates a `tap0` interface (10.0.2.2/24) on first run for L2
network testing -- this requires sudo (interactive prompt acceptable).

To configure passwordless sudo for `ip tuntap` on Ubuntu (CI use), add to
`/etc/sudoers.d/isurus-tap`:

```
%sudo ALL=(ALL) NOPASSWD: /sbin/ip tuntap, /sbin/ip addr, /sbin/ip link
```

To remove tap0:

```bash
scripts/teardown_tap.sh
```

## Checking logs

```bash
scripts/check_qemu_log.sh  # View QEMU exception/interrupt log
cat logs/serial.log         # Kernel serial output
```

See [debugging.md](../developer/debugging.md) for interpreting boot traces and using GDB.
