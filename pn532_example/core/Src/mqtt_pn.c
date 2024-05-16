#include "mqtt_pn.h"

const char MQTT_TAG[] = "MQTT";

const char *ssid = "rpi-qr-rfid";
const char *pass = "rpi-qr-rfid";

static esp_mqtt_client_handle_t client;

void mqtt_publish_message(const char *topic, const char *message)
{
    int result = esp_mqtt_client_publish(client, topic, message, 0, 1, 0);

    if (result < 0)
    {
        ESP_LOGE(MQTT_TAG, "Error occured on sending message via MQTT!");
    }
    else
    {
        ESP_LOGI(MQTT_TAG, "Message sent successfully!");
    }
}

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0)
    {
        ESP_LOGE(MQTT_TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(MQTT_TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);

    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;

    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_publish(client, "topic", "Connected to MQTT!", 0, 1, 0);
        ESP_LOGI(MQTT_TAG, "Sent successfully, msg_id=%d", msg_id);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
        ESP_LOGI(MQTT_TAG, "Sent successfully, msg_id=%d", msg_id);
        break;

    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
        {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno", event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(MQTT_TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;

    default:
        ESP_LOGI(MQTT_TAG, "Other event id:%d", event->event_id);
        break;
    }
}

static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_STA_START)
    {
        ESP_LOGI(MQTT_TAG, "WiFi CONNECTING...");
    }
    else if (event_id == WIFI_EVENT_STA_CONNECTED)
    {
        ESP_LOGI(MQTT_TAG, "WiFi CONNECTED!");
    }
    else if (event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGI(MQTT_TAG, "WiFi lost connection(");
        esp_wifi_connect();
        printf("Retrying to Connect...");
    }
    else if (event_id == IP_EVENT_STA_GOT_IP)
    {
        ESP_LOGI(MQTT_TAG, "Wifi got an IP!");
    }
}

void wifi_connection()
{
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t wifi_initiation = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_initiation);

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);

    wifi_config_t wifi_configuration = {
        .sta = {
            .ssid = "",
            .password = "",

        }

    };
    strcpy((char *)wifi_configuration.sta.ssid, ssid);
    strcpy((char *)wifi_configuration.sta.password, pass);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_configuration);

    esp_wifi_start();
    esp_wifi_set_mode(WIFI_MODE_STA);

    esp_wifi_connect();
    ESP_LOGI(MQTT_TAG, "WiFi connected. SSID:%s  Password:%s", ssid, pass);
}

void mqtt_initialize(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address = {
                .uri = "mqtt://192.168.4.1",
            },
            .verification = {
                .skip_cert_common_name_check = true,
            },
        },
        .network = {
            .reconnect_timeout_ms = 250,
        },
        .session = {
            .last_will = {
                .topic = "topic",
                .msg = "My last transmission. Farewell, let our connections endure...",
                .msg_len = 61,
                .qos = 1,
            },
            .keepalive = INT_MAX,
        },
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
}
