# Deployment

## Single node

Boot from ISO. The build system produces `bin/os.iso`, a BIOS-bootable ISO built with xorriso (El Torito no-emulation boot). Flash it to USB, mount it in a VM, or pass it to QEMU with `-cdrom`.

At boot, the kernel scans AHCI SATAPI ports for a volume labeled "ISURUS_OS" and auto-mounts the matching ISO as the root filesystem.

See [Building and Running](../getting-started/building.md) for build instructions.

## Cluster

Each node boots from the same ISO. Nodes discover each other via the BSP inter-node NIC (layer 2). The cluster forms automatically once all nodes are online.

Current boot method: ISO on each node (USB, CD, or virtual media).

Planned: PXE boot from a central server, allowing diskless nodes to boot directly from the network.

## Cloud (AWS)

Planned: an AMI (Amazon Machine Image) for deployment on AWS EC2 instances. The AMI will contain the same boot image used for bare-metal deployment.

## Hardware requirements

See [Hardware Requirements](../getting-started/hardware.md) for processor and DMA coherence requirements.
