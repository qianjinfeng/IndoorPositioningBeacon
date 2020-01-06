# WiFi softAP example

(See the README.md file in the upper level 'examples' directory for more information about examples.)


## How to use example

### Configure the project

```
python2 $(which idf.py) menuconfig
```

* Set serial port under Serial Flasher Options.

* Set WiFi SSID and WiFi Password and Maximal STA connections under Example Configuration Options.

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
python2 $(which idf.py) build
python2 $(which idf.py) -p /dev/ttyUSB0 flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.

## Example Output

There is the console output for this example:

```
I (917) phy: phy_version: 3960, 5211945, Jul 18 2018, 10:40:07, 0, 0
I (917) wifi: mode : softAP (30:ae:a4:80:45:69)
I (917) wifi softAP: wifi_init_softap finished.SSID:myssid password:mypassword
I (26457) wifi: n:1 0, o:1 0, ap:1 1, sta:255 255, prof:1
I (26457) wifi: station: 70:ef:00:43:96:67 join, AID=1, bg, 20
I (26467) wifi softAP: station:70:ef:00:43:96:67 join, AID=1
I (27657) tcpip_adapter: softAP assign IP to station,IP is: 192.168.4.2
```
## Control

connect to ESP
set wifi ap
http POST http://192.168.4.1/api/wifi ap=Xiaomi_duoduo password=duoduo2011
http GET http://192.168.4.1/api/reboot
retrieve information
http GET http://192.168.4.1/api/info
0 learning 1 scanning
http POST http://192.168.4.1/api/mode mode:=1 family=ct location=cabin12 host=http://139.24.185.184:8005
set bluetooth scan parameter duration in seconds, other two in mseconds
http POST http://192.168.4.1/api/bluetooth duration:=20 interval:=5000 window:=3000

start learning or scanning   1 start  0 stop
http POST http://192.168.4.1/api/start value:=1

http GET http://139.24.185.184:8005/api/v1/battery/ct/566710009943
http POST http://139.24.185.184:8005/api/v1/settings/passive family=ct device=bluetooth-f2:91:5c:42:b4:e3 location=cabin5

http POST http://139.24.185.184:8005/api/v1/settings/passive family=ct device=bluetooth-ee:2c:dd:e6:ca:4c

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
beijing
c4:9d:c3:9a:fa:8e 566710004059
innovation
bluetooth-cf:9a:d7:f2:5c:ef 566710009940


http GET http://139.24.185.184:8005/api/v1/location_asset/ct/566710009949


mkspiffs -c [test foder] -b 4096 -p 256 -s 0x10000 spiffs.bin
python esptool.py --chip esp32 --port [port] --baud [baud] write_flash -z 0x3F0000 spiffs.bin
..\..\..\..\components\esptool_py\esptool\esptool.py -p COM4 -b 115200 write_flash -z 0x3F0000 build\spiffs.bin


