#include <pn532.h>


static const char MAIN_TAG[] = "Main";

void app_main(void)
{
    init();
    ESP_LOGI(MAIN_TAG, "Setup!");
    nvs_flash_init(); 
    
    wifi_connection(); 
    vTaskDelay(10000 /portTICK_PERIOD_MS); 
    
    mqtt_initialize();
    ESP_LOGI(MAIN_TAG, "Setup completed!");

    xTaskCreate(pn532_example, "pn532_example", 4096, NULL, 5, NULL);
}