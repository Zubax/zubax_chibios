#!/bin/bash

if ! which sz; then
    echo "Command 'sz' is not available; please install it and try again."
    exit 1
fi

file=$1
if [[ $file == "" ]]; then
    echo "Usage:"
    echo "    `basename $0` <file> [serial-port]"
    echo "The serial port will be detected automatically if not specified explicitly."
    exit 1
fi

set -e

port=$2
if [[ $port == "" ]]; then
    matching_ports=`ls /dev/serial/by-id/* | grep -i Zubax`
    num_matching_ports=`echo $matching_ports | wc -l`

    if [[ $num_matching_ports < 1 ]]; then
        echo "Could not find matching serial port."
        exit 1
    fi
    if [[ $num_matching_ports > 1 ]]; then
        echo "Too many matching serial ports, please select one manually. The ports are:"
        echo "$matching_ports"
        exit 1
    fi

    port="$matching_ports"
fi

echo "Serial port: $port"
stty -F $port 115200

echo "Configuring the device..."
echo -e "\rbootloader\r" > $port
sleep 3
echo -e "\rwait\r" > $port
echo -e "\rdownload\r" > $port

echo "Starting YMODEM sender..."
sz -vv --ymodem --1k $file > $port < $port
