#!/bin/sh

# Process arguments and add programs to the binary file whch will be transformed into the ELF section */


printf "../../libphoenix/test/psh\n../../phoenix-rtos-devices/tty/pc-uart/pc-uart" | cpio -o -H newc > /tmp/programs

#elf64-littleriscv

# Create ELF file consiting programs
riscv64-unknown-elf-objcopy -I binary -O elf64-littleriscv -Briscv:rv64 --redefine-sym _binary__tmp_programs_start=pprograms \
	-N _binary__tmp_programs_end -N _binary__tmp_programs_size /tmp/programs /tmp/programs.elf

# Copy ELF file into the .program section
riscv64-unknown-elf-objcopy -I elf64-littleriscv -O elf64-littleriscv --update-section .data=/tmp/programs programs.o \
	--add-symbol programs=.data:0
#	../phoenix-riscv64-withprograms.elf
	
#riscv64-unknown-elf-objcopy -I elf64-littleriscv -O elf64-littleriscv -R .sdata --add-section .sdata=/tmp/programs.elf ../phoenix-riscv64.elf \
#	--set-section-flags .programs=alloc ../phoenix-riscv64-withprograms.elf

# Relink ELF with new section to alloc VMA
#riscv64-unknown-elf-ld -o ../phoenix-riscv64.elf -nostdlib -e _start --section-start .init=0x3fc0000000 \
#	-Tbss=0x0000003fc0029000 ../phoenix-riscv64-withprograms.elf 


#cp ../phoenix-riscv64-withprograms.elf ../phoenix-riscv64.elf

#echo "Image file `basename ${OUTPUT}` has been created"
