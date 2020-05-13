
# Introduction

Asset management through indoor positioning via bluetooth. 
![Schema](./schema.PNG)
 - Solution: HomeAssistant
 - Locator: ESP32 for gathering bluetooth signal
 - Beacon: nRF52832 for emitting bluetooth signal and representing assets 

# Technology


## HomeAssistant
 - mqtt sensor 
   battery sensor for asset tag
   ([mqtt_room](https://www.home-assistant.io/integrations/mqtt_room/)) sensor representing the location of the asset  
 - mqtt binary sensor for locator installed at fixed places 
 - lovelace page 
    ([entities-list](https://github.com/thomasloven/lovelace-auto-entities))

[code](./HomeAssitant)
Tips:
http GET http://139.24.185.184:8123/api/error_log  "Authorization: Bearer Token"
docker run -d --name=home-assistant -v /root/homeassistant:/config -p 8123:8123 homeassistant/home-assistant:stable
ufw allow proto tcp from 10.10.196.0/24 to 139.24.185.184 port 1883

## Locator
ESP32 development board (brough from taobao)
### installation
 - [ESP-IDF](https://docs.espressif.com/projects/esp-idf/zh_CN/stable/search.html) 

[code](./locator)

## Beacon 
[code](./nRF5_SDK_xxx)
- Development with ([nRF5 Series: Developing with SEGGER Embedded Studio](https://infocenter.nordicsemi.com/topic/ug_gsg_ses/UG/gsg/install_toolchain.html))  

- Specific Board nrf52832_tb with one button, one LED (brought from taobao, 30RMB) but available with jlink interface  

    board file [nrf52832_tb](./nRF5_SDK_xxx/components/boards/nrf52832_tb.h)  
    example project with this board [ble_app_eddystone](./nRF5_SDK_xxx/examples/ble_peripheral/ble_app_eddystone/nrf52832_tb/s132/ses/ble_app_eddystone_nrf52832_tb_s132.emProject)

- Major and Minor field are used for asset managmement  


## WeiChat MiniApp
[code](./eddystone)
a mobiled beacon scan