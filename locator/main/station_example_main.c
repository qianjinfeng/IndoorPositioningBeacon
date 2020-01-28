            
/* WiFi station Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_spiffs.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "os.h"
#include <cJSON.h>
#include "esp_tls.h"
#include "esp_http_client.h"

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"

#include "esp_eddystone_protocol.h"
#include "esp_eddystone_api.h"

#include <math.h>
#include "mqtt_client.h"

#include "esp_efuse.h"
#include "esp_efuse_table.h"

/* The examples use WiFi configuration that you can set via project configuration menu

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;
/* The event group allows multiple bits for each event, but we only care about one event 
 * - are we connected to the AP with an IP? */
const int WIFI_CONNECTED_BIT = BIT0;

/* Root cert for howsmyssl.com, taken from howsmyssl_com_root_cert.pem

   The PEM file was extracted from the output of this command:
   openssl s_client -showcerts -connect www.howsmyssl.com:443 </dev/null

   The CA root cert is the last cert given in the chain of certs.

   To embed it in the app binary, the PEM file is named
   in the component.mk COMPONENT_EMBED_TXTFILES variable.
*/
extern const char root_cert_pem_start[] asm("_binary_root_cert_pem_start");
extern const char root_cert_pem_end[]   asm("_binary_root_cert_pem_end");

static const char *TAG = "wifi station";

static int s_retry_num = 0;
static EventGroupHandle_t mqtt_event_group;
const static int CONNECTED_BIT = BIT0;
static esp_mqtt_client_handle_t mqtt_client = NULL;

char ssid[32] = {0};
char pass[64] = {0};
char c_host[64] = {0};//"mqtt://mqtt:mqtt@139.24.185.184:1883";
char c_location[64] =  {0};//"Test";
char c_ip[32] = { 0 };  
char c_mac[32] = { 0 };  
char c_configtopic[128] = {0};
char c_presencetopic[128] = {0};
char c_presencesubtopic[128] = {0};
char c_roomtopic[128] = {0};

int i_upload = 30;  // 20s scan, 10 s wait
int i_duration = 20; //seconds for bluetooth scan
int i_interval = 40; //in msecs /0.625 = 64= 0x40
int i_window = 30; //in msecs /0.625 = 48 = 0x30

bool g_running = false;

/* declare static functions */
static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param);

static esp_ble_scan_params_t ble_scan_params = {
    .scan_type              = BLE_SCAN_TYPE_PASSIVE,
    .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval          = 0x40,
    .scan_window            = 0x30,
    .scan_duplicate         = BLE_SCAN_DUPLICATE_ENABLE
};

static double calculateDistance(int txPower, int rssi, bool eddy) {
    ESP_LOGI(TAG, "Power and rssi=%d, %d", txPower, rssi);
    if (rssi == 0) {
        return -1.0; // if we cannot determine distance, return -1.
    }
    // if (!txPower) {
    //     // somewhat reasonable default value
    //     txPower = -59;
    // } else if (txPower > 0) {
    // txPower = txPower * -1;
    // }
    double ratio = rssi*1.0/(eddy?txPower-41:txPower);
    if (ratio < 1.0) {
        return pow(ratio,10);
    }
    else {
        double accuracy =  (0.89976)*pow(ratio,7.7095) + 0.111;
        return accuracy;
    }
}
static void bluetooth_init() 
{
    /*<! set scan parameters */
    ble_scan_params.scan_interval = i_interval / 0.625;
    ble_scan_params.scan_window = i_window / 0.625;
    esp_ble_gap_set_scan_params(&ble_scan_params);
}
static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    switch (event->event_id) {
    case MQTT_EVENT_CONNECTED: 
        {
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        xEventGroupSetBits(mqtt_event_group, CONNECTED_BIT);
        
        bluetooth_init();
        g_running = true;

            sprintf(c_configtopic, "homeassistant/binary_sensor/%s/config", c_mac);
            sprintf(c_presencetopic, "homeassistant/binary_sensor/%s/state", c_mac);
            sprintf(c_presencesubtopic, "homeassistant/binary_sensor/%s/state/tele", c_mac);
            cJSON *root = cJSON_CreateObject();
            cJSON_AddStringToObject(root, "name", c_location);
            cJSON_AddStringToObject(root, "device_class", "connectivity");
            cJSON_AddStringToObject(root, "state_topic", c_presencetopic);
            cJSON_AddStringToObject(root, "json_attributes_topic", c_presencesubtopic);
            cJSON_AddStringToObject(root, "unique_id", c_mac);
            cJSON *child = cJSON_CreateObject();
            cJSON_AddStringToObject(child, "identifiers", c_mac);
            cJSON_AddStringToObject(child, "manufacturer", "cpl");
            cJSON_AddStringToObject(child, "name", "ESP32");
            cJSON_AddItemToObject(root, "device", child);
            const char *sys_info = cJSON_Print(root);
            esp_mqtt_client_publish(mqtt_client, c_configtopic, sys_info, strlen(sys_info), 0, 1);

            free((void *)sys_info);
            cJSON_Delete(root);
            esp_mqtt_client_publish(mqtt_client, c_presencetopic, "ON", 0, 0, 1);

            sleep(10);
            root = cJSON_CreateObject();
            cJSON_AddStringToObject(root, "ip", c_ip);
            cJSON_AddStringToObject(root, "mac", c_mac);
            sys_info = cJSON_Print(root);
            esp_mqtt_client_publish(mqtt_client, c_presencesubtopic, sys_info, strlen(sys_info), 0, 1);
            free((void *)sys_info);
            cJSON_Delete(root);
        }
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        g_running = false;
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
    return ESP_OK;
}

static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param)
{
    esp_err_t err;

    switch(event)
    {
        case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
            esp_ble_gap_start_scanning(i_duration);
            ESP_LOGI(TAG,"-----------ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT------ ");
            break;
        }
        case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT: {
            if((err = param->scan_start_cmpl.status) != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(TAG,"Scan start failed: %s", esp_err_to_name(err));
            }
            else {
                ESP_LOGI(TAG,"Start scanning...");
            }
            break;
        }
        case ESP_GAP_BLE_SCAN_RESULT_EVT: {
            esp_ble_gap_cb_param_t* scan_result = (esp_ble_gap_cb_param_t*)param;
            switch(scan_result->scan_rst.search_evt)
            {
                case ESP_GAP_SEARCH_INQ_RES_EVT: {
                    char bAddress[30];
                    sprintf(bAddress, "%02x:%02x:%02x:%02x:%02x:%02x", scan_result->scan_rst.bda[0], 
                                                                       scan_result->scan_rst.bda[1], 
                                                                       scan_result->scan_rst.bda[2], 
                                                                       scan_result->scan_rst.bda[3], 
                                                                       scan_result->scan_rst.bda[4], 
                                                                       scan_result->scan_rst.bda[5]);
                    ESP_LOGI(TAG,"Found %s...%d", bAddress, scan_result->scan_rst.rssi);
                    esp_eddystone_result_t eddystone_res;
                    memset(&eddystone_res, 0, sizeof(eddystone_res));
                    esp_err_t ret = esp_eddystone_decode(scan_result->scan_rst.ble_adv, scan_result->scan_rst.adv_data_len, &eddystone_res);
                    if (ret) {
                        // error:The received data is not an eddystone frame packet or a correct eddystone frame packet.
                    } else {   
                        ESP_LOGI(TAG, "EDDYSTONE_DEMO");
                        // The received adv data is a correct eddystone frame packet.
                        switch(eddystone_res.common.frame_type)
                        {
                            case EDDYSTONE_FRAME_TYPE_URL: {
                                ESP_LOGI(TAG, "URL ");
                                cJSON *root = cJSON_CreateObject();
                                cJSON_AddStringToObject(root, "id", bAddress);
                                double dis = calculateDistance(eddystone_res.inform.url.tx_power, scan_result->scan_rst.rssi, true);
                                cJSON_AddNumberToObject(root, "distance", dis);
                                ESP_LOGI(TAG, "URL distance %f", dis);
                                const char *sys_info = cJSON_Print(root);
                                esp_mqtt_client_publish(mqtt_client, c_roomtopic, sys_info, strlen(sys_info), 0, 0);
                                free((void *)sys_info);
                                cJSON_Delete(root);
                                break;
                            }
                            case EDDYSTONE_FRAME_TYPE_UID: {
                                ESP_LOGI(TAG, "UID ");
                                cJSON *root = cJSON_CreateObject();
                                cJSON_AddStringToObject(root, "id", bAddress);
                                double dis = calculateDistance(eddystone_res.inform.uid.ranging_data, scan_result->scan_rst.rssi, true);
                                cJSON_AddNumberToObject(root, "distance", dis);
                                ESP_LOGI(TAG, "UID distance %f", dis);
                                const char *sys_info = cJSON_Print(root);
                                esp_mqtt_client_publish(mqtt_client, c_roomtopic, sys_info, strlen(sys_info), 0, 0);
                                free((void *)sys_info);
                                cJSON_Delete(root);
                                break;
                            }
                            case EDDYSTONE_FRAME_TYPE_TLM: {
                                ESP_LOGI(TAG, "battery voltage: %d mV", eddystone_res.inform.tlm.battery_voltage);
                                cJSON *root = cJSON_CreateObject();
                                cJSON_AddNumberToObject(root, "battery", eddystone_res.inform.tlm.battery_voltage);
                                const char *sys_info = cJSON_Print(root);
                                char c_batterytopic[128] = {0};
                                sprintf(c_batterytopic, "battery_status/%s", bAddress);
                                esp_mqtt_client_publish(mqtt_client, c_batterytopic, sys_info, strlen(sys_info), 0, 0);
                                free((void *)sys_info);
                                cJSON_Delete(root);
                                break;
                            }
                            default:
                                break;
                        }
                        
                    }


                    break;
                }
                case ESP_GAP_SEARCH_INQ_CMPL_EVT: {
                    ESP_LOGI(TAG, "--------SEARCH_INQ_CMPL----------");
                    sleep(300);
                    if (g_running) {
                        esp_ble_gap_start_scanning(i_duration);
                    }
                }
                default:
                    break;
            }
            break;
        }
        case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:{
            if((err = param->scan_stop_cmpl.status) != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(TAG,"Scan stop failed: %s", esp_err_to_name(err));
            }
            else {
                ESP_LOGI(TAG,"Stop scan successfully");
            }
            break;
        }
        default:
            break;
    }
}
static void mqtt_app_start(void)
{
    mqtt_event_group = xEventGroupCreate();
    const esp_mqtt_client_config_t mqtt_cfg = {
        .event_handle = mqtt_event_handler,
        .lwt_topic = c_presencetopic,
        .lwt_msg = "DISCONNECTED",
        //.cert_pem = (const char *)mqtt_eclipse_org_pem_start,
    };

    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_set_uri(mqtt_client, c_host);
}

static void event_handler(void* arg, esp_event_base_t event_base, 
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        sprintf(c_ip, IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);

        mqtt_app_start();

        xEventGroupClearBits(mqtt_event_group, CONNECTED_BIT);
        esp_mqtt_client_start(mqtt_client);
        ESP_LOGI(TAG, "Note free memory: %d bytes", esp_get_free_heap_size());
        xEventGroupWaitBits(mqtt_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
    }
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t *wifi_config = (wifi_config_t *)os_zalloc(sizeof(wifi_config_t));
    /* Using strncpy allows the max SSID length to be 32 bytes (as per 802.11 standard).
     * But this doesn't guarantee that the saved SSID will be null terminated, because
     * wifi_cfg->sta.ssid is also 32 bytes long (without extra 1 byte for null character) */
    strncpy((char *) wifi_config->sta.ssid, ssid, sizeof(wifi_config->sta.ssid));

    /* Using strlcpy allows both max passphrase length (63 bytes) and ensures null termination
     * because size of wifi_cfg->sta.password is 64 bytes (1 extra byte for null character) */
    strlcpy((char *) wifi_config->sta.password, pass, sizeof(wifi_config->sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );
    os_free(wifi_config);

    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_efuse_read_field_blob(ESP_EFUSE_MAC_FACTORY, &mac, sizeof(mac) * 8));    
    sprintf(c_mac, "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    ESP_LOGI(TAG, "wifi_init_sta finished.");
    ESP_LOGI(TAG, "connect to ap SSID:%s password:%s", ssid, pass);
}

void app_main(void)
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BLE);

    esp_bluedroid_init();
    esp_bluedroid_enable();
    esp_err_t status;
    /*<! register the scan callback function to the gap module */
    if((status = esp_ble_gap_register_callback(esp_gap_cb)) != ESP_OK) {
        ESP_LOGE(TAG,"gap register error: %s", esp_err_to_name(status));
        return;
    }
    ESP_LOGI(TAG, "[APP] Free memory : %d bytes", esp_get_free_heap_size());
    //Initialize SPIFFS
    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = true
    };
    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }
    //Open renamed file for reading
    ESP_LOGI(TAG, "Reading file");
    FILE* fp = fopen("/spiffs/config.txt", "r");
    if (fp != NULL) {
        char buf[64];
        int count = 0;
        while (fgets(buf,sizeof(buf),fp) != NULL) {
            char *p = strchr(buf,'\r');
            if (!p) p = strchr(buf,'\n');
            if (p) *p = '\0';
            switch(count) {
                case 0: 
                    memcpy(ssid, buf, 32);
                    ESP_LOGI(TAG, "ssid %s", ssid);
                    break;
                case 1:
                    memcpy(pass, buf, 64);
                    ESP_LOGI(TAG, "password %s", pass);
                    break;
                case 2:
                    memcpy(c_location, buf, 64);
                    ESP_LOGI(TAG, "location %s", c_location);
                    sprintf(c_roomtopic, "room_presence/%s", c_location);
                    
                    ESP_LOGI(TAG, "room topic %s", c_roomtopic);
                    break;
                case 3:
                    memcpy(c_host, buf, 64);
                    ESP_LOGI(TAG, "host %s", c_host);
                    break;
                default:
                    break;
            }
            count++;
        }
        fclose(fp);
        ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
        wifi_init_sta();
    }
    else {
        ESP_LOGI(TAG, "config.txt not found.");
    }
}




 
