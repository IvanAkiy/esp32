#include "main.h"
#include "pn532.h"

namespace
{
    const char LTAG[] = "Main";
    Main my_main;
}

extern "C" void app_main(void)
{
    ESP_LOGI(LTAG, "Creating default event loop");
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(my_main.setup());
}

Main::Main()
{
}

esp_err_t Main::setup(void)
{
    ESP_LOGI(LTAG, "Setup!");
    xTaskCreate(pn532_example, "pn532_example", 4096, NULL, 5, NULL);
    ESP_LOGI(LTAG, "Setup completed!");
    
    return ESP_OK;
}

void Main::loop(void)
{
    vTaskDelay(pdSECOND);
}
