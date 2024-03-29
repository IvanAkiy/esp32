#include "mqtt_client.h" 
#include "esp_event.h" 
#include "nvs_flash.h" 
#include "freertos/FreeRTOS.h"
#include "freertos/task.h" 
#include "esp_log.h"
#include <string.h>
#include "esp_system.h" 
#include "esp_wifi.h" 
#include "nvs_flash.h" 
#include "lwip/err.h" 
#include "lwip/sys.h"
#include <stdio.h>
#include "mqtt_client.h"

const char *ssid = "rpi-qr-rfid";
const char *pass = "rpi-qr-rfid";
int retry_num=0;
const char LTAG[] = "Main";

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(LTAG, "Last error %s: 0x%x", message, error_code);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(LTAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
            ESP_LOGI(LTAG, "MQTT_EVENT_CONNECTED");
            // Publish a message to the "topic" after successfully connecting
            msg_id = esp_mqtt_client_publish(client, "topic", "Hello MQTT!", 0, 1, 0);
            ESP_LOGI(LTAG, "Sent publish successful, msg_id=%d", msg_id);
            break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(LTAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(LTAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
        ESP_LOGI(LTAG, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(LTAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(LTAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);   
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(LTAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(LTAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(LTAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

        }
        break;
    default:
        ESP_LOGI(LTAG, "Other event id:%d", event->event_id);
        break;
    }
}

    static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id,void *event_data){
    if(event_id == WIFI_EVENT_STA_START)
    {
    ESP_LOGI(LTAG, "WiFi CONNECTING....");
    }
    else if (event_id == WIFI_EVENT_STA_CONNECTED)
    {
    ESP_LOGI(LTAG,"WiFi CONNECTED\n");
    }
    else if (event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
    ESP_LOGI(LTAG,"WiFi lost connection\n");
    if(retry_num<5){esp_wifi_connect();retry_num++;printf("Retrying to Connect...\n");}
    }
    else if (event_id == IP_EVENT_STA_GOT_IP)
    {
    ESP_LOGI(LTAG,"Wifi got IP...\n\n");
    }
}


void wifi_connection()
{
    // 2 - Wi-Fi Configuration Phase
    esp_netif_init();
    esp_event_loop_create_default();     // event loop                    s1.2
    esp_netif_create_default_wifi_sta(); // WiFi station                      s1.3
    wifi_init_config_t wifi_initiation = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_initiation); //     
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);
    wifi_config_t wifi_configuration = {
        .sta = {
            .ssid = "",
            .password = "",
            
           }
    
        };
    strcpy((char*)wifi_configuration.sta.ssid, ssid);
    strcpy((char*)wifi_configuration.sta.password, pass);    
    //esp_log_write(ESP_LOG_INFO, "Kconfig", "SSID=%s, PASS=%s", ssid, pass);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_configuration);
    // 3 - Wi-Fi Start Phase
    esp_wifi_start();
    esp_wifi_set_mode(WIFI_MODE_STA);
    // 4- Wi-Fi Connect Phase
    esp_wifi_connect();
    ESP_LOGI(LTAG, "wifi_init_softap finished. SSID:%s  password:%s",ssid,pass);
    
}

static void mqtt_initialize(void)
{
    // Set up MQTT broker address and security verification
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address = {
                .uri = "mqtt://172.24.193.28", // TODO write for hostname
            },
            .verification = {
                .skip_cert_common_name_check = true, // Set to true if you want to skip CN check
            },
        },
        .session = {
            .keepalive = 60, // Specify the keep-alive time in seconds
        },
        .network = {
            .reconnect_timeout_ms = 5000, // Specify the reconnect timeout in milliseconds
            .timeout_ms = 10000,          // Specify the network operation timeout in milliseconds
        },
        .task = {
            .priority = 5,     // Specify the MQTT task priority
            .stack_size = 8192 // Specify the MQTT task stack size
        },
        .buffer = {
            .size = 1024,  // Specify the MQTT send/receive buffer size
            .out_size = 1024 // Specify the MQTT output buffer size (if needed)
        },
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
}


void app_main(void)
{
    nvs_flash_init(); 
    wifi_connection(); 
    vTaskDelay(10000 /portTICK_PERIOD_MS); 
    mqtt_initialize();
}











// #include "main.h"

// static const char LTAG[] = "Main";

// void app_main(void)
// {
//     ESP_LOGI(LTAG, "Setup!");
//     xTaskCreate(pn532_example, "pn532_example", 4096, NULL, 5, NULL);
//     ESP_LOGI(LTAG, "Setup completed!");
// }
