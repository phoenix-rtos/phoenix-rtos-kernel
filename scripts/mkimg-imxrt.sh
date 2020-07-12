#!/bin/bash
# Phoenix-RTOS
#
# Operating system kernel
#
# Creates syspage for i.MX RT based on given apps.
#
# Copyright 2017-2019 Phoenix Systems
# Author: Aleksander Kaminski
#
# This file is part of Phoenix-RTOS.

# This script creates image of Phoenix-RTOS kernel, syspage and aplications for i.MX RT platform.
# Created image can be programmed directly to the device.
# Usage:
# $1      - path to Phoenix-RTOS kernel ELF
# $2      - kernel argument string
# $3      - output file name
# $4, ... - applications ELF(s)
# example: ./mkimg-imxrt.sh phoenix-armv7-imxrt.elf "Xapp1.elf Xapp2.elf" flash.img app1.elf app2.elf

set -e

reverse() {
	local num=$1 i
	for i in 1 2 3 4; do
		printf "%02x" $(($num & 0xff))
		num=$(($num>>8))
	done
}


CROSS=${CROSS:-arm-phoenix-}


if [ "$1" == "-h" -o $# -lt 3 ]; then
	echo "usage: $0 <kernel ELF> <kernel args> <output file> [[app] ... ]"
	exit 1
fi

KERNELELF=$1
KARGS=$2
OUTPUT=$3
shift 3

GDB_SYM_FILE="$(dirname ${OUTPUT})/gdb_symbols"

STRIP_OPT="--strip-unneeded -R .symtab"

SIZE_PAGE=$((0x200))
PAGE_MASK=$((0xfffffe00))
KERNEL_END=$(($(readelf -l $KERNELELF | awk '/LOAD/ && /R E/ { print $6 }')))
FLASH_START=$((0x00000000))
SYSPAGE_OFFSET=$((512))

rm -f *.img syspage.hex syspage.bin "$OUTPUT"

prognum=$#

reverse 0x20000000 >> syspage.hex
reverse 0x20040000 >> syspage.hex
reverse $(($FLASH_START + $SYSPAGE_OFFSET + 16 + ($prognum * 28))) >> syspage.hex
reverse $prognum >> syspage.hex

OFFSET=$(($FLASH_START + $KERNEL_END))
OFFSET=$((($OFFSET + $SIZE_PAGE - 1)&$PAGE_MASK))

for arg in "$@"; do
	IFS=';' read -r app map <<< "$arg"
	map=$(($map))
	echo "Proccessing $app"

	reverse $OFFSET >> syspage.hex #start

	cp $app tmp.elf
	${CROSS}strip $STRIP_OPT tmp.elf
	echo "${CROSS}strip $STRIP_OPT tmp.elf"
	SIZE=$(stat -c "%s" tmp.elf)
	rm -f tmp.elf
	END=$(($OFFSET + $SIZE))
	reverse $END >> syspage.hex #end
	reverse $map >> syspage.hex #mapno

	OFFSET=$((($OFFSET + $SIZE + $SIZE_PAGE - 1)&$PAGE_MASK))

	app=${app##*/}
	for ((j=0; j < 16; j++)); do
		printf "%02x" \'${app:j:1} >> syspage.hex
	done

done

# Kernel arg
echo -n $KARGS | xxd -p >> syspage.hex
echo -n "00" >> syspage.hex

# Use hex file to create binary file
xxd -r -p syspage.hex > syspage.bin

# Make kernel binary image
${CROSS}objcopy $KERNELELF -O binary $OUTPUT

dd if="syspage.bin" of=$OUTPUT bs=1 seek=$SYSPAGE_OFFSET conv=notrunc 2>/dev/null

OFFSET=$((($KERNEL_END + $SIZE_PAGE - 1)&$PAGE_MASK))

rm -f $GDB_SYM_FILE
printf "file %s \n" $(realpath $KERNELELF) > $GDB_SYM_FILE

for arg in "$@"; do
	app=${arg%%;*}
	cp $app tmp.elf
	${CROSS}strip $STRIP_OPT tmp.elf
	printf "App %s @offset 0x%08x\n" $app $OFFSET
	ELFOFFS=$(($(readelf -l $app | awk '/LOAD/ && /R E/ { print $2 }')))
	printf "add-symbol-file %s 0x%08x\n" $(realpath $app) $((OFFSET + $FLASH_START + $ELFOFFS)) >> $GDB_SYM_FILE
	dd if=tmp.elf of=$OUTPUT bs=1 seek=$OFFSET 2>/dev/null
	OFFSET=$((($OFFSET + $(stat -c "%s" tmp.elf) + $SIZE_PAGE - 1)&$PAGE_MASK))
	rm -f tmp.elf
done

#Convert binary image to hex
${CROSS}objcopy --change-addresses $FLASH_START -I binary -O ihex ${OUTPUT} ${OUTPUT%.*}.hex

rm -f syspage.bin syspage.hex

echo "Image file $(basename ${OUTPUT}) has been created"
