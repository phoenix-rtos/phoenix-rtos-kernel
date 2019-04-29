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
# $2      - output file name
# $3, ... - applications ELF(s)
# example: ./mkimg-stm32.sh phoenix-armv7-stm32.elf flash.img app1.elf app2.elf

if [ -z "$CROSS" ]
then
    CROSS="arm-phoenix-"
fi


KERNELELF=$1
shift

OUTPUT=$1
shift

GDB_SYM_FILE=`dirname ${OUTPUT}`"/gdb_symbols"

SIZE_PAGE=$((0x200))
PAGE_MASK=$((0xfffffe00))
KERNEL_END=$((`readelf -l $KERNELELF | grep "LOAD" | grep "R E" | awk '{ print $6 }'`))
FLASH_START=$((0x08000000))
APP_START=$((0x08010000))

declare -i i
declare -i k

i=$((0))


if [ $KERNEL_END -gt $(($APP_START-$FLASH_START)) ]; then
	echo "Kernel image is bigger than expected!"
	printf "Kernel end: 0x%08x > App start: 0x%08x\n" $KERNEL_END $APP_START
	exit 1
fi

rm -f *.img
rm -f syspage.hex syspage.bin
rm -f $OUTPUT

printf "%08x %08x\n" $i 0x20000000 >> syspage.hex #pbegin
i=$i+4
printf "%08x %08x\n" $i 0x20014000 >> syspage.hex #pend
i=$i+4
printf "%08x %08x\n" $i 0 >> syspage.hex #arg
i=$i+4
printf "%08x %08x\n" $i $((`echo $@ | wc -w`)) >> syspage.hex #progssz
i=$i+4

#Find syspage size first
SYSPAGESZ=$(($i+($((`echo $@ | wc -w`))*$((24)))))
SYSPAGESZ=$((($SYSPAGESZ+$SIZE_PAGE-1)&$PAGE_MASK))

OFFSET=$(($APP_START+$SYSPAGESZ))

for app in $@; do
	echo "Proccessing $app"
	k=0

	START=$(($OFFSET-$APP_START-$(($i-$k))))
	printf "%08x %08x\n" $i $START >> syspage.hex #start
	i=$i+4
	k=$k+4

	cp $app tmp.elf
	${CROSS}strip tmp.elf
	SIZE=$((`du -b tmp.elf | cut -f1`))
	rm -f tmp.elf
	END=$(($START+$SIZE))
	printf "%08x %08x\n" $i $END >> syspage.hex #end
	i=$i+4
	k=$k+4

	OFFSET=$((($OFFSET+$SIZE+$SIZE_PAGE-1)&$PAGE_MASK))

	for (( j=0; j<4; j++)); do
		printf "%08x %08x\n" $i 0 >> syspage.hex #cmdline
		i=$i+4
		k=$k+4
	done
done

# Use hex file to create binary file
xxd -g4 -c4 -r syspage.hex > syspage.bin

# Convert to little endian
objcopy --reverse-bytes=4 -I binary syspage.bin -O binary syspage.bin

# Make kernel binary image
${CROSS}objcopy $KERNELELF -O binary kernel.img

cp kernel.img $OUTPUT

OFFSET=$(($APP_START-$FLASH_START))
dd if="syspage.bin" of=$OUTPUT bs=1 seek=$OFFSET 2>/dev/null
OFFSET=$(($OFFSET+$SYSPAGESZ))

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
