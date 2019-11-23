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

//#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
//#include <WiFiMulti.h>
//WiFiMulti wifiMulti;

const char* ssid     = "Xiaomi_duoduo";
const char* password = "duoduo2011";
const char* host = "192.168.31.131";
//const char* test_root_ca= ;

// You can use x.509 client certificates if you want
const char* test_client_key = \
 "-----BEGIN PRIVATE KEY-----\n" \
"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\n" \
"krN5Buz6rqjDa3WLl4p1QAqlVWrbbA==\n" \
"-----END PRIVATE KEY-----\n";

const char* test_client_cert = \
"-----BEGIN CERTIFICATE-----\n"
"MIIF9zCCA9+gAwIBAgIUWEUOntUjP0VyZy3I+JeFdfCsqP4wDQYJKoZIhvcNAQEL\n" \
"BQAwgYoxCzAJBgNVBAYTAkNOMREwDwYDVQQIDAhTaGFuZ2hhaTERMA8GA1UEBwwI\n" \
"U2hhbmdoYWkxDTALBgNVBAoMBHRlc3QxDDAKBgNVBAsMA2NwbDEXMBUGA1UEAwwO\n" \
"MTkyLjE2OC4zMS4xMzExHzAdBgkqhkiG9w0BCQEWEGd1eS5mZW5nQDE2My5jb20w\n" \
"HhcNMTkxMDIzMTA0MjU4WhcNMTkxMTIyMTA0MjU4WjCBijELMAkGA1UEBhMCQ04x\n" \
"ETAPBgNVBAgMCFNoYW5naGFpMREwDwYDVQQHDAhTaGFuZ2hhaTENMAsGA1UECgwE\n" \
"dGVzdDEMMAoGA1UECwwDY3BsMRcwFQYDVQQDDA4xOTIuMTY4LjMxLjEzMTEfMB0G\n" \
"CSqGSIb3DQEJARYQZ3V5LmZlbmdAMTYzLmNvbTCCAiIwDQYJKoZIhvcNAQEBBQAD\n" \
"ggIPADCCAgoCggIBANLUq8OsDyKxUTCFfUrq2U6WT9Oy0lKFUnpdZ1/wIkyPTXEv\n" \
"b76sDiZym2KJfD6agH6DURDaKnncyCdg0nMUqRXenCj8PFf3Y8XZmFglEje4kpRM\n" \
"+E7J9JuDOx1gpef4K41+RI+mn2S0wQ5D1Fd+b3NhcLwasoGM9euZIdrE7vwQhR9Q\n" \
"DBS724nXUluLuupQCbqCzE/7caEYgvBZc0yOQj3HM8PcPX7/FPiePuvRCYco2/wf\n" \
"MUF2mHjca9yYMTvd3NSOuyxQbC2yG3EsJF5LbPLKWfVe1cFMkJpwFCMhdwYxjIFX\n" \
"EVJzdY49ecQCtwPJsrXd85JX9FF6DxbSRXPXBr1tiYvPAO+pA0AmgbwkaFItxSJ/\n" \
"+CbKZ+EngzBC9kXfH53ai/jDnHzvBeqI3Vu0WcoNMvIrzgtBUZMvpLlPhORiE/Of\n" \
"8SDJIvEs0zS/iJkMw9S/EE53WF85dbx3QMfkza7vowTa1Bqtbqbxkw+vul0sf6cu\n" \
"cNBLa5FSNoY3zA2fmf2Sf05cwiVi9kr7Tu8udY87RvY13INNGOGPO/JMX/LD2RFw\n" \
"0H5InJ4D946zLbd+uB7NwVOXScjeab1nYxl5JAWEY9LtCSycsqePDoAKxOWsbVtI\n" \
"iOIN1lU5/bdX4nd9l6Rn2xUWHjBlCtHHluxoIb24mIi8PDcx9nZDQMUlYnVTAgMB\n" \
"AAGjUzBRMB0GA1UdDgQWBBTLSRG7KXbrPYTle3M62SHFetgOKjAfBgNVHSMEGDAW\n" \
"gBTLSRG7KXbrPYTle3M62SHFetgOKjAPBgNVHRMBAf8EBTADAQH/MA0GCSqGSIb3\n" \
"DQEBCwUAA4ICAQA3Pj9GV0J8mrn54w81TeQTOv3iN/FGJmTuOgUllc9pGPXbGByG\n" \
"BoCZORYWMThCCcp8lcpPvhr7sa2ffXT5W8oOKhCOvlLianNm44BXybPhgLZkWaQB\n" \
"SHaZBoq2zS/ACvJJq8LWiTtR/WZGhOpVbeXOe6a+ZzQJh9ChBJuItRy0Lq23bnBs\n" \
"l7BMWR9wNuPzIXh4sV3zg01+6qTwNgPpswjrLXSjoCprUhHUZ6jwGAg7UN07k9JD\n" \
"XlEv1nw/wUIpiSd4oJowDxb6dMOcHM6k8rnXWQfBgFlELgQsGwD7P62bnyligtA0\n" \
"q6k+P0nUOCmshOr6L7civqw3eDKUhbMkrpS4bb0bRMvkM+cs6Tn2IYqeNNh8iGUe\n" \
"xrl3nS1z0iHCnx4AkW1p9OH0/vZ4Kgs2i1YSg2Wxde2TOWS4QhLOoR9yktD+PNNz\n" \
"j8uxGwNXH/mDACQakPVmGl3kHD+j9yzcvKX60mXPQl+QR8mAkS+wlH8tUQ4n9/EX\n" \
"sLfIqkIwFm7+w/WiT4bLxKPP4jKS2odWTXJHdDgCDU7sii8hIvJAcEwQAnaKruYe\n" \
"DpTDZFBDd5TrU7UlHoimpk56m2JHZfzevxYTCzWi6nE6kDy8SYoqDHazsVTuKmSw\n" \
"5NcMs9Pxkl5GQyQIBbrjRqDhynpBXFVQwG5tlptYMhNZyvQjEi1xic/qeA==\n" \
"-----END CERTIFICATE-----\n";

// Uncomment to set to learn mode
//#define MODE_LEARNING 1
#define LOCATION "Kitchen"
#define GROUP_NAME "home"

// Important! BLE + WiFi Support does not fit in standard partition table.
// Manual experimental changes are needed.
// See https://desire.giesecke.tk/index.php/2018/01/30/change-partition-size/

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEBeacon.h>

#define BLE_SCANTIME 6
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

// Not sure if WiFiClientSecure checks the validity date of the certificate. 
// Setting clock just to be sure...
void setClock() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  Serial.print(F("Waiting for NTP time sync: "));
  time_t nowSecs = time(nullptr);
  while (nowSecs < 8 * 3600 * 2) {
    delay(500);
    Serial.print(F("."));
    yield();
    nowSecs = time(nullptr);
  }

  Serial.println();
  struct tm timeinfo;
  gmtime_r(&nowSecs, &timeinfo);
  Serial.print(F("Current time: "));
  Serial.print(asctime(&timeinfo));
}

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
  pBLEScan->setActiveScan(false); // active scan uses more power, but get results faster
  pBLEScan->setInterval(6000); //2000 mseconds
  pBLEScan->setWindow(5000);  // less or equal setInterval value
}


void SubmitWiFi(void)
{
  DynamicJsonDocument jdoc(4096);
  JsonObject root = jdoc.to<JsonObject>();
  root["d"] = chipIdStr;
  root["f"] = GROUP_NAME;
  #ifdef MODE_LEARNING
  root["l"] = LOCATION;
  #endif 
  JsonObject data = root.createNestedObject("s");
  JsonObject bt_network = data.createNestedObject("bluetooth");

  bool hasEddyStone = false;
  DynamicJsonDocument bdoc(2048);
  JsonObject broot = bdoc.to<JsonObject>();
  broot["f"] = GROUP_NAME;
  JsonObject bdata = broot.createNestedObject("b");

  // //Wifi Scan 
  // Serial.println("[ INFO ]\tWiFi scan starting..");
  // int n = WiFi.scanNetworks(false, true);
  // if (n == 0) {
  //   Serial.println("[ ERROR ]\tNo networks found");
  // } else {
  //   Serial.print("[ INFO ]\t");
  //   Serial.print(n);
  //   Serial.println(" WiFi networks found.");
  //   JsonObject wifi_network = data.createNestedObject("wifi");
  //   for (int i = 0; i < n; ++i) {
  //     wifi_network[WiFi.BSSIDstr(i)] = WiFi.RSSI(i);
  //   }
  // }

  //Bluetooth scan
  Serial.println("[ INFO ]\tBLE scan starting..");
  BLEScanResults foundDevices = pBLEScan->start(BLE_SCANTIME);
  int n = foundDevices.getCount();
  if (n == 0) {
    Serial.println("[ ERROR ]\tNo BLE devices found");
    return;
  }
  else {
    Serial.print("[ INFO ]\t");
    Serial.print(n);
    Serial.println(" BLE devices found.");

    for(int i=0; i<n; i++)
    {
      BLEAdvertisedDevice aDevice = foundDevices.getDevice(i);
      std::string mac = aDevice.getAddress().toString();
      bt_network[(String)mac.c_str()] = (int)aDevice.getRSSI();

      if (aDevice.haveServiceData()) {
        // eddy beacon, send namespace, battery to /beacon
        if (aDevice.getServiceDataUUID().getNative()->uuid.uuid16 == 0xfeaa) {
          bdata[(String)mac.c_str()] = aDevice.getServiceData().c_str();
          hasEddyStone = true;
        }
      }
    }
  }
  pBLEScan->clearResults();   // delete results fromBLEScan buffer to release memory

  serializeJsonPretty(jdoc, Serial);  
  
  WiFiClientSecure client;
  client.setCertificate(test_client_cert); // for client verification
  client.setPrivateKey(test_client_key);	// for client verification
  //WiFiClient client;
  const int httpPort = 443;
  Serial.println("\nStarting connection to server...");
  if (!client.connect(host, httpPort)) {
    Serial.println("connection failed");
    return;
  }

  // We now create a URI for the request
  String url = "/passive";
  Serial.print("[ INFO ]\tRequesting URL: ");
  Serial.println(url);
  String request;
  serializeJson(jdoc, request); 

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

  if (hasEddyStone) {
    String request;
    serializeJson(bdoc, request); 
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
        Serial.println("[ ERROR ]\tHTTP Client Timeout at EddyStone !");
        client.stop();
        return;
      }
    }

    // Check HTTP status
    char status[60] = {0};
    client.readBytesUntil('\r', status, sizeof(status));
    if (strcmp(status, "HTTP/1.1 200 OK") != 0) {
      Serial.print(F("[ ERROR ]\tUnexpected Response at EddyStone: "));
      Serial.println(status);
      return;
    }
    else
    {
      Serial.println(F("[ INFO ]\tGot a 200 OK at EddyStone."));
    }

    char endOfHeaders[] = "\r\n\r\n";
    if (!client.find(endOfHeaders)) {
      Serial.println(F("[ ERROR ]\t Invalid Response at EddyStone"));
      return;
    }
    else
    {
      Serial.println("[ INFO ]\tLooks like a valid response at EddyStone.");
    }
  }

  client.stop();
  Serial.println("[ INFO ]\tClosing connection.");
  Serial.println("=============================================================");


   #ifdef USE_DEEPSLEEP
   esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
   esp_deep_sleep_start();
   #endif
  
}

void loop() {
  SubmitWiFi();
  delay(10000); //Delay 
}
