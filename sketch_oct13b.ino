/*
  This file is part of esp-find3-client by Sylwester aka DatanoiseTV.
  The original source can be found at https://github.com/DatanoiseTV/esp-find3-client.

  esp-find3-client is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
  
  esp-find3-client is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with esp-find3-client.  If not, see <http://www.gnu.org/licenses/>.
*/

//#include <WiFiClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>

const char* ssid     = "Xiaomi_duoduo";
const char* password = "duoduo2011";
const char* host = "192.168.31.131";

// Uncomment to set to learn mode
// #define MODE_LEARNING 1
#define LOCATION "living room"
#define GROUP_NAME "home"

// Important! BLE + WiFi Support does not fit in standard partition table.
// Manual experimental changes are needed.
// See https://desire.giesecke.tk/index.php/2018/01/30/change-partition-size/

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>

#define BLE_SCANTIME 5
BLEScan* pBLEScan;

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
       Serial.printf("Advertised Device: %s \n", advertisedDevice.toString().c_str());
    }
};

// Enable Deepsleep for power saving. This will make it run less often.
// #define USE_DEEPSLEEP 1
#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
// 20 currently results in an interval of 45s
#define TIME_TO_SLEEP  20        /* Time ESP32 will go to sleep (in seconds) */
#define DEBUG 1

String chipIdStr;



void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("Find3 ESP client by DatanoiseTV (WiFi + BLE support.)");

  chipIdStr = String((uint32_t)(ESP.getEfuseMac()>>16));
  Serial.print("[ INFO ]\tChipID is: ");
  Serial.println(chipIdStr);

  WiFi.begin(ssid, password);
  Serial.println("[ INFO ]\tConnecting to WiFi..");

  while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
  }
    Serial.println("[ INFO ]\tWiFi connection established.");
    Serial.print("[ INFO ]\tIP address: ");
    Serial.println(WiFi.localIP());
  
  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan(); // create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); // active scan uses more power, but get results faster
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);  // less or equal setInterval value
}


void SubmitWiFi(void)
{
  String request;

  DynamicJsonDocument jdoc(512);

  JsonObject root = jdoc.to<JsonObject>();
  root["d"] = chipIdStr;
  root["f"] = GROUP_NAME;
  JsonObject data = root.createNestedObject("s");
  serializeJsonPretty(jdoc, Serial);

    Serial.println("[ INFO ]\tBLE scan starting..");
    
    BLEScanResults foundDevices = pBLEScan->start(BLE_SCANTIME);
    Serial.print("[ INFO ]\t");
    Serial.print(foundDevices.getCount());
    Serial.println(" BLE devices found.");

    JsonObject bt_network = data.createNestedObject("bluetooth");
    for(int i=0; i<foundDevices.getCount(); i++)
    {
      BLEAdvertisedDevice aDevice = foundDevices.getDevice(i);
      //std::string mac = foundDevices.getDevice(i).getAddress().toString();
      if (aDevice.haveName()) {
        Serial.print(aDevice.getName().c_str());
        bt_network[(String)aDevice.getName().c_str()] = (int)foundDevices.getDevice(i).getRSSI();
      }
      
    }
    pBLEScan->clearResults();   // delete results fromBLEScan buffer to release memory

    #ifdef MODE_LEARNING
      root["l"] = LOCATION;
    #endif
    
    serializeJson(jdoc, request);
    serializeJsonPretty(jdoc, Serial);

    #ifdef DEBUG
    Serial.println(request);
    #endif
        
    WiFiClient client;
    const int httpPort = 8005;
    if (!client.connect(host, httpPort)) {
      Serial.println("connection failed");
      return;
    }

    // We now create a URI for the request
    String url = "/passive";

    Serial.print("[ INFO ]\tRequesting URL: ");
    Serial.println(url);

    // This will send the request to the server
    client.print(String("POST ") + url + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "Content-Type: application/json\r\n" +
                 "Content-Length: " + request.length() + "\r\n\r\n" +
                 request +
                 "\r\n\r\n"
                );

    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 5000) {
        Serial.println("[ ERROR ]\tHTTP Client Timeout !");
        client.stop();
        return;
      }
    }

    // Check HTTP status
    char status[60] = {0};
    client.readBytesUntil('\r', status, sizeof(status));
    if (strcmp(status, "HTTP/1.1 200 OK") != 0) {
      Serial.print(F("[ ERROR ]\tUnexpected Response: "));
      Serial.println(status);
      return;
    }
    else
    {
      Serial.println(F("[ INFO ]\tGot a 200 OK."));
    }

   char endOfHeaders[] = "\r\n\r\n";
   if (!client.find(endOfHeaders)) {
    Serial.println(F("[ ERROR ]\t Invalid Response"));
    return;
   }
   else
   {
    Serial.println("[ INFO ]\tLooks like a valid response.");
   }

   Serial.println("[ INFO ]\tClosing connection.");
   Serial.println("=============================================================");

   #ifdef USE_DEEPSLEEP
   esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
   esp_deep_sleep_start();
   #endif
  
}

void loop() {
  SubmitWiFi();
  delay(20000);
}
