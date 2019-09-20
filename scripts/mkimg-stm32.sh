#!/bin/bash
# Phoenix-RTOS
#
# Operating system kernel
#
# Creates syspage for STM32 based on given apps.
#
# Copyright 2017-2019 Phoenix Systems
# Author: Aleksander Kaminski
#
# This file is part of Phoenix-RTOS.

# This script creates image of Phoenix-RTOS kernel, syspage and aplications for STM32 platform.
# Created image can be programmed directly to the device.
# Usage:
# $1      - path to Phoenix-RTOS kernel ELF
# $2      - kernel argument string
# $2      - output file name
# $3, ... - applications ELF(s)
# example: ./mkimg-stm32.sh phoenix-armv7-stm32.elf "argument" flash.img app1.elf app2.elf


reverse() {
	num=$1
	printf "0x"
	for i in 1 2 3 4; do
		printf "%02x" $(($num & 0xff))
		num=$(($num>>8))
	done
}


if [ -z "$CROSS" ]
then
	CROSS="arm-phoenix-"
fi


KERNELELF=$1
shift

KARGS=$1
shift

OUTPUT=$1
shift

GDB_SYM_FILE=`dirname ${OUTPUT}`"/gdb_symbols"

SIZE_PAGE=$((0x200))
PAGE_MASK=$((0xfffffe00))
KERNEL_END=$((`readelf -l $KERNELELF | grep "LOAD" | grep "R E" | awk '{ print $6 }'`))
FLASH_START=$((0x08000000))
SYSPAGE_OFFSET=$((512))

declare -i i
declare -i j

i=$((0))

rm -f *.img
rm -f syspage.hex syspage.bin
rm -f $OUTPUT

prognum=$((`echo $@ | wc -w`))

printf "%08x%08x" $((`reverse 0x20000000`)) $((`reverse 0x20014000`)) >> syspage.hex
printf "%08x%08x" $((`reverse $(($FLASH_START + $SYSPAGE_OFFSET + 16 + ($prognum * 24)))`)) $((`reverse $prognum`)) >> syspage.hex
i=16

OFFSET=$(($FLASH_START+$KERNEL_END))
OFFSET=$((($OFFSET+$SIZE_PAGE-1)&$PAGE_MASK))

for app in $@; do
	echo "Proccessing $app"

	printf "%08x" $((`reverse $OFFSET`)) >> syspage.hex #start

	cp $app tmp.elf
	${CROSS}strip tmp.elf
	SIZE=$((`du -b tmp.elf | cut -f1`))
	rm -f tmp.elf
	END=$(($OFFSET+$SIZE))
	printf "%08x" $((`reverse $END`)) >> syspage.hex #end
	i=$i+8

	OFFSET=$((($OFFSET+$SIZE+$SIZE_PAGE-1)&$PAGE_MASK))

	j=0
	for char in `basename "$app" | sed -e 's/\(.\)/\1\n/g'`; do
		printf "%02x" "'$char" >> syspage.hex
		j=$j+1
	done

	for (( ; j<16; j++ )); do
		printf "%02x" 0 >> syspage.hex
	done

	i=$i+16
done

# Kernel arg
echo -n $KARGS | xxd -p >> syspage.hex
echo -n "00" >> syspage.hex

# Use hex file to create binary file
xxd -r -p syspage.hex > syspage.bin

# Make kernel binary image
${CROSS}objcopy $KERNELELF -O binary kernel.img

cp kernel.img $OUTPUT

dd if="syspage.bin" of=$OUTPUT bs=1 seek=$SYSPAGE_OFFSET conv=notrunc 2>/dev/null

OFFSET=$(($KERNEL_END))
OFFSET=$((($OFFSET+$SIZE_PAGE-1)&$PAGE_MASK))

[ -f $GDB_SYM_FILE ] && rm -rf $GDB_SYM_FILE
printf "file %s \n" `realpath $KERNELELF` >> $GDB_SYM_FILE

for app in $@; do
	cp $app tmp.elf
	${CROSS}strip tmp.elf
	printf "App %s @offset 0x%08x\n" $app $OFFSET
	printf "add-symbol-file %s 0x%08x\n" `realpath $app` $((OFFSET + $FLASH_START + $((0xc0)))) >> $GDB_SYM_FILE
	dd if=tmp.elf of=$OUTPUT bs=1 seek=$OFFSET 2>/dev/null
	OFFSET=$((($OFFSET+$((`du -b tmp.elf | cut -f1`))+$SIZE_PAGE-1)&$PAGE_MASK))
	rm -f tmp.elf
done

#Convert binary image to hex
${CROSS}objcopy --change-addresses $FLASH_START -I binary -O ihex ${OUTPUT} ${OUTPUT%.*}.hex

rm -f kernel.img
rm -f syspage.bin
rm -f syspage.hex

echo "Image file `basename ${OUTPUT}` has been created"
