#ifndef MQTT_PN_H
#define MQTT_PN_H

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h" 
#include "esp_wifi.h"
#include "nvs_flash.h" 
#include "esp_log.h"
#include "esp_system.h" 
#include "lwip/err.h" 
#include "lwip/sys.h"
#include "mqtt_client.h" 

void wifi_connection(void);
void mqtt_initialize(void);
void mqtt_publish_message(const char *topic, const char *message);

#endif // MQTT_PN_H