#!/bin/bash
#
# Copyright (c) 2015-2018 Zubax Robotics, zubax.com
# Distributed under the MIT License, available in the file LICENSE.
# Author: Pavel Kirienko <pavel.kirienko@zubax.com>
#

function die()
{
    echo "$@" 1>&2
    exit 1
}

[[ $EUID -ne 0 ]] || die "Do NOT run this script as root.
If you resorted to root because the debugger cannot be accessed by a regular user, \
follow this guide to configure the access perimissions correctly: https://kb.zubax.com/x/N4Ah"

PORT="$1"
if [ -z "$PORT" ]
then
    if [ "$(uname)" == "Darwin" ]
    then
        PORT=$(ls /dev/cu.usb[sm][eo][rd][ie][am]* | head -n 1)
    else
        PORT=$(readlink -f /dev/serial/by-id/*Black*Magic*Probe*0)
    fi

    [ -e "$PORT" ] || die "Debugger not found"
    echo "Using port $PORT"
fi

# Find the firmware ELF
elf="$2"
if [ ! -e "$elf" ]; then
	elf=$(ls ../../build/*.elf 2>/dev/null)
	if [ ! -e "$elf" ]; then
	    elf=$(ls build/*.elf 2>/dev/null)
	fi
fi
[ -e "$elf" ] && echo "Using ELF file $elf" || die "ELF file could not be found"

arm-none-eabi-size $elf || die "Could not check the size of the binary"

tmpfile=.blackmagic_gdb.tmp
cat > $tmpfile <<EOF
target extended-remote $PORT
mon swdp_scan
attach 1
load
kill
EOF

# Key -n to ignore .gdbinit
arm-none-eabi-gdb $elf -n --batch -x $tmpfile

rm -f $tmpfile
