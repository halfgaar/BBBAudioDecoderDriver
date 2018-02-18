#!/bin/bash


echo "You are about to write the BBBAUdioDecoder eeprom into I2c bus 2, address 0x54."
read -p "Continue (y/n): " answer

if [ "$answer" != y ]; then
  echo Quiting...
  exit 1
fi

echo "Writing..."
cat data.eeprom > /sys/bus/i2c/devices/2-0054/eeprom
