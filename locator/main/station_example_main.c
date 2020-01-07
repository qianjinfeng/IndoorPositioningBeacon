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
#include "esp_efuse.h"
#include "esp_efuse_table.h"
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
char ssid[32];
char pass[64];
char c_host[64] = { 0 };
char c_family[64] = { 0 };
char c_location[64] = { 0 };
char c_mode[32] ={0};
char c_device[128] = { 0 };  //location+ip
char c_url[80];

int i_upload = 30;  // 20s scan, 10 s wait
int i_duration = 20; //seconds for bluetooth scan
int i_interval = 40; //in msecs /0.625 = 64= 0x40
int i_window = 30; //in msecs /0.625 = 48 = 0x30
cJSON *bluetooth_json = NULL;
cJSON *battery_json = NULL;
cJSON *asset_json = NULL;

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


esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // Write out data
                printf("%.*s", evt->data_len, (char*)evt->data);
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            int mbedtls_err = 0;
            esp_err_t err = esp_tls_get_and_clear_last_error(evt->data, &mbedtls_err, NULL);
            if (err != 0) {
                ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
                ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
            }
            break;
    }
    return ESP_OK;
}

static void post_data()
{
    ESP_LOGI(TAG, "[APP] Free memory before post: %d bytes", esp_get_free_heap_size());
     esp_http_client_config_t client_config = {
        .event_handler = _http_event_handler,
        .cert_pem = root_cert_pem_start,
    };
    sprintf(c_url, "%s/passive", c_host);
    client_config.url = c_url;

    esp_http_client_handle_t client = esp_http_client_init(&client_config);
    esp_err_t err;

    ESP_LOGI(TAG, "host:%s, family:%s, Device:%s, location:%s", c_host, c_family, c_device, c_location);
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "f", c_family);
    cJSON_AddStringToObject(root, "d", c_device);
    cJSON_AddStringToObject(root, "l", c_location);
    cJSON *sensor_json = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "s", sensor_json);
    cJSON_AddItemToObject(sensor_json, "bluetooth", bluetooth_json);
    char *sys_info = cJSON_Print(root);
    // ESP_LOGI(TAG,"Bluetooth...%s", sys_info);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_post_field(client, sys_info, strlen(sys_info));
    err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }
    free((void *)sys_info);
    cJSON_Delete(root);
    sys_info = NULL;
    root = NULL;

    root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "f", c_family);
    cJSON_AddItemToObject(root, "b", battery_json);
    sys_info = cJSON_Print(root);
    // ESP_LOGI(TAG,"Battery...%s", sys_info);
    sprintf(c_url, "%s/battery", c_host);
    esp_http_client_set_url(client, c_url);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_post_field(client, sys_info, strlen(sys_info));
    err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }    
    free((void *)sys_info);
    cJSON_Delete(root);
    sys_info = NULL;
    root = NULL;

    root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "f", c_family);
    cJSON_AddItemToObject(root, "a", asset_json);
    sys_info = cJSON_Print(root);
    // ESP_LOGI(TAG,"Asset...%s", sys_info);
    sprintf(c_url, "%s/asset", c_host);
    esp_http_client_set_url(client, c_url);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_post_field(client, sys_info, strlen(sys_info));
    err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }    
    free((void *)sys_info);
    cJSON_Delete(root);
    sys_info = NULL;
    root = NULL;

    esp_http_client_cleanup(client);
    ESP_LOGI(TAG, "[APP] Free memory after post: %d bytes", esp_get_free_heap_size());

    sleep((i_upload-i_duration));
    esp_ble_gap_start_scanning(i_duration);
    vTaskDelete(NULL);
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
                ESP_LOGI(TAG, "[APP] Free memory before create: %d bytes", esp_get_free_heap_size());
                bluetooth_json = cJSON_CreateObject();
                asset_json = cJSON_CreateObject();
                battery_json = cJSON_CreateObject();
                //ESP_LOGI(TAG, "[APP] Free memory after create: %d bytes", esp_get_free_heap_size());
            }
            break;
        }
        case ESP_GAP_BLE_SCAN_RESULT_EVT: {
            esp_ble_gap_cb_param_t* scan_result = (esp_ble_gap_cb_param_t*)param;
            switch(scan_result->scan_rst.search_evt)
            {
                case ESP_GAP_SEARCH_INQ_RES_EVT: {
                    //ESP_LOGI(TAG, "RSSI of packet:%d dbm", scan_result->scan_rst.rssi);
                    // if (scan_result->scan_rst.rssi < -85) {
                    //     ESP_LOGI(TAG, "continue");
                    //     break;
                    // }
                    //ESP_LOGI(TAG, "-------- Found----------");
                    //esp_log_buffer_hex("Bluetooth Device address:", scan_result->scan_rst.bda, ESP_BD_ADDR_LEN);
                    
                    char bAddress[30];
                    sprintf(bAddress, "%02x:%02x:%02x:%02x:%02x:%02x", scan_result->scan_rst.bda[0], 
                                                                       scan_result->scan_rst.bda[1], 
                                                                       scan_result->scan_rst.bda[2], 
                                                                       scan_result->scan_rst.bda[3], 
                                                                       scan_result->scan_rst.bda[4], 
                                                                       scan_result->scan_rst.bda[5]);
                    cJSON_AddNumberToObject(bluetooth_json, bAddress, scan_result->scan_rst.rssi);

                    esp_eddystone_result_t eddystone_res;
                    memset(&eddystone_res, 0, sizeof(eddystone_res));
                    esp_err_t ret = esp_eddystone_decode(scan_result->scan_rst.ble_adv, scan_result->scan_rst.adv_data_len, &eddystone_res);
                    if (ret) {
                        // error:The received data is not an eddystone frame packet or a correct eddystone frame packet.
                        // if (scan_result->scan_rst.adv_data_len > 0) {
                        //     ESP_LOGI(TAG, "adv data:");
                        //     esp_log_buffer_hex(TAG, &scan_result->scan_rst.ble_adv[0], scan_result->scan_rst.adv_data_len);
                        // }
                    } else {   
                        // The received adv data is a correct eddystone frame packet.
                        // Here, we get the eddystone infomation in eddystone_res, we can use the data in res to do other things.
                        // For example, just print them:
                        //ESP_LOGI(TAG, "--------Eddystone Found----------");
                        //esp_eddystone_show_inform(&eddystone_res);
                        switch(eddystone_res.common.frame_type)
                        {
                            case EDDYSTONE_FRAME_TYPE_UID: {
                                ESP_LOGI(TAG, "EDDYSTONE_DEMO: Instance ID:0x");
                                char aInstance[30];
                                sprintf(aInstance, "%02x%02x%02x%02x%02x%02x", eddystone_res.inform.uid.instance_id[0], 
                                                                       eddystone_res.inform.uid.instance_id[1], 
                                                                       eddystone_res.inform.uid.instance_id[2], 
                                                                       eddystone_res.inform.uid.instance_id[3], 
                                                                       eddystone_res.inform.uid.instance_id[4], 
                                                                       eddystone_res.inform.uid.instance_id[5]);
                                cJSON_AddStringToObject(asset_json, bAddress, aInstance);
                                esp_log_buffer_hex(TAG, eddystone_res.inform.uid.instance_id, 6);
                                break;
                            }
                            case EDDYSTONE_FRAME_TYPE_TLM: {
                                ESP_LOGI(TAG, "battery voltage: %d mV", eddystone_res.inform.tlm.battery_voltage);
                                cJSON_AddNumberToObject(battery_json, bAddress, eddystone_res.inform.tlm.battery_voltage);
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
                    //Stack error might due to limite main task's stack size; Thus create another task for post data
                    if (xTaskCreate(post_data, "post_data_task", 16384, NULL, 5, NULL) != pdTRUE) {
                        ESP_LOGE(TAG, "Error create mqtt task");
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

static void bluetooth_init() 
{
    /*<! set scan parameters */
    ble_scan_params.scan_interval = i_interval / 0.625;
    ble_scan_params.scan_window = i_window / 0.625;
    esp_ble_gap_set_scan_params(&ble_scan_params);
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
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);

        sprintf(c_device, "%s-" IPSTR, c_location, IP2STR(&event->ip_info.ip));
        bluetooth_init();
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

    ESP_LOGI(TAG, "wifi_init_sta finished.");
    ESP_LOGI(TAG, "connect to ap SSID:%s password:%s",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);

    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_efuse_read_field_blob(ESP_EFUSE_MAC_FACTORY, &mac, sizeof(mac) * 8));
    sprintf(c_device, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
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
    // Open renamed file for reading
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
                    memcpy(c_family, buf, 64);
                    break;
                case 3:
                    memcpy(c_location, buf, 64);
                    break;
                case 4:
                    memcpy(c_host, buf, 64);
                    ESP_LOGI(TAG, "host %s", c_host);
                    break;
                case 5:
                    memcpy(c_mode, buf, 32);
                    ESP_LOGI(TAG, "mode %s", c_mode);
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
