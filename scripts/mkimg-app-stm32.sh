#!/bin/bash
# Phoenix-RTOS
#
# Operating system kernel
#
# Creates syspage for STM32 based on given apps.
#
# Copyright 2017 Phoenix Systems
# Author: Aleksander Kaminski
#
# This file is part of Phoenix-RTOS.

# This script creates image of syspage and aplications for STM32 platform
# Usage:
# $1 - path to Phoenix-RTOS kernel ELF
# $2 - application(s) ELFs
# example: ./mkimg-stm32.sh phoenix-armv7-stm32.elf app1.elf app2.elf ...

CROSS="arm-phoenix-"

OUTPUT="../app.flash"
SIZE_PAGE=$((0x200))
APP_START=$((0))

declare -i i
declare -i k

i=$((0))

rm -f *.img
rm -f syspage.hex syspage.bin
rm -f $OUTPUT

printf "%08x %08x\n" $i 0 >> syspage.hex #stack
i=$i+4
printf "%08x %08x\n" $i 0 >> syspage.hex #stacksize
i=$i+4
printf "%08x %08x\n" $i $((`echo $@ | wc -w`)) >> syspage.hex #progssz
i=$i+4

#Find syspage size first
SYSPAGESZ=$i
for app in $@; do
	SYSPAGESZ=$SYSPAGESZ+20
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
	echo `readelf -S $app | grep ".got" | awk '{ print $5 }'` >> syspage.hex
	i=$i+4
	k=$k+4

	printf "%08x " $i >> syspage.hex
	echo -n "00" >> syspage.hex
	echo `readelf -S $app | grep ".got" | awk '{ print $7 }'` >> syspage.hex
	i=$i+4
	k=$k+4

	printf "%08x %08x\n" $i $(($OFFSET-$APP_START-$(($i-$k)))) >> syspage.hex
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

OFFSET=$((0))
dd if="syspage.bin" of=$OUTPUT bs=1 seek=0 2>/dev/null
OFFSET=$(($OFFSET+$SYSPAGESZ))

for app in $@; do
	${CROSS}objcopy $app -O binary tmp.img
	printf "App %s @offset 0x%08x\n" $app $OFFSET
	dd if=tmp.img of=$OUTPUT bs=1 seek=$OFFSET 2>/dev/null
	OFFSET=$((($OFFSET+$((`du -b tmp.img | cut -f1`))+$SIZE_PAGE-1)&$((0xffffff00))))
	rm -f tmp.img
done

rm -f syspage.bin
rm -f syspage.hex

echo "Image file ${OUTPUT:3} has been created"
