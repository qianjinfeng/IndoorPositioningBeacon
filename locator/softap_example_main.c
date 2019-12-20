/*  WiFi softAP Example

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
#include <sys/param.h>

#include <cJSON.h>
#include "lwip/err.h"
#include "lwip/sys.h"

#include <esp_http_server.h>
#include "esp_tls.h"
#include "esp_http_client.h"

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"

#include "esp_eddystone_protocol.h"
#include "esp_eddystone_api.h"

#define SCRATCH_BUFSIZE (512)

typedef struct rest_server_context {
    char scratch[SCRATCH_BUFSIZE];
} rest_server_context_t;

/* The examples use WiFi configuration that you can set via project configuration menu.

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_MAX_STA_CONN       CONFIG_ESP_MAX_STA_CONN
#define EXAMPLE_ESP_MAXIMUM_RETRY  10
/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about one event 
 * - are we connected to the AP with an IP? */
const int WIFI_CONNECTED_BIT = BIT0;
static int s_retry_num = 0;

static const char *TAG = "wifi softAP and station";
static httpd_handle_t server = NULL;

char g_ipaddress[32] = { 0 };
int g_Mode = 1;  //0 learning; 1 scanning
char c_host[64] = { 0 };
char c_family[64] = { 0 };
char c_location[64] = { 0 };
char c_device[32] = { 0 };
int i_duration = 20; //seconds for bluetooth scan
int i_interval = 40; //in msecs /0.625 = 64= 0x40
int i_window = 30; //in msecs /0.625 = 48 = 0x30
cJSON *bluetooth_json = NULL;
cJSON *battery_json = NULL;
cJSON *asset_json = NULL;
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
                // printf("%.*s", evt->data_len, (char*)evt->data);
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
static esp_http_client_config_t client_config = {
        .event_handler = _http_event_handler,
    };
static void http_rest_post_data(esp_http_client_config_t *aconfig, const char *post_data)
{    
    if (g_running == false || c_host[0] == 0) {
        ESP_LOGD(TAG, "Not running or NO Host configured");
        return;
    }
    esp_http_client_handle_t client = esp_http_client_init(aconfig);
    esp_err_t err;

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

static void post_data()
{
    ESP_LOGI(TAG, "host:%s, family:%s, Device:%s, location:%s", c_host, c_family, c_device, c_location);
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "f", c_family);
    cJSON_AddStringToObject(root, "d", c_device);
    cJSON_AddStringToObject(root, "l", c_location);
    cJSON *sensor_json = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "s", sensor_json);
    cJSON_AddItemToObject(sensor_json, "bluetooth", bluetooth_json);
    
    char c_url[80];
    if (g_Mode) { //scanning 
        sprintf(c_url, "%s/passive", c_host);
    }
    else {  //learning
        sprintf(c_url, "%s/data", c_host);
    }
    client_config.url = c_url;
    const char *sys_info = cJSON_Print(root);
    http_rest_post_data(&client_config, sys_info);
    free((void *)sys_info);
    cJSON_Delete(root);
}
static void post_battery()
{
    ESP_LOGI(TAG, "host:%s, family:%s, Device:%s, location:%s", c_host, c_family, c_device, c_location);
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "f", c_family);
    cJSON_AddItemToObject(root, "b", battery_json);
    
    char c_url[80];
    sprintf(c_url, "%s/battery", c_host);
    client_config.url = c_url;
    const char *sys_info = cJSON_Print(root);
    http_rest_post_data(&client_config, sys_info);
    free((void *)sys_info);
    cJSON_Delete(root);
}
static void post_asset()
{
    ESP_LOGI(TAG, "host:%s, family:%s, Device:%s, location:%s", c_host, c_family, c_device, c_location);
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "f", c_family);
    cJSON_AddItemToObject(root, "a", asset_json);
    
    char c_url[80];
    sprintf(c_url, "%s/asset", c_host);
    client_config.url = c_url;
    const char *sys_info = cJSON_Print(root);
    http_rest_post_data(&client_config, sys_info);
    free((void *)sys_info);
    cJSON_Delete(root);
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
                bluetooth_json = cJSON_CreateObject();
                asset_json = cJSON_CreateObject();
                battery_json = cJSON_CreateObject();
            }
            break;
        }
        case ESP_GAP_BLE_SCAN_RESULT_EVT: {
            esp_ble_gap_cb_param_t* scan_result = (esp_ble_gap_cb_param_t*)param;
            switch(scan_result->scan_rst.search_evt)
            {
                case ESP_GAP_SEARCH_INQ_RES_EVT: {
                    ESP_LOGI(TAG, "-------- Found----------");
                    esp_log_buffer_hex("Bluetooth Device address:", scan_result->scan_rst.bda, ESP_BD_ADDR_LEN);
                    ESP_LOGI(TAG, "RSSI of packet:%d dbm", scan_result->scan_rst.rssi);
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
                        if (scan_result->scan_rst.adv_data_len > 0) {
                            ESP_LOGI(TAG, "adv data:");
                            esp_log_buffer_hex(TAG, &scan_result->scan_rst.ble_adv[0], scan_result->scan_rst.adv_data_len);
                        }
                    } else {   
                        // The received adv data is a correct eddystone frame packet.
                        // Here, we get the eddystone infomation in eddystone_res, we can use the data in res to do other things.
                        // For example, just print them:
                        ESP_LOGI(TAG, "--------Eddystone Found----------");
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
                    post_data();
                    sleep(2);
                    post_asset();
                    sleep(2);
                    post_battery();
                    
                    if (g_running) {
                        sleep(8);
                        esp_ble_gap_start_scanning(i_duration);
                    }
                    else {
                        ESP_LOGI(TAG,"Stop scan");
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


static esp_err_t system_info_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();

    cJSON_AddStringToObject(root, "mac", c_device);
    cJSON_AddStringToObject(root, "ip", g_ipaddress);
    if (g_Mode) {
        cJSON_AddStringToObject(root, "mode", "tracking");
    } else
    {
        cJSON_AddStringToObject(root, "mode", "learning");
    }
    if (g_running) {
        cJSON_AddStringToObject(root, "scanning", "true");
    } else
    {
        cJSON_AddStringToObject(root, "scanning", "false");
    }
    

    const char *sys_info = cJSON_Print(root);
    httpd_resp_sendstr(req, sys_info);
    free((void *)sys_info);
    cJSON_Delete(root);
    return ESP_OK;
}
static esp_err_t system_reboot_handler(httpd_req_t *req)
{
    esp_restart();
}

static esp_err_t set_mode_handler(httpd_req_t *req)
{
    int total_len = req->content_len;
    int cur_len = 0;
    char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
    int received = 0;
    if (total_len >= SCRATCH_BUFSIZE) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }
    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len);
        if (received <= 0) {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post mode value");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    g_Mode = cJSON_GetObjectItem(root, "mode")->valueint;
    ESP_LOGI(TAG, "mode is %d", g_Mode);
    sprintf(c_host, "%s", cJSON_GetObjectItem(root, "host")->valuestring);
    sprintf(c_family, "%s", cJSON_GetObjectItem(root, "family")->valuestring);
    sprintf(c_location, "%s", cJSON_GetObjectItem(root, "location")->valuestring);
    
    cJSON_Delete(root);
    httpd_resp_sendstr(req, "Post mode value successfully");
    return ESP_OK;
}

static esp_err_t set_bluetooth_handler(httpd_req_t *req)
{
    int total_len = req->content_len;
    int cur_len = 0;
    char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
    int received = 0;
    if (total_len >= SCRATCH_BUFSIZE) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }
    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len);
        if (received <= 0) {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post bluetooth value");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    i_duration = cJSON_GetObjectItem(root, "duration")->valueint;
    i_interval = cJSON_GetObjectItem(root, "interval")->valueint;
    i_window = cJSON_GetObjectItem(root, "window")->valueint;
    ESP_LOGI(TAG, "Light control: duration = %d, interval = %d, window = %d", i_duration, i_interval, i_window);
    cJSON_Delete(root);
    httpd_resp_sendstr(req, "Post bluetooth value successfully");
    return ESP_OK;
}
static esp_err_t set_wifi_handler(httpd_req_t *req)
{
    int total_len = req->content_len;
    int cur_len = 0;
    char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
    int received = 0;
    if (total_len >= SCRATCH_BUFSIZE) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }
    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len);
        if (received <= 0) {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post wifi value");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    wifi_config_t wifi_config_sta;
    sprintf((char*)wifi_config_sta.sta.ssid, "%s", cJSON_GetObjectItem(root, "ap")->valuestring);
    sprintf((char*)wifi_config_sta.sta.password, "%s", cJSON_GetObjectItem(root, "password")->valuestring);
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config_sta) );
    esp_wifi_connect();
    cJSON_Delete(root);
    httpd_resp_sendstr(req, "Post wifi value successfully");
    return ESP_OK;
}
void set_bluetooth_parameter()
{
    /*<! set scan parameters */
    ble_scan_params.scan_interval = i_interval / 0.625;
    ble_scan_params.scan_window = i_window / 0.625;
    esp_ble_gap_set_scan_params(&ble_scan_params);
    
}
static esp_err_t user_start_stop_handler(httpd_req_t *req)
{
    int total_len = req->content_len;
    int cur_len = 0;
    char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
    int received = 0;
    if (total_len >= SCRATCH_BUFSIZE) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }
    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len);
        if (received <= 0) {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post wifi value");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    int nValue = cJSON_GetObjectItem(root, "value")->valueint;
    if (nValue) {
        g_running = true;
        set_bluetooth_parameter(); // start scan in callback
        ESP_LOGI(TAG, "running is true");
    }
    else {
        g_running = false;
        ESP_LOGI(TAG, "running is false");
    }
    cJSON_Delete(root);
    httpd_resp_sendstr(req, "Post wifi value successfully");
    return ESP_OK;
}

esp_err_t start_rest_server()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    rest_server_context_t *rest_context = calloc(1, sizeof(rest_server_context_t));
    if (rest_context == NULL ) {
        ESP_LOGI(TAG, "No memory for rest context");
    };

    ESP_LOGI(TAG, "Starting HTTP Server");
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGI(TAG, "Error starting server!");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);

    /* URI handler for fetching system info */
    httpd_uri_t system_info_get_uri = {
        .uri = "/api/info",
        .method = HTTP_GET,
        .handler = system_info_get_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &system_info_get_uri);

    httpd_uri_t system_reboot_uri = {
        .uri = "/api/reboot",
        .method = HTTP_GET,
        .handler = system_reboot_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &system_reboot_uri);

    /* URI handler for setting wifi */
    httpd_uri_t user_set_wifi_uri = {
        .uri = "/api/wifi",
        .method = HTTP_POST,
        .handler = set_wifi_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &user_set_wifi_uri);

    /* URI handler for setting bluetooth */
    httpd_uri_t user_set_bluetooth_uri = {
        .uri = "/api/bluetooth",
        .method = HTTP_POST,
        .handler = set_bluetooth_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &user_set_bluetooth_uri);

    /* URI handler for set learn or scan */
    httpd_uri_t user_set_mode_uri = {
        .uri = "/api/mode",
        .method = HTTP_POST,
        .handler = set_mode_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &user_set_mode_uri);

    /* URI handler for start learn or scan */
    httpd_uri_t user_start_stop_uri = {
        .uri = "/api/start",
        .method = HTTP_POST,
        .handler = user_start_stop_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &user_start_stop_uri);

    return ESP_OK; 
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
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
        sprintf(g_ipaddress, IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_softap(void)
{
    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_efuse_read_field_blob(ESP_EFUSE_MAC_FACTORY, &mac, sizeof(mac) * 8));
    sprintf(c_device, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    s_wifi_event_group = xEventGroupCreate();

    esp_netif_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    // ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config_sta) );
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);


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

    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
    wifi_init_softap();

    /* Start the server for the first time */
    start_rest_server();

    esp_bluedroid_init();
    esp_bluedroid_enable();
    esp_err_t status;
    /*<! register the scan callback function to the gap module */
    if((status = esp_ble_gap_register_callback(esp_gap_cb)) != ESP_OK) {
        ESP_LOGE(TAG,"gap register error: %s", esp_err_to_name(status));
        return;
    }
}
