#!/bin/bash
#
# Copyright (c) 2015 Zubax Robotics, zubax.com
# Distributed under the MIT License, available in the file LICENSE.
# Author: Pavel Kirienko <pavel.kirienko@zubax.com>
#

PORT="$1"
if [ -z "$PORT" ]
then
    if [ "$(uname)" == "Darwin" ]
    then
        PORT=$(ls /dev/cu.usb[sm][eo][rd][ie][am]* | head -n 1)
    else
        PORT=$(readlink -f /dev/serial/by-id/*Black*Magic*Probe*0)
    fi

    [ -z "$PORT" ] && exit 1
    echo "Using port: $PORT"
fi

# Find the firmware ELF
elf=$(ls ../../build/*.elf 2>/dev/null)
if [ -z "$elf" ]; then
    elf=$(ls build/*.elf 2>/dev/null)
fi
if [ -z "$elf" ]; then
    echo "No firmware found"
    exit 1
fi

arm-none-eabi-size $elf || exit 1

tmpfile=.blackmagic_gdb.tmp
cat > $tmpfile <<EOF
target extended-remote $PORT
mon swdp_scan
attach 1
load
kill
EOF

arm-none-eabi-gdb $elf --batch -x $tmpfile

rm -f $tmpfile
