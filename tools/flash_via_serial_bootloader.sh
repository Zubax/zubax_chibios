#!/bin/bash

if ! which sz > /dev/null; then
    echo "Command 'sz' is not available; please install it and try again."
    echo "On Debian-based systems, use package 'lrzsz'."
    exit 1
fi

file=$1
device_uid_or_port_name=$2

if [[ $file == "" ]]; then
    echo "Usage:"
    echo "    `basename $0` <file> [part-of-device-UID | port-name]"
    echo "The serial port will be detected automatically using the provided device UID (or any part of it)."
    echo "If the device UID is not provided, the script will fail unless there is only one Zubax device connected."
    echo "If port name is specified instead of the device ID, it will be used instead."
    echo "Example:"
    echo "    `basename $0` com.zubax.babel-1.0-1.0.a6b50c4.application.bin 380032000D57324133363920"
    echo "    `basename $0` com.zubax.babel-1.0-1.0.a6b50c4.application.bin /dev/ttyUSB0"
    exit 1
fi

function detect_port()
{
    if [[ $device_uid_or_port_name == /* ]]; then
        matching_ports="$device_uid_or_port_name"
    else
        matching_ports=`ls /dev/serial/by-id/* | grep -i Zubax | grep -i "$device_uid_or_port_name"`
    fi

    num_matching_ports=`echo $matching_ports | wc -l`
    if [ -z "$matching_ports" ] || [[ $num_matching_ports < 1 ]]; then
        echo "Could not find matching serial port."
        exit 1
    fi
    if [[ $num_matching_ports > 1 ]]; then
        echo "Too many matching serial ports. The ports are:"
        echo "$matching_ports"
        exit 1
    fi

    port="$matching_ports"
    echo "Using serial port: $port"
}

# Configuring the serial port
echo "Configuring the serial port..."
detect_port
stty -F $port 115200

# Starting the bootloader
echo "Configuring the device..."
echo -e "\rbootloader\r" > $port
sleep 3

# Re-detecting the serial port, because if it is a USB CDC ACM port, it will re-appear, likely under a different name
detect_port

set -e

# Telling the bootloader to receive the new firmware image
echo -e "\rwait\r" > $port
echo -e "\rdownload\r" > $port

# Transferring the new image. The bootloader will boot it automatically if it is correct.
echo "Starting YMODEM sender..."
sz -vv --ymodem --1k $file > $port < $port
