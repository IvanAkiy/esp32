#include "mqtt_pn.h"

static const char MAIN_TAG[] = "Main";

void mqtt_publish_task(void *pvParameters) {
    while (1) {
        // Generate the message
        char message[50];
        static int i = 0;
        sprintf(message, "Message from task! %d", i);
        
        // Publish the message using mqtt_publish_message function
        mqtt_publish_message("topic", message);
        
        i++;
        vTaskDelay(5000 / portTICK_PERIOD_MS); // Delay for 5 seconds
    }
}

void app_main(void)
{
    ESP_LOGI(MAIN_TAG, "Process started!");
    nvs_flash_init(); 
    wifi_connection(); 
    vTaskDelay(10000 /portTICK_PERIOD_MS); 
    mqtt_initialize();

     xTaskCreate(mqtt_publish_task, "mqtt_publish_task", 4096, NULL, 5, NULL);
}











// #include "main.h"

// static const char MAIN_TAG[] = "Main";

// void app_main(void)
// {
//     ESP_LOGI(MAIN_TAG, "Setup!");
//     xTaskCreate(pn532_example, "pn532_example", 4096, NULL, 5, NULL);
//     ESP_LOGI(MAIN_TAG, "Setup completed!");
// }
