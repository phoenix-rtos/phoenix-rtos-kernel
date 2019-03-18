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
KERNEL_END=$((`readelf -l $KERNELELF | grep "LOAD" | grep "R E" | awk '{ print $6 }'`))
FLASH_START=$((0x08000000))
APP_START=$((0x08010000))

declare -i i
declare -i k

i=$((0))


if [ $KERNEL_END -gt $APP_START ]; then
	echo "Kernel image is bigger than expected!"
	printf "Kernel end: 0x%08x > App start: 0x%08x\n" $KERNEL_END $APP_START
	exit 1
fi

rm -f *.img
rm -f syspage.hex syspage.bin
rm -f $OUTPUT

printf "%08x %08x\n" $i 0 >> syspage.hex #arg
i=$i+4
printf "%08x %08x\n" $i $((`echo $@ | wc -w`)) >> syspage.hex #progssz
i=$i+4

#Find syspage size first
SYSPAGESZ=$i
for app in $@; do
	SYSPAGESZ=$SYSPAGESZ+24
	SEGMENTS=`readelf -l $app | grep "LOAD" | wc -l`

	for (( j=1; j<=$SEGMENTS; j++ )); do
		SYSPAGESZ=$SYSPAGESZ+24
	done
done
SYSPAGESZ=$((($SYSPAGESZ+$SIZE_PAGE-1)&$((0xffffff00))))

OFFSET=$(($APP_START+$SYSPAGESZ))

for app in $@; do
	echo "Proccessing $app"
	k=0

	ENTRY=$((`readelf -l $app | grep "Entry point" | awk '{ print $3 }'`))
	ENTRY=$(($ENTRY+$OFFSET-$APP_START-$(($i-$k))))
	printf "%08x %08x\n" $i $ENTRY >> syspage.hex #entry
	i=$i+4
	k=$k+4

	SEGMENTS=`readelf -l $app | grep "LOAD" | wc -l`
	printf "%08x %08x\n" $i $SEGMENTS >> syspage.hex #hdrssz
	i=$i+4
	k=$k+4

	printf "%08x " $i >> syspage.hex
	echo `readelf -S $app | grep ".got" | awk '{ print $5 }'` >> syspage.hex #got
	i=$i+4
	k=$k+4

	printf "%08x " $i >> syspage.hex
	echo -n "00" >> syspage.hex
	echo `readelf -S $app | grep ".got" | awk '{ print $7 }'` >> syspage.hex #gotsz
	i=$i+4
	k=$k+4

	printf "%08x %08x\n" $i $(($OFFSET-$APP_START-$(($i-$k)))) >> syspage.hex #offset
	i=$i+4
	k=$k+4

	${CROSS}objcopy $app -Obinary tmpsize.img
	SIZE=$((`du -b tmpsize.img | awk '{ print $1 }'`))
	rm -f tmpsize.img
	printf "%08x %08x\n" $i $(($SIZE)) >> syspage.hex #size
	i=$i+4
	k=$k+4

	printf "%08x %08x\n" $i 0 >> syspage.hex #cmdline
	i=$i+4
	k=$k+4

	for (( j=1; j<=$SEGMENTS; j++ )); do
		LINE=`readelf -l $app | grep "LOAD" | sed -n ${j}p`

		VIRTADDR=`echo $LINE | awk '{ print $3 }'`
		PHYSADDR=`echo $LINE | awk '{ print $4 }'`
		FILESIZE=`echo $LINE | awk '{ print $5 }'`
		MEMSIZE=`echo $LINE | awk '{ print $6 }'`
		ALIGN=`echo $LINE | rev | cut -d ' ' -f 1 | rev`

		FLAGS=`echo $LINE | grep -o '[RWE]' | tr -d '\n'`
		FLAGSBIN=$((2))
		if [ `echo $FLAGS | grep "W"` ]; then
			FLAGSBIN=$(($FLAGSBIN+1))
		fi
		if [ `echo $FLAGS | grep "E"` ]; then
			FLAGSBIN=$(($FLAGSBIN+4))
			PHYSADDR=$(($OFFSET-$APP_START-$(($i-$k))))
		else
			PHYSADDR=$((0))
		fi

		printf "%08x %08x\n" $i $((PHYSADDR)) >> syspage.hex #addr
		i=$i+4
		k=$k+4
		printf "%08x %08x\n" $i $((MEMSIZE)) >> syspage.hex #memsz
		i=$i+4
		k=$k+4
		printf "%08x %08x\n" $i $((FLAGSBIN)) >> syspage.hex #flags
		i=$i+4
		k=$k+4
		printf "%08x %08x\n" $i $((VIRTADDR)) >> syspage.hex #vaddr
		i=$i+4
		k=$k+4
		printf "%08x %08x\n" $i $((FILESIZE)) >> syspage.hex #filesz
		i=$i+4
		k=$k+4
		printf "%08x %08x\n" $i $((ALIGN)) >> syspage.hex #align
		i=$i+4
		k=$k+4
	done

	${CROSS}objcopy $app -O binary tmp.img
	OFFSET=$((($OFFSET+$((`du -b tmp.img | cut -f1`))+$SIZE_PAGE-1)&$((0xffffff00))))
	rm -f tmp.img
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
	${CROSS}objcopy $app -O binary tmp.img
	printf "App %s @offset 0x%08x\n" $app $OFFSET
	printf "add-symbol-file %s 0x%08x\n" `realpath $app` $((OFFSET + $FLASH_START)) >> $GDB_SYM_FILE
	dd if=tmp.img of=$OUTPUT bs=1 seek=$OFFSET 2>/dev/null
	OFFSET=$((($OFFSET+$((`du -b tmp.img | cut -f1`))+$SIZE_PAGE-1)&$((0xffffff00))))
	rm -f tmp.img
done

#Convert binary image to hex
${CROSS}objcopy --change-addresses $FLASH_START -I binary -O ihex ${OUTPUT} ${OUTPUT%.*}.hex

rm -f kernel.img
rm -f syspage.bin
rm -f syspage.hex

echo "Image file `basename ${OUTPUT}` has been created"
