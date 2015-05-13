#!/bin/sh
avrdude -p atmega328 -c avrispmkII -P usb -U efuse:w:0b100:m -U hfuse:w:0b11011110:m -U lfuse:w:0b11010111:m -U flash:w:optiboot_abspromini.hex
