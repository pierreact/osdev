#!/bin/bash
start_dir=`pwd`

OBJ=bin/os.bin

cd src
make clean
make

#cd $start_dir
#bash update_image.sh
#bash run_qemu.sh




nasm -f bin -o bootsector.bin bootsector.asm
cd $start_dir

#gcc $OPTIONS -c screen.c
#gcc $OPTIONS -c kernel.c

#ld --oformat binary -m i386linux -b elf32-i386 -Ttext=1000 -Tdata=2000 kern.o screen.o -o kernel

#ld --oformat binary -m i386linux -b elf32-i386 -Ttext=1000 kern.o -o kernel

cat src/bootsector.bin src/kernel /dev/zero | dd iflag=fullblock of=$OBJ bs=512 count=204800

#qemu-system-x86_64 -s -monitor stdio -boot order=c -hda $OBJ

qemu-system-x86_64 -s -monitor stdio -boot order=c -drive file=$OBJ,if=ide,index=2
#qemu-system-x86_64 -m 128M -boot order=c -drive file=$OBJ,if=ide,index=2 #-d cpu_reset


