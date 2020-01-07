# WiFi station example

(See the README.md file in the upper level 'examples' directory for more information about examples.)


## How to use example

### Configure the project

```
idf.py menuconfig
```

* Set serial port under Serial Flasher Options.

* Set WiFi SSID and WiFi Password and Maximum retry under Example Configuration Options.

* customize partitions

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

cabin24
bluetooth-ee:2c:dd:e6:ca:4c 566710009943
bluetooth-cd:06:5f:9d:9f:cd 566710009949
cabin20
bluetooth-f8:22:22:c5:b4:32 566710009953 
bluetooth-c0:7d:81:15:a4:7e 566710011561
cabin12
bluetooth-f6:6c:34:cc:c7:34 566710011427
cabin11
bluetooth-ca:d6:e9:31:ae:b8 566710011562
cabin9
bluetooth-f0:34:52:77:5c:d6 566710007314 
cabin7
bluetooth-fa:74:4d:86:51:dd 566710007317
bluetooth-c0:11:03:47:ec:b0 566710007315
cabin5
bluetooth-f2:91:5c:42:b4:e3 566710007236
B1L2BeiJing
bluetooth-c4:9d:c3:9a:fa:8e 566710004059
B1L2InnovationRoom
bluetooth-cf:9a:d7:f2:5c:ef 566710009940




