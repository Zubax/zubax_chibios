#!/bin/sh
#
# Copyright (c) 2015 Zubax Robotics, zubax.com
# Distributed under the MIT License, available in the file LICENSE.
# Author: Pavel Kirienko <pavel.kirienko@zubax.com>
#

# Find the firmware ELF
elf=$(ls -1 ../../build/*.elf)
if [ -z "$elf" ]; then
    elf=$(ls -1 build/*.elf)
fi
if [ -z "$elf" ]; then
    echo "No firmware found"
    exit 1
fi

size $elf || exit 1

bin="${elf%.*}.bin"
st-flash write $bin 8000000
