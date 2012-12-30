#!/bin/bash
FILENAME="$1"
if [ -z "$FILENAME" ]; then FILENAME=`echo *.hex`; fi
avrdude -e -V -u -y -P usb -p m32u4 -c usbtiny -U lfuse:w:0xCE:m -U hfuse:w:0x99:m -U efuse:w:0xF9:m -U lock:w:0xFF:m
avrdude -e -V -u -y -P usb -p m32u4 -c usbtiny -U "flash:w:$FILENAME:i"
