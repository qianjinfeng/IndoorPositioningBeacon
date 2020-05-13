# Locator

Collect eddystone signal (0x00 id, 0x20 tlm) and Route bluetooth information to Home Assistant mqtt_room


## How to use 

### Configure the project

```
idf.py menuconfig
```

* Set serial port under Serial Flasher Options.

* customize partitions: partitions

### Flash Spiffs

* 0x10000 size and 0x3F0000 start address of SPIFFS is defined in partitions.csv
```
mkspiffs -c [a foder with config.txt] -b 4096 -p 256 -s 0x10000 spiffs.bin
python esptool.py --chip esp32 --port [port] --baud [baud] write_flash -z 0x3F0000 spiffs.bin
```
..\..\..\..\components\esptool_py\esptool\esptool.py -p COM4 -b 115200 write_flash -z 0x3F0000 build\spiffs.bin

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py build  (python2 $(which idf.py) build)
idf.py -p /dev/ttyUSB0 flash monitor 
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.

## Sample Data

config.txt
wifissid
wifipassword
location
mqttip





