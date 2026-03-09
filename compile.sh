#!/bin/bash

start_dir=`pwd`

OBJ=bin/os.bin

cd src
make clean
make

cd $start_dir

cat src/bootsector.bin src/kernel /dev/zero | dd of=$OBJ bs=512 count=204624

# Create FAT32 disk image
./create_fat32_disk.sh

