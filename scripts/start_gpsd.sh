#!/bin/bash -e
# Start up gpsd

baudrate=38400

if [ $1 ];
then
    serial_port=$1
else
    serial_port=/dev/ttyUSB0
fi

if [ ! -e "$serial_port" ]
then
  echo "Cannot run GPS..."
  exit
fi

echo Using ${serial_port}...

echo "Configuring ${serial_port} for ${baudrate}..."
stty -F ${serial_port} 38400 -echo raw
sleep 1
echo "Configuration completed!"

echo "Launching gpsd..."
cd ../src/submodules/gpsd
./gpsd ${serial_port} &
echo "gpsd runnning!"
