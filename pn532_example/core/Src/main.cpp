#include "main.h"

#define pdHALF_SECOND pdMS_TO_TICKS(500)
#define pdSECOND pdMS_TO_TICKS(1000)

namespace
{
    const char LTAG[] = "PN532";
    Main my_main;
}

#define I2C_MASTER_SCL_IO 18
#define I2C_MASTER_SDA_IO 17
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 2000
#define I2C_MASTER_TX_BUF_DISABLE 0
#define I2C_MASTER_RX_BUF_DISABLE 0

#define PN532_I2C_ADDRESS (0x48 >> 1)

static esp_err_t i2c_master_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master = {
            .clk_speed = I2C_MASTER_FREQ_HZ,
        },
        .clk_flags = 0,
    };

    i2c_param_config(I2C_MASTER_NUM, &conf);

    return i2c_driver_install(I2C_MASTER_NUM,
                conf.mode, I2C_MASTER_RX_BUF_DISABLE,
                I2C_MASTER_TX_BUF_DISABLE, 0);
}

void pn532_example(void*) {
    i2c_master_init();

    esp_err_t result;
    
    uint8_t wake_up_command[] = {0x55, 0x55, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x03, 0xFD, 0xD4, 0x14, 0x01, 0x17, 0x00};
    result = i2c_master_write_to_device(I2C_MASTER_NUM, PN532_I2C_ADDRESS, wake_up_command, sizeof(wake_up_command), pdSECOND*10);
    if (result == ESP_OK) {
        ESP_LOGI(LTAG, "PN532 Wake Up Successfully!");
    } else {
        ESP_LOGE(LTAG, "Error waking up: %s", esp_err_to_name(result));
        return;
    }

    uint8_t rf_config_command[] = {0x00, 0x00, 0xFF, 0x06, 0xFA, 0xD4, 0x32, 0x05, 0xFF, 0xFF, 0x02, 0xF5, 0x00};
    result = i2c_master_write_to_device(I2C_MASTER_NUM, PN532_I2C_ADDRESS, rf_config_command, sizeof(rf_config_command), pdSECOND*10);
    if (result == ESP_OK) {
        ESP_LOGI(LTAG, "RF Config Command Sent Successfully!");
    } else {
        ESP_LOGE(LTAG, "Error sending RF Config command: %s", esp_err_to_name(result));
        return;
    }

    uint8_t in_list_passive_target_command[] = {0x00, 0x00, 0xFF, 0x04, 0xFC, 0xD4, 0x4A, 0x01, 0x00, 0xE1, 0x00};
    result = i2c_master_write_to_device(I2C_MASTER_NUM, PN532_I2C_ADDRESS, in_list_passive_target_command, sizeof(in_list_passive_target_command), pdSECOND*10);
    if (result == ESP_OK) {
        ESP_LOGI(LTAG, "In List Passive Target Command Sent Successfully!");
    } else {
        ESP_LOGE(LTAG, "Error sending In List Passive Target command: %s", esp_err_to_name(result));
        return;
    }
    
    while (1) {
        uint8_t uid[20];
        result = i2c_master_read_from_device(I2C_MASTER_NUM, PN532_I2C_ADDRESS, uid, sizeof(uid), pdSECOND);
        
        if (result == ESP_OK) {
            uint8_t card_detected_uid[] = {0x01, 0x00, 0x00, 0xFF, 0x0C, 0xF4, 0xD5, 0x4B, 0x01};
            uint8_t card_released_uid[] = {0x01, 0x00, 0x00, 0xFF, 0x0C, 0xF4, 0xD5, 0x4B, 0x00};
            
            bool detected_uid_match = true, release_uid_match = true;
            for (int i = 0; i < 8; i++) {
                if (uid[i] != card_detected_uid[i]) {
                    detected_uid_match = false;
                }
                else if (uid[i] == card_released_uid[i]) {
                    release_uid_match = false;
                }

                if (detected_uid_match == false && !release_uid_match == false) {
                    break;
                }
            }
            
            if (detected_uid_match) {
                ESP_LOGI(LTAG, "Card is detected!");
                printf("Received UID: ");
                for (int i = 6; i < sizeof(uid); i++) {
                    printf("%02X ", uid[i]);
                }
                printf("\n");
                
                result = i2c_master_write_to_device(I2C_MASTER_NUM, PN532_I2C_ADDRESS, in_list_passive_target_command, sizeof(in_list_passive_target_command), pdSECOND);
            } 
            else if (release_uid_match) {
                ESP_LOGI(LTAG, "Card released!");
                result = i2c_master_write_to_device(I2C_MASTER_NUM, PN532_I2C_ADDRESS, in_list_passive_target_command, sizeof(in_list_passive_target_command), pdSECOND);
            }
        } else {
            printf("Error reading UID\n");
        }

        vTaskDelay(pdHALF_SECOND);
    }
   
}


extern "C" void app_main(void)
{
    ESP_LOGI(LTAG, "Creating default event loop");
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(my_main.setup());

    while (true)
    {
        my_main.loop();
    }
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
