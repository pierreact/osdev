#!/bin/bash
OBJ=bin/os.bin
./compile.sh

rm /tmp/qemu.log
qemu-system-x86_64 -m 2G -s -monitor stdio -boot order=c -drive file=$OBJ,if=ide,index=2 -D /home/pierre/Projects/Code/OS/kernel/qemu.log  #-enable-kvm #-no-shutdown -no-reboot

