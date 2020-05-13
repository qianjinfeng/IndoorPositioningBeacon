

# Minimum configurations in configuration.yaml
lovelace:
  mode: yaml

mqtt:
  broker: 139.24.185.184
  port: 1883
  username: mqtt
  password: mqtt

recorder:
  purge_interval: 2
  purge_keep_days: 5
  
sensor: !include_dir_merge_list sensors
binary_sensor: !include_dir_merge_list binary_sensors

logger:
  default: warning
  logs:
    homeassistant.components.mqtt: info
    homeassistant.components.yamaha: critical
    custom_components.my_integration: critical
    homeassistant.components.mqtt: warning



sensor:
# One entry for each beacon you want to track
  - platform: mqtt
    state_topic: "battery_status/address"  //bluetooth address
    name: "Battery Tablet"
    unit_of_measurement: "mV"
    value_template: '{{ value_json.batt }}'
    device_class: power

sensor:
# One entry for each beacon you want to track
  - platform: mqtt_room
    device_id: "fda50693a4e24fb1a"  # bluetooth address
    name: 'iBeacon Room Presence'   # Asset number
    state_topic: 'room_presence'   # Publish to room_presence/room2  {"id":"", "distance": 5.67}
    timeout: 60
    away_timeout: 120


binary_sensor:
# One entry per sensor node to understand when the device is online/offline and see device metadata such as IP address and settings values
  - platform: mqtt
    name: ESP32 A
    state_topic: "presence_nodes/esp32_a"       # Publish to presence_nodes/esp32_a  {}
    json_attributes_topic: "presence_nodes/esp32_a/tele"
    payload_on: "CONNECTED"
    payload_off: "DISCONNECTED"
    device_class: connectivity