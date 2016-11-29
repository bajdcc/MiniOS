# makefile

.PHONY: init install run fs fsck clean
.IGNORE: init

MAKE = make -r
AS = nasm
CC = gcc
DEL = rm -f
QEMU = qemu
LD = ld
OBJCPY = objcopy
GDB = cgdb
IMG = qemu-img
MKFS = mkfs.minix
FSCK = fsck.minix
CFLAGS = -c -O0 -Wall -Werror -nostdinc -fno-builtin -fno-stack-protector -funsigned-char \
		 -finline-functions -finline-small-functions -findirect-inlining \
		 -finline-functions-called-once -Iinclude -m32 -ggdb -gstabs+ -fdump-rtl-expand
ROOTFS = bin/rootfs
OBJS = bin/loader.o bin/main.o bin/asm.o bin/vga.o bin/string.o

# create a 1.44MB floppy include kernel and bootsector
bin/floppy.img: boot/floppy.asm bin/bootsect.bin bin/kernel 
	$(AS) -I ./bin/ -f bin -l lst/floppy.s $< -o $@ 

# bootsector
bin/bootsect.bin: boot/bootsect.asm 
	$(AS) -I ./boot/ -f bin -l lst/bootsect.s $< -o $@ 

bin/loader.o : src/kernel/loader.asm
	$(AS) -I ./boot/ -f elf32 -g -F stabs -l lst/loader.s $< -o $@ 

# link loader.o and c objfile 
# generate a symbol file(kernel.elf) and a flat binary kernel file(kernel)
bin/kernel: script/link.ld $(OBJS) 
	$(LD) -T$< -melf_i386 -static -o $@.elf $(OBJS) -M>lst/map.map
	$(OBJCPY) -O binary $@.elf $@

# compile c file in all directory
bin/%.o: src/*/%.c
	$(CC) $(CFLAGS) -c $^ -o $@  
	$(CC) $(CFLAGS) -S $^ -o lst/$*.s

#----------------------------------------

# init
init:
	mkdir lst
	mkdir bin
	mkdir $(ROOTFS)

default: Makefile
	$(MAKE) bin/floppy.img

# make a disk with minix v1 file system
fs: 
	$(DEL) bin/rootfs.img
	$(IMG) create -f qcow2 bin/rootfs.img 10M
	$(MKFS) bin/rootfs.img -1 -n14
	sudo mount -o loop -t minix bin/rootfs.img $(ROOTFS)
	mkdir $(ROOTFS)/bin
	mkdir $(ROOTFS)/share
	cp usr/logo.txt $(ROOTFS)/share
	sleep 1
	sudo umount $(ROOTFS)

# check root file system
fsck:
	$(FSCK) -fsl bin/rootfs.img

# run with qemu
run:
	$(QEMU) -S -s -fda bin/floppy.img -hda bin/rootfs.img -boot a &
	sleep 1
	$(GDB) -x script/gdbinit

# clean the binary file
clean: 
	$(DEL) bin/*.lst 
	$(DEL) bin/*.o 
	$(DEL) bin/*.bin 
	$(DEL) bin/*.tmp 
	$(DEL) bin/kernel 
	$(DEL) bin/kernel.elf
	$(DEL) bin/floppy.img
	$(DEL) lst/*
